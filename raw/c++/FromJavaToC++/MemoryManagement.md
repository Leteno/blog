> Java 的对象都在堆上，而 C++ 可以是在 栈 上

Java 对空间回收的概念比较模糊。

举个例子：

```java
// java
String toString() {
  StringBuffer buf = new StringBuffer();
  buf.append("juzhen");
  buf.append("-");
  buf.append("HelloWorld");
  return buf.toString();
}
```

```c++
// c++
char* getName() {
  char name[] = "juzhen";
  return name;
}
```

在 Java 执行完 toString 之后，栈上的内容会被回收，但是由于 buf 指向的内容存储在堆上，且 buf.toString() 返回的对象也是存储在堆上的，所以我们依旧可以访问到 `buf.toString` 的内容。

而 C++, 由于 name 实际上是定义在栈上的，所以在 `getName` 结束时，栈上的 `name` 将被回收，所以返回的是一个被回收的指针，对它执行的操作，结果未定义。

如果真的想返回 string name，可以考虑把 name 放在堆上，返回 name，或者，接受一个 name 指针，往 name 里面放内容。

```c++
// c++
char* getName() {
  char* name = malloc(sizeof(char) * 10);
  strcat(name, "juzhen");
  return name;
}

bool getName(char* name, int size) {
  if (size < 7) {
    return false;
  }
  strcat(name, "juzhen");
  return true;
}
char myName[10];
getName(myName, 10);
```

建议用下面的，干净 =v=） 全都在栈上，不在堆上。

写 C++ 的时候，尤其是返回值，必须考虑对象的生命周期，是否会被回收等等，而 Java 则是简化这一过程，让你无需考虑，因为它都在堆上，就连数组，也需是 `char[] name = new char[123]` 

个人感觉，虽然 Java 帮你简化了这一过程，但是工程师也失去了一些东西，对内存空间的掌控。你只能把释放内存空间，交予 Java 的垃圾回收机制，你唯一能做的是尽量少申请不必要的空间，接着就是相信 gc 能帮你处理好。

而 C++ 虽然需要你多考虑一层，但却给你很高的自由度，让你能高效的管理内存空间。只要你申明在栈上的空间，在作用域结束时都能帮你回收掉，如果是对象，他还会帮你执行析构函数，执行你制定的回收代码。

析构函数，我一开始也以为只是回收对象内管理的内存，后面发现有很多技巧利用了 “作用域消失，栈上对象执行析构函数” 这一特性，如下代码：

```c++
class ResourceManager {
public:
  ResourceManager(Res* res) : _res(res) {}
  ~ResourceManager() {
      res->free();
  }
private:
  Res* _res;
};

int main() {
  Res res;
  ResourceManager watcher(&res);
  // Do something to res
  return 0;
} // When the function is done, ~ResourceManager will be called. The res will be free() automatically.
```

我们将在 [DefaultFunction](DefaultFunction.md) 有更多的讨论。