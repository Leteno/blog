# shared_ptr

> a versatile tool for managing shared ownership of an object or array

versatile 是多才多艺的意思。文档这么高的评价，值得学习其神奇的地方。

> shared_ptr ，类模板，用来存放动态分配的内存指针，尤其是 new, 会保证在最后一个 shared_ptr 被销毁（或 reset）的时候，delete 指针

从数据结构出发：

```c++
template<class T> class shared_ptr {
private:
    element_type *px;                 // 存放的内存指针，element_type 就是 T，详见下文
    boost::detail::shared_count pn;   // 引用这个指针的 shared_ptr 的个数，sp 销毁时 --，为 0 则 delete px; 
};

class shared_count {
   	sp_counted_base * pi_;           // sp_counted_base 是指针，拷贝的多个 shared_ptr 会共享同一个 pi_
};

// 这个的实现因平台而异，详见 libs/smart_ptr/include/boost/smart_ptr/detail/sp_counted_base_*.hpp
// 本文取 sp_counted_base_gcc_x86.hpp 为例
class sp_counted_base {
    int use_count_;
    int weak_count_;
    sp_counted_base(): use_count_( 1 ), weak_count_( 1 ) {} // 初始值均为 1
    void add_ref_copy() {
        atomic_increment( &use_count_ ); // 汇编代码实现，原子操作 use_count_ ++; 所以才有不同平台的实现
    }
    void release() {
        if( atomic_exchange_and_add( &use_count_, -1 ) == 1 ) // use_count_ += -1; 原子操作，返回加法结果
        {
            dispose();
            weak_release();
        }
    }
    void weak_release()
    {
        if( atomic_exchange_and_add( &weak_count_, -1 ) == 1 )
        {
            destroy();
        }
    }
   	virtual void dispose() = 0;
    virtual void destroy() { delete this; }
};
```

比较有意思的是上述的判断是 == 1, 也就是说 初始值为 1，第一次引用，值为 2。

另外一点是， shared_count 存放的是 sp_counted_base 指针，所以每次 shared_ptr 拷贝 析构 可以更新同一块内存空间(ref-count) 自然就知道什么时候我们可以安全地 delete 目标指针了：

```c++
// 拷贝构造时 +1
shared_count(shared_count const & r) BOOST_SP_NOEXCEPT: pi_(r.pi_)
{
    if( pi_ != 0 ) pi_->add_ref_copy();
}
// 析构时 -1
~shared_count()
{
    if( pi_ != 0 ) pi_->release();
}
// 赋值操作符会 当前 pi_--, 新值 pi_++，压缩篇幅

// pi_ 的赋值挺绕的 为：默认值为：new sp_counted_impl_p<Y>( p );
template<class T> class sp_counted_impl_p : public sp_counted_base {
	void dispose() BOOST_SP_NOEXCEPT BOOST_OVERRIDE
    {
        boost::checked_delete( px_ ); // delete in dispose, dispose 的调用见上文。
    }
};
```

结合上文，可以看到：

* 当 shared_ptr 新增一个实例的时候，成员 shared_count 也被新增，最终执行 `pi_->add_ref_copy` 即 pi_.count ++
* 当 shared_ptr 销毁一个实例的时候，成员 shared_count 也被销毁，执行 `pi->release` 即 pi_.count --, 减到初始值时，执行 dispose，也就是 delete

我们也可以传入 delete 处理函数：

```c++
template<class P, class D> shared_ptr( P * p, D d ): px( p ), pn( p, d )
{
    boost::detail::sp_deleter_construct( this, p );
}

// 经由 shared_count(pn)，使得 pi_ = new sp_counted_impl_pd<P, D>(p, d)  (压缩篇幅。。)

template<class P, class D> class BOOST_SYMBOL_VISIBLE sp_counted_impl_pd: public sp_counted_base {
    P ptr;
    D del;
    sp_counted_impl_pd( P p, D & d ): ptr( p ), del( d ) {}
    void dispose() BOOST_SP_NOEXCEPT BOOST_OVERRIDE { del( ptr ); }
};
```

也就是 传入的 del 函数指针，

* 需要满足 `del(P* ptr)` 的能力
* 最终是由 sp_counted_base 的 dispose 来调用的。调用时机为**最后一个** shared_count/shared_ptr **被销毁** 见上文

瞄了一眼，除了可以传入 del 之外  还能传入 allocator (template 实现，需要支持 deallocate )，不在这里赘述。

卧槽, shared_ptr 还能本身就支持数组 delete，不需要額外传 deleter

```c++
template<class Y>
    explicit shared_ptr( Y * p ): px( p ), pn() // Y must be complete
{
    boost::detail::sp_pointer_construct( this, p, pn );
}
template< class T, class Y > inline void sp_pointer_construct( boost::shared_ptr< T[] > * /*ppx*/, Y * p, boost::detail::shared_count & pn )
{
    sp_assert_convertible< Y[], T[] >();
    boost::detail::shared_count( p, boost::checked_array_deleter< T >() ).swap( pn );
}

template<class T> struct checked_array_deleter
{
    void operator()(T * x) const BOOST_NOEXCEPT   // 由于是 del(T* x)
    {
        boost::checked_array_delete(x);         // delete [] x;
    }
};

```



比较有意思的是这一处：

```c++
template<class T> class shared_ptr
{
public:
	typedef typename boost::detail::sp_element< T >::type element_type;
};
// 而 sp_element<T> 的定义:
template< class T > struct sp_element
{
    typedef T type;
};
```

巧妙地获取了类型 T。

以及这一处：

```c++
void reset() BOOST_SP_NOEXCEPT
{
    this_type().swap(*this);
}
void swap( shared_ptr & other ) BOOST_SP_NOEXCEPT
{
    std::swap(px, other.px);
    pn.swap(other.pn);
}
```

swap 函数没啥好说的，而 reset 函数却是用一个局部值`this_type()`与之交换. 我们原先的 shared_count 去了局部值内，由于函数结束，作用域退出，局部值会被销毁，所以原先的 shared_count 也会随之销毁。而原先的局部值的 shared_count 内 pi_ 为空，换到我们当前的 shared_ptr 保留也没事。所以是真·重置了成员变量。

用一个不恰当的比喻：如何清理垃圾？绑在生命周期更短的目标上。

## Best Practice

> 任何 new 分配的指针，都应该让 shared_ptr 管理

```c++
改：
int *pInt = new int(123);
int *pAnotherInt = pInt;
// 此处出 exception 就跪了，delete 将不被执行
delete pInt;
为：
shared_ptr<int> pInt(new int(123));
shared_ptr<int> pAnotherInt = pInt;
```

让 shared_ptr 管理动态分配资源的释放，又由于它会随着最后一个 shared_ptr 析构（由于局部变量离开作用域）自动释放资源，不担心中间由于 exception 而内存泄漏之类的问题，十分安全可靠。

## 结语

shared_ptr 利用 pi_ 指针保存引用数，在每次拷贝或是析构时，更新 pi_ 的内容，当判断 pi_ 已回复到初始值，认定当前销毁的 shared_ptr 为最后一个，此时所管理的内容 ptr 便被销毁。更新 pi_ 的 count 是汇编操作，确保原子，各平台均有实现。

支持普通指针 数组指针(T[])，也支持自定义 deleter，还支持 allocator.deallocate ! 确实多姿多彩.