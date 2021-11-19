# 默认函数

我们先看下面的代码：

```c++
Student s1, s2;
s2 = s1;
Student s3 = s1;
```

其实上述代码每一行执行的默认函数都不同，分别是 默认构造函数，赋值函数，拷贝构造函数

分别是：

```c++
Student() {}

Student& operator=(const Student& s) {}

Student(const Student& s) {}
```

比较有意思的是这样写法会调用俩次函数：

```c++
Student s1 = Student();
             ---------  // 默认构造函数
       ------ // 拷贝构造函数
```

而我们写 Java 经常这么写，不过这样只会调用一次构造函数：

```java
Student s1 = new Student();
```

其实你也发现了，声明一个对象会调用构造函数/拷贝构造函数，赋值会调用赋值函数，另外执行函数形如 `void print(Student s)` 会调用 Student 的拷贝构造函数（使用别名可以避免调用该函数，如：`void print(const Student& s)`）

我们还没考虑右值呢，比如

```c++
print(Student());

// 如果有申明的话，可以匹配到函数 print(Student&& s)
```

而上述的所有情况，我们都可以重写其函数实现。干嘛这么麻烦，有什么用？

### 一点作用

我们可以完全阻止一个对象被拷贝，只需 delete 相应的拷贝构造函数，赋值函数，比如 unique_ptr 的实现：

```c++
class UniquePtr {
  UniquePtr(const UniquePtr& p) = delete;
  UniquePtr& operator=(const UniquePtr& p) = delete;
  ...
};
```

```c++
UniquePtr p1;
UniquePtr p2 = p1;  // error: UniquePtr(const UniquePtr& p) is deleted;
UniquePtr p3;
p3 = p1;           // error: UniquePtr& operator=(const UniquePtr& p) is deleted;
```

这里举另外一个例子  SharedPtr

```c++
class SharedPtr {
public:
  SharedPtr(void* ptr) {
    _count = new int(1);
    _ptr = ptr;
  }
  ~SharedPtr() {
    deRef();
  }
  SharedPtr(const SharedPtr& ptr) {
    deRef();  // 原先的 count - 1
    _count = ptr._count;
    _ptr = ptr._ptr;
    addRef();  // 新的 count + 1
  }
  SharedPtr& operator=(const SharedPtr& ptr) {
    if (ptr == this) return this;
    deRef();  // 原先的 count - 1
    _count = ptr.count;
    _ptr = ptr.ptr;
    addRef();  // 新的 count + 1
    return this;
  }

  // other default function.

  void deRef() {
  	if (_count) {
  	  *_count--;
  	  if (0 == *_count) {
  	    delete _ptr;
        delete _count;
        _count = nullptr;
        _ptr = nullptr;
  	  }
    }
  }
  void addRef() {
  	if (_count) {
  	  *_count++;
  	}
  }
private:
  int *_count;
  void* _ptr;
};
```

可以看到他在析构函数，拷贝构造函数，赋值函数中，更新 count 值，一旦 count 值为 0，则回收 ptr。

这样我们就能够很放心地把我们创建的内存空间交给它，他会很小心地更新 count 值，并在最后一个 SharedPtr 消亡的时候，回收内存空间。

形如：

```c++
void doSomething(SharedPtr pcopy) {
  // 操作里面的 _ptr
}

void test() {
  SharedPtr p1(new int[123]);  // count = 1
  SharedPtr p2 = p1;  // 调用赋值函数 count + 1 = 2
  doSomething(p1); // 由于函数命名为 (SharedPtr pcopy), 将执行拷贝构造函数，count + 1 = 3，但是在函数执行结束后，pcopy 将被析构，所以 count - 1 = 2;
}  // 函数结束后 回收 p1 p2, 调用其析构函数，此时 count - 2 = 0，触发 delete ptr; 我们之前传入的 (new int[123]) 将被回收
```

比较有意思的是，全程 SharedPtr 都是栈上的对象！他会很干净地穿梭在各大调用中，依然保持引用计数的准确（依靠拷贝 赋值 析构等函数）并不负使命，在最后一个 SharedPtr 倒下的时候，清理空间。

## 真的不要小看这些函数

我之前写过这样的代码：

```c++
auto fetcher = network::SimpleURLLoader::Create(
    std::move(request), TRAFFIC_ANNOTATION_WITHOUT_PROTO("ignore_me"));
// config on UrlLoader fetcher
fetcher->DownloadToString(
    url_loader_factory,
    base::BindOnce(
        [](std::unique_ptr<std::string> response) {
            LOG(INFO) << "Get Message: " << *response;
        }),
    response_buffer);
```

后面发现回调函数从未被调过，看一下别人的例子发现，很多人构建完 fetcher 之后，都是将 fetcher 储存起来，而不是单单一个局部变量。

怀疑是析构完之后不了了之了，直接存到堆上，嘿嘿，可以了。但不能直接这么干啊，得找个合适的安家之所，所以就这么写了：

```c++
auto fetcher = network::SimpleURLLoader::Create(
    std::move(request), TRAFFIC_ANNOTATION_WITHOUT_PROTO("ignore_me"));
// config on UrlLoader fetcher
fetcher->DownloadToString(
    url_loader_factory,
    base::BindOnce(
        [](std::unique_ptr<SimpleURLLoader> fetcher,
           std::unique_ptr<std::string> response) {
            LOG(INFO) << "Get Message: " << *response;
        }, // 回调函数结束后，unique_ptr 被析构，fetcher 终于结束自己罪恶的一生
        std::move(fetcher)
    ),
    response_buffer);
```

自己搞定之后，还会把自己回收，完美 ！

这也侧面说明，对象的生命周期是 C++ 里面需要考虑的点。