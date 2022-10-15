# PostTask and  WeakPtr

Chromium 里面，当我们想把一个 task 异步执行，我们会使用 `ThreadPool::PostTask`.

task 经常是用 Bind 方法绑定一个方法，外加方法所需参数，比如：

```
base::ThreadPool::PostTask(
  FROM_HERE,
  base::BindOnce(&Student::Say, base::Unretained(this), "Hello world")
);
```

注意此处的 `base::Unretained(this)`, 本质上还是 `this` 裸指针。裸指针的异步使用，有可能会导致 UAF 漏洞。

Chromium 对此的解决方案是通过 WeakPtr 包裹一下，传递的是 WeakPtr，比如这样：

```
class Student {
...
base::ThreadPool::PostTask(
  FROM_HERE,
  base::BindOnce(&Student::Say, weak_factory_.GetWeakPtr(), "Hello world")
);
...

private:
  base::WeakPtrFactory<Student> weak_factory_{this};
};

```

这样，当 callback 最终要被调用之前，会检查一下 `Student` 对象是否已经被销毁了，如果是，则不继续执行。

本文将简述这部分判断逻辑，具体 code 在哪里。

## callback.h

我们从 Callback::Run 入手。（相信大家已经见过 `std::move(callback).Run()` 无数次了）

执行的 code 是这个：

```
  using PolymorphicInvoke = R (*)(internal::BindStateBase*,
                                  internal::PassingType<Args>...);
  R Run(Args... args) const & {
    PolymorphicInvoke f =
        reinterpret_cast<PolymorphicInvoke>(this->polymorphic_invoke());
    return f(this->bind_state_.get(), std::forward<Args>(args)...);
  }
```

而这里面的 `f` 背后是 `Invoker::RunOnce` 或者 `Invoker::Run` ：

// base/functional/bind_internal.h

```
template <template <typename> class CallbackT,
          typename Functor,
          typename... Args>
decltype(auto) BindImpl(Functor&& functor, Args&&... args) {
  ...
  if constexpr (kIsOnce) {
    invoke_func = Invoker::RunOnce;
  } else {
    invoke_func = Invoker::Run;
  }
  ...
  return CallbackType(BindState::Create(
      reinterpret_cast<InvokeFuncStorage>(invoke_func),
      std::forward<Functor>(functor), std::forward<Args>(args)...));
}
```

RunOnce 最终调用了 RunImpl，而 RunImpl 里面就做了一次 weak_call 判断 :)

```
template <typename Functor, typename BoundArgsTuple, size_t... indices>
  static inline R RunImpl(Functor&& functor,
                          BoundArgsTuple&& bound,
                          std::index_sequence<indices...> seq,
                          UnboundArgs&&... unbound_args) {
    ...
    static constexpr bool is_weak_call =
        IsWeakMethod<is_method,
                     std::tuple_element_t<indices, DecayedArgsTuple>...>();
    ...
    
    return InvokeHelper<is_weak_call, R, indices...>::MakeItSo(
        std::forward<Functor>(functor), std::forward<BoundArgsTuple>(bound),
        std::forward<UnboundArgs>(unbound_args)...);
}
```

有意思的 is_weak_call, 看看怎么判断的：

```
template <typename T, typename... Args>
struct IsWeakMethod<true, T, Args...> : IsWeakReceiver<T> {};

template <typename T>
struct IsWeakReceiver : std::false_type {};

template <typename T>
struct IsWeakReceiver<WeakPtr<T>> : std::true_type {};
```

所以也就是 如果是 WeakPtr 的话，is_weak_call 为 true，其余为 false。

我们再看看最终 InvokeHelper 对于 is_weak_call true 的 case 是怎么处理的：

// base/functional/bind_internal.h

定义了俩套：

```
template <typename ReturnType, size_t... indices>
struct InvokeHelper<false, ReturnType, indices...> {
  ...
}


template <typename ReturnType, size_t... indices>
struct InvokeHelper<true, ReturnType, indices...> {
  ...
}
```

true 比 false 多了一些检查：

```
static inline void MakeItSo(Functor&& functor,
                            BoundArgsTuple&& bound,
                            RunArgs&&... args) {
  // check start
  if (!std::get<0>(bound))
      return;                       // Line 01
  // check end
  using Traits = MakeFunctorTraits<Functor>;
  Traits::Invoke(
      std::forward<Functor>(functor),
      Unwrap(std::get<indices>(std::forward<BoundArgsTuple>(bound)))...,
      std::forward<RunArgs>(args)...);
}
```

调用的是啥呢？普通指针类型判断其是否为空 `!ptr`, 而 WeakPtr 的实现 `operator bool()`则是判断其 ref 的值是否被销毁：

```
explicit operator bool() const { return get() != nullptr; }

T* get() const {
  return ref_.IsValid() ? reinterpret_cast<T*>(ptr_) : nullptr;
}
```

所以 如果这个对象被销毁，函数会在 `Line 01` 结束，所以 "Callback" 不会被执行，这也避免了 UAF 的发生。

Interesting journey.