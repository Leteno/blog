# Bind codes

bind 的源码在：https://github.com/boostorg/bind

boost::bind 定义了很多这样的模板：

```c++
bind(F f);
bind(F f, A1 a);
bind(F f, A1 a, A2 a);
...
bind(F f, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5, A6 a6, A7 a7, A8 a8, A9 a9);
```

试了一下 11 个 args 的，果然失败了（偶然发现 std::bind 支持 11 个的。。

而上述模板最后是存储在 bind_t 的结构，以 A3 为例：

```c++
template<class R, class F, class A1, class A2, class A3>
    _bi::bind_t<R, F, typename _bi::list_av_3<A1, A2, A3>::type>
    BOOST_BIND(F f, A1 a1, A2 a2, A3 a3)
{
    typedef typename _bi::list_av_3<A1, A2, A3>::type list_type;
    return _bi::bind_t<R, F, list_type>(f, list_type(a1, a2, a3));
}
```

可以看到，尽管有多个函数模板，但是殊途同归，最后都是 `bint_t<R, F, list_type>`

学习一下 bind_t:

```c++
bind_t<R, F, L>:
  - F f_;
  - L l_;
```

看一下基础的不带参数的运算符 (),   result_type 的赋值见后文

```c++
result_type operator()()
{
    list0 a;
    return l_( type<result_type>(), f_, a, 0 );
}
```

也就是：

```
bind(f, 1, 2, 3)()
=> bind_t(f, list_type(1, 2, 3)) ()
=> list_type(1, 2, 3)(f_, list0, 0);
```

其实 list_type 就是 list3,  `list3::operator()`  的实现：

```c++
template<class A1, class A2, class A3> struct list_av_3
{
    typedef typename add_value<A1>::type B1;
    typedef typename add_value<A2>::type B2;
    typedef typename add_value<A3>::type B3;
    typedef list3<B1, B2, B3> type;
};
template< class A1, class A2, class A3 > class list3: private storage3< A1, A2, A3 >
{
    template<class R, class F, class A> R operator()(type<R>, F & f, A & a, long)
    {
        return unwrapper<F>::unwrap(f, 0)(a[base_type::a1_], a[base_type::a2_], a[base_type::a3_]);
    }
    // 特例化，考虑上了 void 的情况：
    template<class F, class A> void operator()(type<void>, F & f, A & a, int)
    {
        unwrapper<F>::unwrap(f, 0)(a[base_type::a1_], a[base_type::a2_], a[base_type::a3_]);
    }
};
// 看一下 unwrapper 的实现:
template<class F> struct unwrapper {
    static inline F & unwrap( F & f, long )
    {
        return f;
    }
    // 特例化, 考虑上 ref() 的情况
    template<class F2> static inline F2 & unwrap( reference_wrapper<F2> rf, int )
    {
        return rf.get();
    }
};


// list0 很无为，[] 传入多少，他就返回多少
class list0
{
public:
    list0() {}
    template<class T> T & operator[] (_bi::value<T> & v) const { return v.get(); }
};
```

解开所有

```c++
bind(f, 1, 2, 3) ()
=> bind_t(f, list3(1, 2, 3)) ()
=> list_type(1, 2, 3)(f_, list0, 0);
=> unwrapper<F>::unwrap(f, 0)(list0[add_value::type(1)], list0[add_value::type(2)], list0[add_value::type(3)])
```

其中 `add_value` 以及 `list3::operator []` 的实现：

```c++
template<class T> struct add_value
{
    typedef _bi::value<T> type;
};
template<class T> class value
{
public:
    value(T const & t): t_(t) {}
    T & get() { return t_; }
private:
    T t_;
};

template< class A1, class A2, class A3 > class list3: private storage3< A1, A2, A3 > {
    template<class T> T & operator[] (_bi::value<T> & v) const { return v.get(); }    
};
```

解引用：

