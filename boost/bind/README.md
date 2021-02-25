# Bind

依旧探索：

* repo https://github.com/boostorg/bind
* doc  https://www.boost.org/doc/libs/1_75_0/libs/bind/doc/html/bind.html

> boost::bind 起源于 std::bind1st std::bind2nd, 是他们的泛化。它支持函数对象，函数指针以及成员函数等，青出于蓝而胜于蓝，bind 不像标准库，对函数对象没有任何限制，要求。

例子：

```c++
int g(int a, int b, int c)
{
    return a + b + c;
}

// bind(g, 1, 2, 3)() 等价于 g(1, 2, 3)
```

它还可以这样：

```c++
using namespace boost::placeholders;
bind(g, _1, 2, _2) 等价于 f(x, y) { return g(x, 2, y); }
```

> Bind 一般会创建目标函数的拷贝，如果目标函数是 non-copyable, 或者拷贝操作昂贵的话，可以使用 boost::ref, 它会存储函数的引用。当然此时，我们得确保函数依旧存在

```c++
bind(ref(g), 1, 2, 3)()
```

