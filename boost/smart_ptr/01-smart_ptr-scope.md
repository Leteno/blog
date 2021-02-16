# smart_ptr

> 巧妙地利用了 C++ 的机制，来控制指针的释放

刚开始了解 smart_ptr 的时候，是大学课本，当时没啥想法，就是一个封装了 refCount 的类，后续看了些书 [Effective C++](https://book.douban.com/subject/1842426/)， 开始觉得精巧，原来它会通过 C++ 的析构机制更新 refCount，以及 RAII。 现在开始拜读 boost 的源码，希望能够得到一些惊喜。Setup 环境 [GetStart.md](00-GetStart.md)

## 路漫漫其修远兮

smart_ptr 位于 boostorg/boost/libs/smart_ptr (其实是 submodule，需要 clone --recursive 详见 [00-GetStart.md](00-GetStart.md)) 而文档可以去这个位置浏览 https://www.boost.org/doc/libs/release/libs/smart_ptr/，很抱歉， libs/smart_ptr/doc 理论上应该可以本地生成文档的，我尝试用 b2 build 会报错，有知道的同学希望能指点一下，谢谢。比较遗憾的是，文档只有英文版，不过这是学计算机的必经之路。

> 文档提及的东西会以这个形式

我们尝试在看的每个类实现的同时，比较同侪之间的区别。

扫了一眼，文档描述挺多的。打算以每一节的文档介绍，再加上该介绍对应的源码。

## scope_ptr

文档提到 scope_ptr 会负责资源的释放，时机可以是自己被析构的时候（局部变量退出作用域）抑或是执行 reset() 函数：

```c++
#include <boost/scoped_ptr.hpp>
#include <iostream>

struct Shoe { ~Shoe() { std::cout << "Buckle my shoe\n"; } };

class MyClass {
    boost::scoped_ptr<int> ptr;
  public:
    MyClass() : ptr(new int) { *ptr = 0; }
    int add_one() { return ++*ptr; }
};

int main()
{
    boost::scoped_ptr<Shoe> x(new Shoe);
    MyClass my_instance;
    std::cout << my_instance.add_one() << '\n';
    std::cout << my_instance.add_one() << '\n';
}

===== 输出 ====
1
2
Buckle my shoe
```

而源码中析构函数：

```c++
~scoped_ptr() BOOST_SP_NOEXCEPT
{
#if defined(BOOST_SP_ENABLE_DEBUG_HOOKS)
    boost::sp_scalar_destructor_hook( px );
#endif
    boost::checked_delete( px );
}
```

可以看到，`scoped_ptr` 析构时 delete 指针 px。

可以看到例子中并没有对 x 执行额外的操作，在离开其作用域时，`Shoe` 就被 `delete` 了，执行了析构函数 `~Shoe()`。

而 MyClass 只是说明：

* 对于 scope_ptr 只需关注`~scope_ptr()`析构时机，放哪都一样。
* delete 真的是在退出作用域时发生的，先输出 `1` ,`2`，后输出 `Buckle my shoe`

> scope_ptr 不支持 share ownership 以及 transfer ownership. 

其源码的表现是 private 了拷贝构造函数，以及赋值运算符：(noncopyable)

``` c++
private:
    scoped_ptr(scoped_ptr const &);
    scoped_ptr & operator=(scoped_ptr const &);
```

这样就会阻止类似：

```c++
scoped_ptr<Shoe> a(new Shoe);

scoped_ptr<Shoe> b = a; // 等价于 scoped_ptr<Shoe> b(a); 拷贝构造函数是 private 的
scoped_ptr<Shoe> c;
c = a;                  // 失败，赋值运算符是 private 的
```

这样就可以确保实例只有一个，功能处处受限能使我们安心，不怕它被人乱来（再坏能坏到哪去）受限也是一种自由。

## scoped_array

> scoped_array 管理动态生成的数组`new[]`, 会在析构函数，或 reset 函数销毁数组。
>
> 与 shared_ptr<T[]> 相比，scoped_array 是 noncopyable 的

noncopyable 同 scoped_ptr，拷贝构造函数和赋值运算符也是 private 的。

而跟 scoped_ptr 的区别是 delete 资源时调用:  `delete[]` (scoped_ptr 是 `delete`)

> 与之相比，std::vector 比较重，但是更灵活；boost::array 则是不需动态分配内存

居然有不用动态分配内存的，有这种好事？看了一下 boost::array 的实现，原来是利用了 template:

```c++
template<class T, std::size_t N>
class array {
	T elems[N];
};
```

实际上，N 只能是常量表达式（非类型参数），比不上 new[dynamicInt]，与普通的 `int array[6]` 相比，只能说多了很多封装好的方法，但是这个也不是没有代价：

```
array<int, 6>
array<int, 7>
```

实际上会在编译期生成俩个不同的类（详见 C++ Primer 16.1 定义模板 -  非类型模板参数）

不过这也说明 std::vector 会动态定义空间，估计 enlarge 的时候也掺和一脚。没看过源码，mark 一下。

介绍里面提及重载了 `[]` 符号，所以可以直接 `scoped_array[index]` 取值了.

蓦然发现它这边还 private 了操作符 `==`, `!=` ，scoped_ptr 也是这样，为啥呢？

原因估计是：既然是 uncopyable, 没有对比的必要了，因为都是不等于的。

瞄了一眼 shared_ptr, 稳了，它 public 了操作符。