```
=> unwrapper<F>::unwrap(f, 0)(list0[add_value::type(1)], list0[add_value::type(2)], list0[add_value::type(3)])
=> unwrapper<F>::unwrap(f, 0)(list0[value(1)], list0[value(2)], list0[value(3)])
=> unwrapper<F>::unwrap(f, 0)(value(1).get(), value(2).get(), value(3).get())
=> unwrapper<F>::unwrap(f, 0)(1, 2, 3)
=> f(1, 2, 3)
```

一层层的包裹，方便中间扩展

抄录一下：

```
bind(f, 1, 2, 3)()
=> bind_t(f, list3(1, 2, 3)) ()
=> list3(1, 2, 3)(f_, list0, 0)
=> unwrapper<F>::unwrap(f, 0)(list0[add_value::type(1)], list0[add_value::type(2)], list0[add_value::type(3)])
=> unwrapper<F>::unwrap(f, 0)(list0[value(1)], list0[value(2)], list0[value(3)])
=> unwrapper<F>::unwrap(f, 0)(value(1).get(), value(2).get(), value(3).get())
=> unwrapper<F>::unwrap(f, 0)(1, 2, 3)
=> f(1, 2, 3)
```



## placeholder

placeholder 是什么情况呢？

其定义

```c++
inline boost::arg<1> _1() { return boost::arg<1>(); }
inline boost::arg<2> _2() { return boost::arg<2>(); }
```

以上面的例子稍作修改：

```
bind(f, _1, _2, 3)(x, y)
=> bind_t(f, list3(_1, _2, 3)) (x, y)
```

看一眼 bind_t (x, y) 的实现

```c++
template<class R, class F, class L> class bind_t
    template<class A1, class A2> result_type operator()( A1 && a1, A2 && a2 )
    {
        rrlist2< A1, A2 > a( a1, a2 );
        return l_( type<result_type>(), f_, a, 0 );
    }
};

template< class A1, class A2 > class rrlist2
{
    A1 && operator[] (boost::arg<1>) const { return std::forward<A1>( a1_ ); }
    A2 && operator[] (boost::arg<2>) const { return std::forward<A2>( a2_ ); }
}
```

也就是：

```
=> bind_t(f, list3(_1, _2, 3)) (x, y)
=> list3(_1, _2, 3)(f, rrlist2(x, y), 0)
```

而 storage 系列对于 `_1 _2`等有特别照顾：

```
template<int I> struct storage1< boost::arg<I> > {
    explicit storage1( boost::arg<I> ) {}
    static boost::arg<I> a1_() { return boost::arg<I>(); }
};
```

所以解开应该是这样：

```
=> list3(_1, _2, 3)(f, rrlist2(x, y), 0)
=> unwrapper<F>::unwrap(f, 0)(rrlist2[storage3::a1_], rrlist2[storage3::a2_], rrlist2[storage3::a3_])
=> unwrapper<F>::unwrap(f, 0)(rrlist2[_1], rrlist2[_2], rrlist2[value(3)])
=> unwrapper<F>::unwrap(f, 0)(x, y, 3)
=> f(x, y, 3)
```

之前提到的 nested bind:

```
bind(f, bind(g, _1), _1, _2)(x, y)
```

对于 外部的 bind 照旧取 _1, _2 的值，对于内部 bind 的支持在这：

```c++
template< class A1, class A2 > class rrlist2 {
    template<class R, class F, class L> typename result_traits<R, F>::type operator[] (bind_t<R, F, L> & b) const
    {
        rrlist2<A1&, A2&> a( a1_, a2_ );
        return b.eval( a );
    }
};
```

直接传引用给内部 bind，而 bind_t 的 eval 跟自己的 operator() 是几乎等价的

```c++
template<class A> result_type eval( A & a )
{
    return l_( type<result_type>(), f_, a, 0 );
}
```

而这样的结构，支持了套娃，所以 bind 可以有多个内嵌 bind, 而且其传入的参数，所有 bind 都共享（比如此例的 rrlist2(x, y)）



## 成员函数的支持



