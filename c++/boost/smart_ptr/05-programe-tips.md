# Programming Tips

文末总结了好多 smart_ptr 相关的编程技巧，我们逐一看并印证把：

## Using incomplete classes for implementation hiding

其实就是指针的类型用抽象类型会好于具体类型（隐藏实现，留有余地）

```
shared_ptr<Xiaoming> ming(new Xiaoming());    // No good，最少知识原则,没必要类型是 Xiaoming, 后续都没有操作空间
ming.readBook();

shared_ptr<Student> student(new Xiaoming());  // Good
student.readBook();
```

## The "Pimpl" idiom

一般头文件的修改，会影响到外部引用者重新编译，而 cpp 文件则不会影响外部调用者。

使用 Pimpl, 减少对头文件的修改，减少不必要的重新编译。。。

```c++
// file.hpp

class file
{
private:

    class impl;
    shared_ptr<impl> pimpl_;

public:

    file(char const * name, char const * mode);

    // compiler generated members are fine and useful

    void read(void * data, size_t size);
};


// file.cpp

#include "file.hpp"

class file::impl
{
private:

    impl(impl const &);
    impl & operator=(impl const &);

    // private data

public:

    impl(char const * name, char const * mode) { ... }
    ~impl() { ... }
    void read(void * data, size_t size) { ... }
};

file::file(char const * name, char const * mode): pimpl_(new impl(name, mode))
{
}

void file::read(void * data, size_t size)
{
    pimpl_->read(data, size);
}
```

## Preventing `delete px.get()`

避免 delete ptr.get().

* smart_ptr 会在自己生命周期结束时 delete 指针， 没必要画蛇添足
* 私自 delete smart_ptr 内的指针，而 smart_ptr 不知，smart_ptr 会访问回收过的区域，很危险！

如果真的很想自己操作 delete 过程，可以自己自定义 deleter:

```c++
class X
{
private:

    ~X();

    class deleter;
    friend class deleter;

    class deleter
    {
    public:

        void operator()(X * p) { delete p; }
    };

public:

    static shared_ptr<X> create()
    {
        shared_ptr<X> px(new X, X::deleter());
        return px;
    }
};
```

## Encapsulating allocation details, wrapping factory functions

古老的 C 代码，没有析构函数，怎么让他们享受现代生活呢？ 比如说他们有这些方法：

```c++
X * CreateX();
void DestroyX(X *);
```

可以 wrapper 一下，rap 起来 ！

```c++
shared_ptr<X> createX()
{
    shared_ptr<X> px(CreateX(), DestroyX);
    return px;
}
```

