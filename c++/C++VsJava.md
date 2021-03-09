C++ vs Java

> 讨论区别时，可以从 Java 简化（阉割）的部分入手

Java 跟 C++ 有什么区别，记得刚毕业的时候，面试官问了我这个问题。我当时乱答一同，现在想想还是了解太少。。。最近看了俩本 C++ 的书，开始对这个问题有点想法，在此抛砖引玉，望诸公能给点建议，这样我也能看得更远的地方，谢谢。

本文会以：

- 所有对象都在堆上（失去 栈这种很好的对象回收机制 
- 拷贝 vs 引用     func(student&) vs func(student) 
- 默认所有对象方法是可继承的，可继承这件事其实是很危险的。脆弱的继承 

感觉 C++ 可以操作的粒度会小点，要操心的东西多一点。

## 所有对象都在堆上

C++ 的对象可以存放在栈上，当作用域退出时，栈上的对象执行析构，自然被回收。

而 Java 淡化了 C++ 栈 的概念。

Java 对象都会存在堆上，自然会有回收的问题，所以提出了垃圾回收机制。

对象在栈上有什么好处呢？

如果你熟悉[智能指针](boost/smart_ptr/)，或者 RAII，你会发现这俩者的实现都是基于：栈上的对象，在作用域退出时，自然执行析构函数

举例：

```
void example1() {
  HeavyResource* res = acquire();
  doSomething(res);
  res->release();
}

void example2() {
  HeavyResource* res = acquire();
  AutoRelease ar(res);
  doSomething(res);
}
class AutoRelease {
public:
  ~AutoRelease() {
    res->release();
  }
};
```

可以看到原先在 doSomething 结束之后，需要手动 release，现在都不需要了，由于 `ar` 是栈上的对象，在函数结束时执行析构函数，自然会执行 `res->release()` 。当然事情没有那么简单，万一 doSomething 抛异常了，怎么办。对于版本 1，直接跳到 example1 往上可以处理异常的代码。而版本2 也是会跳到 example1 往上可以捕获异常的地方，由于超过 ar 的作用域，ar 析构函数照样执行，res 被回收了。（先捕获异常还是先回收呢？测试结果是先回收

所以这种方式很安全，也很优美。

Java 也可以有类似的实现，可以用 `try-catch-finally` 将 release 语句写在 finally 块内。根据 java 的语义，就算 catch 异常，finally 还是会运行。

```java
HeavyResource res = null;
try {
  res = acquire();
  doSomething(res);
} catch (XXException e) {
} finally {
  if (res != null) {
    res.release();
  }
}
```

代码挺多的。。。

其实 java 也可以定制某个对象的 finallize （相当于 C++ 的析构）主要还是不能很好的预期析构对象的时机，毕竟都交给垃圾回收机制了。。。

## 拷贝 vs 引用

比如 `readBook(book1);` 这里面是值传递，引用传递，还是地址传递呢？



其实语言没有优劣之分，C++ 粒度小，工程师的自由度高，与之带来，需要考虑的东西就变多了，一个不好的决定，可能会引发意外的事情发生。Java 粒度粗一点，而工程师需要顾忌的点会少点，可以更多地关注在程序上。

