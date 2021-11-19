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

bind 也可以用于成员函数：

```c++
struct X
{
    bool f(int a);
};

X x;
shared_ptr<X> p(new X);
int i = 5;

bind(&X::f, ref(x), _1)(i);		// x.f(i)
bind(&X::f, &x, _1)(i);			// (&x)->f(i)
bind(&X::f, x, _1)(i);			// (internal copy of x).f(i)
bind(&X::f, p, _1)(i);			// (internal copy of p)->f(i)
```

bind 也可以互相嵌套，不过 placeholder 却是这样：

```
bind(g, bind(f, _3, _1), _2)("x", "y", "z") => g(f("z", "x"), "y")
```

bind 感觉用起来像是 lambda (可能是本末倒置):

```c++
std::remove_if(first, last, !bind(&X::visible, _1)); // 等价于 for each x: if(!x.visible()) remove;
std::find_if(first, last, bind(&X::name, _1) == "Peter" || bind(&X::name, _1) == "Paul");
```

Do and Don't 里面提及到：

```c++
int f(int);

int main()
{
    boost::bind(f, "incompatible");      // OK so far, no call
    boost::bind(f, "incompatible")();    // error, "incompatible" is not an int
    boost::bind(f, _1);                  // OK
    boost::bind(f, _1)("incompatible");  // error, "incompatible" is not an int
}
```

有点意思，记录一下 以后印证。