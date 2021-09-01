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

哪一天有人在 do something, 中间判断一个条件就直接 return 了，导致你末尾的回收资源的代码不被执行，也不用担心突然 do something 出了 exception，末尾回收资源的代码不被执行。（所以有人说这样安全