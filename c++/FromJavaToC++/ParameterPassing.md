# 参数传递

> 你永远不用担心对象在 Java 里面会不会被 clone 一份出来，因为这些都需要显示调用。
>
> 但 C++ 在你不知道的时候，对象已经拷贝一份出来，而你操作的是拷贝，而不是原对象

给定俩段代码：

```java
// java
void getPaidFromBossPI(Worker w) {
    w.paid = w.time * 3.14;
}
Worker w(100 /* time */);
getPaidFromBossPI(w);
System.out.println(w.paid)
```

```c++
// c++
void getPaidFromBossPI(Worker w) {
    w.paid = w.time * 3.14;
}
Worker w(100 /* time */);
getPaidFromBossPI(w);
cout << w.paid << endl;
```

俩者结果如何呢？

```
java: 314
c++: 0  // 太惨了
```

java 所有对象都在堆上，自然是传引用。我们对引用的所有操作，自然在函数结束仍然生效

c++ 在上述的情况下，并没有传递引用，而是拷贝，即内存空间复制了一份，对复制品的操作，自然不会作用到原对象上，所以不给 pay。(C++ 写不好不给 paid 的  = =)

但实际上，C++ 支持你这么搞：

```c++
// c++
void getPaidFromBossPI(Worker* w) {
    w->paid = w->time * 3.14;
}
Worker w(100 /* time */);
getPaidFromBossPI(&w);
cout << w.paid << endl;
```

或者

```c++
// c++
void getPaidFromBossPI(const Worker& w) {
    w.paid = w.time * 3.14;
}
Worker w(100 /* time */);
getPaidFromBossPI(w);
cout << w.paid << endl;
```

这样，你就能得到 paid 了：

```
c++: 314
```

你或许会抱怨规则太复杂。其实我用下来感觉，Java 其实就是在简化 C++，去掉很多东西，只保留 essential 的东西，而实际上 去掉就意味着程序员在这方面缺少控制手段。

其实你也发现了，写 C++ 比 写 Java 容易出 bug，尤其是如果你写的经验不多，有些规则掌握不熟练的话。但是 C++ 由于能控制，是能让你写出更简洁干净的代码的。这就相当于自由恋爱虽然容易翻车，但比起包办婚姻，还是好一点的。。。

比如 一开始的那段程序，复制这一特性有用吗？有用的，如果函数申明为这个：

```c++
void print(Worker w);
```

你一般看到这样的函数，你就可以很放心地使用这个 print 方法，因为他只会操作其复制，正常情况下，不会影响到你原来的对象。(除非你对象里包含一个指针，你的复制函数又是直接拷贝这个指针的, shallow copy)

而且跟 const 不同，你还是可以直接操作这个对象的，并且这些操作都不会作用到原数据上（如果你拷贝函数是 deep copy 的话）