# smart_ptr

> 巧妙地利用了 C++ 的机制，来控制指针的释放

刚开始了解 smart_ptr 的时候，是大学课本，当时没啥想法，就是一个封装了 refCount 的类，后续看了些书 [Effective C++](https://book.douban.com/subject/1842426/)， 开始觉得精巧，原来它会通过 C++ 的析构机制更新 refCount，以及 RAII。 现在开始拜读 boost 的源码，希望能够得到一些惊喜。默认都看过 [GetStart.md](00-GetStart.md)

## 路漫漫其修远兮

smart_ptr 位于 boostorg/boost/libs/smart_ptr (其实是 submodule，需要 clone --recursive 详见 [00-GetStart.md](00-GetStart.md)) 而文档可以去这个位置浏览 https://www.boost.org/doc/libs/release/libs/smart_ptr/，很抱歉， libs/smart_ptr/doc 下面我尝试用 b2 build 会报错，理论上应该可以本地生成文档的，有知道的同学希望能指点一下，谢谢。比较遗憾的是，这只有英文版，不过这是学计算机的必经之路。

文档中提到（也是我们需要从源码中印证的）：

* RAII，智能指针会 own 指针指向的资源，并负责在不需要的时候，销毁之
* 几种工具类以及生成工具对象的方法，比如 `scope_ptr` 在跳出本作用域时，销毁资源；`scope_array` 同样是 scope 的，只是负责 array. 都有 scope_ptr, 为啥需要 scope_array ? 我们应该在看的每个类实现的同时，多看同侪之间的区别。

扫了一眼，文档描述挺多的。打算以每一小节的文档介绍，再加上该介绍对应的源码。

### scope_ptr

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

可以看到上文并没有对 x 执行额外的操作，在离开其作用域时，`Shoe` 就被 `delete` 了，执行了析构函数。

而 MyClass 只是说明：

* 对于 scope_ptr 只要盯着他什么时候析构即可，放哪都一样。
* delete 真的是在退出作用域时发生的，先输出 `1` ,`2`，后输出 `Buckle my shoe`

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

可以看到，析构时 delete 指针 px。

* 文档提到：scope_ptr 不支持 share ownership 以及 transfer ownership. 

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

这跟 shared_ptr 不一样（它是 copyable）

* 文档里面提到：它不支持 array，请使用 scope_array，估计是 `delete` vs `delete[]`

* 文档它还提到：使用了 scope_ptr, 就是在告诉后继者 ownership 是不能被 transfer 的，所以说使用恰当的数据结构，能蕴含很多信息量，节省大家的时间。TQL

* 文档提到：Handle/Body 方法，避免在 Header 文件中过多地暴露实现：

  ```c++
  // example.h
  class example {
  public:
    void doSomething();
  private:
    class implementation;
    scoped_ptr<implementation> _impl;
  }
  
  // example.cpp
  class example::implementation {
  public:
    void sayHello() {
      cout << "Hello World";
    }
  };
  
  example::example() : _impl(new implementation) {}
  
  void example::doSomething() {
    _impl->sayHello();
  }
  ```

  由于 cpp 文件最终会被打包成二进制，所以里面的实现被隐藏了？还是没能 Get 到点上，我只能看到代码职责分离，几个小而巧的方法而不是一个总的方法。希望有同学能不吝赐教。

  

