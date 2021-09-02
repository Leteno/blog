C++ 的析构函数

一个对象 在其生命周期结束时，会调用其析构函数。

简单提一下生命周期：对象的生命周期跟对象所处的位置有关，栈上的，作用域结束生命结束，堆上的 delete/free 时释放, 而 static 由于存储在 bss 上，所以持续到程序结束。

回到主题 析构函数。

很多人在印象中的析构函数大抵是这样的：

```c++
class Array {
int* data_;
int size_;

Array(int size) {
  size_ = size;
  data_ = new int[size];
}
~Array() {
  delete data_;
}
};
```
是的 析构函数常用于回收构造函数分配的内存。有分配有回收，有头有尾。
然而这种生命周期结束，析构“自动”被调用的机制被人盯上了。

我们总有这样的需求：
* 当我们申请一个资源，我们总要负责这个资源的释放
* 当我们“临时”修改某个值的时侯，在我们做完事的时候，我们总想把这个修改恢复回去，神不知鬼不觉。

所以人们就往析构函数动手脚了
```c++
// 资源锁的例子
class Lock {
IResource* r_;

Lock(IResource* r): r_(r) {}
~Lock() {
  r_->freeResource();
}
};

{
  IResource* r = acquire();
  Lock lock(r);
  // do something to r
} // lock 作用域结束，调用其析构函数，自然 r 就被回收了
```

```c++
// 换灯泡的例子
class Electricity {
  bool on;
};
class WorkerWork {
  Electricity* e_;
  bool original_state_;
  WorkerWork(Electricity* e): e_(e) {
    original_state_ = e->on;
    e->on = false;
  }
  ~WorkerWork() {
    e_->on = original_state_;
  }
};

Eletricity e;
// ...
{
  WorkerWork(&e);
  // 电力切断了，赶紧换灯泡
} // WorkerWork 会被析构，电力要恢复了。。。
```

你会发现这个是新建资源后，委托给别人，然后就可以解放了？？？
我们对比一下：

```
{
  IResource* r = acquire();
  // do something to r
  r->freeResource();
}
```

跟这个相比，有什么区别呢？只是能 "减少" 工程师的注意力吗？

还有一点，安全。

万一 do something 内出现了 exception，代码则会跳转到 catch 语句中，后面的 free 语句没有执行到，最后这个资源就没法回收了。。。（从这可以看出 try-catch 不好的地方，有些原教主义的人不喜欢 try-catch，他们喜欢 bool doSomething(), 或者  bool doSomething(error&).

好的 这就是 析构函数在 C++ 使用时候比较常见的俩个点了。它凭借制度优势（跳出作用域则被执行），被工程师青睐，干一些回收的活，而且还很安全。

