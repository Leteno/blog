# weak_ptr

> 弱引用到已经被 shared_ptr 管理的对象，可以通过 shared_ptr(weak_ptr) 或是 weak_ptr::lock 的方式获取 shared_ptr。
>
> 如果 shared_ptr 管理的对象已经被销毁，使用 shared_ptr(weak_ptr) 会报错：boost::bad_weak_ptr
>
> 此时使用 weak_ptr.lock 会返回一个空的  shared_ptr

```c++
// example
shared_ptr<int> p(new int(5));
weak_ptr<int> q(p);

// some time later
if(shared_ptr<int> r = q.lock())
{
    // use *r
}
```

注： shared_ptr operator bool() 值为：`px != 0`; 定义在 operator_bool.hpp，被 shared_ptr.hpp 引用。

与 shared_ptr 不同，weak_ptr 没有 `get` 方法，因为 weak_ptr 不能决定指针的生命周期，get 获取到的指针什么时候销毁，不清楚，拿着一个生命周期不清楚的指针，任何操作都是提心吊胆，生死未卜。

weak_ptr 的结构是:

```c++
template<class T>
class weak_ptr {
	element_type *px;
	boost::detail::weak_count pn;
};
```

基本功能依赖 shared_ptr，指针 px 只能通过 shared_ptr 传入，不过跟 shared_ptr 一样，是 copyable 的。

俩者最明显的区别是 pn 的类型，一个是 shared_count, 一个是 weak_count, 其定义也在 [shared_count.hpp](https://github.com/boostorg/smart_ptr/blob/5dd84ea389d8fb1b88a06303c866c11577a5594b/include/boost/smart_ptr/detail/shared_count.hpp)  中。数据结构一致：

```c++
class weak_count {
	sp_counted_base * pi_;
};
```

pi_, 跟指针 px 一样，也是抄 shared_ptr 里面的（shared_count）。

构造和析构时调用的方法是 `pi_->weak_add_ref()`, `pi_->weak_release() `(还记得 shared_count 调用的是什么吗？)

weak ref count 到了终点会发生什么呢？竟然是 `sp_counted_base::destroy` 方法。而方法这么牛逼：

```c++
virtual void destroy() // nothrow
{
	delete this; // this 为 sp_counted_base
}
```

太猛了，也就是 weak_ptr 狠起来，可是会销毁 pi_ 指针，届时 shared_ptr 你也别想用。看起来太危险了，是有什么限制条件我没看到吗？

卧槽，初始值和结束值跟 shared_ptr 一样，也是  1

 debug 了一下，原来初始值是 1，结束值是 0

```c++
void weak_release() { 
    if( atomic_decrement( &weak_count_ ) == 1 ) { // 等价于 weak_count_-- == 1
        destroy();
    }   
}
```

而 pi_ 的生死大权一直掌握在 shared_ptr 手中：

```c++
void release() // nothrow
{
    std::cout << "calling release on this: " << this << " weak_count " << weak_count_  << std::endl;
    if( atomic_decrement( &use_count_ ) == 1 )
    {
        dispose();
        weak_release();
    }
}
```

临近 dispose() 才给 weak_release 最后一击，最终执行 destroy()。 卧槽，过瘾。

那初始值给 1 只有一种可能性，那就是 shared_ptr 第一次初始化，并没有对 use_count_ ++, 也就是方法：`add_ref_copy`,

很棒，`add_ref_copy` 调用位置只有俩个：

* shared_count 的拷贝构造函数
* shared_count 的拷贝赋值运算符

默认构造函数不搞事情 ！