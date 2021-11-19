# scope_ptr  shared_ptr 以及 weak_ptr

参考 [boost/smart_ptr](https://github.com/boostorg/smart_ptr) 源码，本文会讨论这三者的异同，关键字有：

* RAII 与 安全
* Copyable 和 RefCount
* shared_ptr 和 weak_ptr

## RAII 与 安全

全称 *Resource Acquisition Is Initialization*

常见的 C++ 技巧，是将资源的生命周期绑定在对象的生命周期中，在对象作用域消失时，回收资源。

比如：

```c++
class Ptr {
private:
    int* pInt;
public:
    Ptr(int* i) : pInt(i) {}
    ~Ptr() { delete pInt; }
};
void helloworld() {
    int* data = new int(1234);
    Ptr guard(data);
    
    // doSomethingOn(data);

    // delete data; // 无需 delete，交由 guard 处理
}
```

可以看到，当 helloworlld 函数结束，局部变量 guard 退出作用域，调用析构函数 ~Ptr(), 而 data 则被 delete 回收。干净利索。

而且还安全：

如果用老方法，在函数末尾 delete，想像一个场景： 

`doSomethingOn(data)` 抛异常了，helloworld 函数会停止执行，控制权交由上方异常处理函数，这样  delete 方法就不会执行，造成内存泄漏

相反，使用 Ptr，将 data 指针跟 Ptr 的生命周期绑定，在其析构时 delete, 由于作用域的缘故，在 helloworld 函数终止执行时，局部变量 Ptr 的生命到头，执行析构函数，此时自然 data 也会被 delete. 这样很安全

而我们的主角 scope_ptr, shared_ptr, weak_ptr, 跟上面的例子很像，也是在析构函数中回收指针（shared_ptr weak_ptr 是 refCount--, 到 0 才回收）

## Copyable and RefCount

scope_ptr 是 NonCopyable 的，而 shared_ptr weak_ptr 是 copyable 的，其表现是我们没法创建 scope_ptr 的拷贝：

```c++
scoped_ptr<Shoe> a(new Shoe);

scoped_ptr<Shoe> b(a);  // fail, scope_ptr<T>(const T&)
scoped_ptr<Shoe> c;
c = a;                  // fail, operator=(const T &)
```

原因自然是这些方法都 private 了：

```c++
private:
    scoped_ptr(scoped_ptr const &);
    scoped_ptr & operator=(scoped_ptr const &);
```

由于没有拷贝，自然就不存在 ref-count (引用计数) 的问题，所以到点准时走人(955 公司 微软知乎都能内推)：

```c++
~scoped_ptr() BOOST_SP_NOEXCEPT
{
    boost::checked_delete( px );
}
```

而 shared_ptr 和 weak_ptr 都可以创建其拷贝，所以有 ref-count，ref-count 到 0 才能被回收。

他们共享着 ref-count 指针，在拷贝/析构实例时，更新 ref-count 值，详细在下文

## shared_ptr  and weak_ptr

weak_ptr 可以脱离 shared_ptr 存在，但是没意义，因为 weak_ptr "管理"的指针是通过 shared_ptr 传入的。

讲 shared_ptr 和 weak_ptr 的区别，观察其数据结构即可：

```
shared_ptr:
  - element_type *px;
  - shared_count pn;
shared_count:
  - sp_counted_base * pi_;

weak_ptr:
  - element_type * px;
  - weak_count pn;
weak_count:
  - sp_counted_base * pi_;

sp_counted_base:
  - uint_least32_t use_count_;
  - uint_least32_t weak_count_;
```

而 weak_ptr 的构建：

```c++
weak_ptr( shared_ptr<Y> const & r ) : px( r.px ), pn( r.pn ) { ... }
weak_count(shared_count const & r) : pi_(r.pi_) { ... }
```

可以看到 weak_ptr 的一切，都是 shared_ptr 给的。

shared_ptr 的所有副本以及 weak_ptr 都共用着 sp_counted_base 指针，shared_ptr 更新 `use_count_`(临结束还会更新 `weak_count_`), weak_ptr 更新 `weak_count_`. 更新时机分别是 shared_ptr 的拷贝以及析构，weak_ptr 的构造，拷贝以及析构，可以参考本文开头例子的实现

这俩个 count 的值分别到 0，会发生什么事呢？

* use_count_ => 0, delete px;
* weak_count_ => 0, delete pi_;

use_count_ 到 0, delete px，很容易理解，所有的副本都被析构了，use_count_ 到 0，自然 `delete px`;

下面要说的是 `weak_count_`，我们看一下 sp_counted_base 的实现

```c++
// class sp_counted_base
sp_counted_base(): use_count_( 1 ), weak_count_( 1 ) {}
void release() // call by ~shared_count()
{
    if( atomic_decrement( &use_count_ ) == 1 )  // 相当于 use_count-- == 1, 也就是结束后 use_count_ = 0
    {
        dispose();
        weak_release();
    }
}
void weak_release() // call by ~weak_count()
{
    if( atomic_decrement( &weak_count_ ) == 1 ) // 相当于 weak_count_-- == 1, 也就是结束后 weak_count_ = 0 
    {
        destroy();
    }
}
virtual void destroy()
{
	delete this;
}
virtual void dispose() = 0; // 一般实现为 delete px
```

可以看到 `use_count_` `weak_count_` 的初始值都是 1，但意义不同：

* 由于 sp_counted_base 是在 shared_ptr 第一次构建时创建的，所以 use_count_ = shared_ptr_count
* 而创建时，并没有 weak_ptr, 所以 weak_count_ = weak_ptr_count + 1，这也是 shared_count 析构的最终还得 weak_release 一次

所以，想想这俩种 case 的情形，背后时怎么回事：

* weak_ptr 已经被析构了，shared_ptr 还存活

use_count_ = shared_ptr_count,  weak_count_ = 1, 在 `use_count_` == 0 时，weak_count_-- 到 0，`px` `pi_` 指针终于被回收

* shared_ptr 已经被析构了，weak_ptr 还有生还者

use_count_ == 0, weak_count_ = weak_ptr_count, px 指针已经被回收了，pi_ 指针还在苟延残喘。等到最后一个 weak_ptr 被析构，weak_count_ 为 0，pi_ 终于被 delete，终结了其罪恶的一生。

## 总结

* 智能指针严谨地利用了对象的构造，拷贝，析构等时机，控制资源的销毁，巧妙且安全。

* Copyable 也好 NonCopyable 也罢，只需细心在每一处时机，合适地更新资源的状态即可。

* 没想到 sp_counted_base 能够严格控制自己，在该销毁时，自己主动销毁，不制造任何脏东西，可靠且干净。

* 内推是真的。。。最好简历上附上个人博客 github 地址之类的，这样会高效一点（

