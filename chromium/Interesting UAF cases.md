# Interesting UAF cases

最近有个同事问我 C++ 的问题，后面帮忙解决了，仔细咂摸，觉得这里面挺有意思的，所以写个博客记录一下。

大概是这样的：

```
class Student {
  Student(LibraryProxy* proxy) : proxy_(proxy) {}
  void Reset() { proxy_ = nullptr; delete this; }
  void BorrowBook() {
    if (proxy_) {
      proxy_->borrowBook(Random());  // 学什么知识看命，命运改变知识（   // Line 01
    }
  }
  void study() {
    // studying
    if (GotProblem()) {
      base::ThreadPool::PostTask(
        IO,
        base::BindOnce(&Student::BorrowBook, base::Unretained(this));  // Line 02
      )
    }
  }

  LibraryProxy* proxy_ = nullptr;
};
```

他发现一个事情，就是他明明已经调过了 Reset 了，BorrowBook 内，proxy_ 应该已经是 nullptr 了，但是 Line 01 还是被调用了。

这是为啥呢？

加了如下的 log

```
void Reset() {
  DLOG(INFO) << "Reset：proxy_ " << proxy_;
  proxy_ = nullptr;
  delete this;
}
void BorrowBook() {
  DLOG(INFO) << "BorrowBook: proxy_ " << proxy_;
  if (proxy_) {
    proxy_->borrowBook(Random());
  }
}
```

得到的日志是：

```
Reset: proxy_ 0x54321
...
BorrowBook: proxy_ 0x4
```

真是见鬼了。nullptr 也不应该是 `0x4` 啊。

其实同事目前成功地写了一个 UAF 的 bug（逃

## UAF

UAF 全称是 Use After Free，字面意思就是 仍然访问已经被 free 掉的内存区域。

比如：

```
Student* s = new Student();
delete s;
s->SayHello();  // Line 01
```

Line 01 就是典型的 UAF

但是有意思的是，正常情况下，直接拷贝上面的 code 去运行，他也不会报错。

不会报错是因为被 free 掉的内存区域 还没被拿去**重新使用（Re-Use）**。

假设 Student 的内存空间如下：

```
---------     |  offset 0x0    <======== Student ptr
xxxx          |
proxy_ ptr    |  offset 0x8
xxxx
---------
```

假设此时 Student 内存被人释放了，然后分配给某个 ObjectX

```
---------     |  offset 0x0   <=========== "Student ptr"
xxxx          |
yyyy          |  offset 0x8
xxxx
---------
```

此时如果我们还是用我们之前保存的 Student 指针再去访问 proxy_ 的时候，就会发生错误，因为此时的 proxy_ 内存块的值已经被改写了。也就是上文我们在 log 中看到的 `0x4`

## 具体是哪里写错了呢？

写错的地方，其实是 最上方 code 的 Line 02.

我们将裸指针 this 包裹成 callback，抛给线程池。当 callback 被调用的时候，如果 Student 已经被销毁了，我们仍然用之前保存的裸指针，继续调用 `BorrowBook`, 但是原先 Student 的内存区域已经重新分配给其他 Object 了, proxy_ 的值也随之被其他 Object 修改成他们的成员变量，这个时候再去调用 proxy_ 的方法，其结果是不可预测的（我们现在只是 crash 了，但是对于有经验的 hacker 来说，可以通过spray，不停 construct 一个精心准备的 object，将 proxy_->borrowBook() 重新导向到一个 hacker 自己的代码片段中，这就劫持了我们的函数了）

那怎么办呢？

## 怎么办

解决 UAF 的方法，就是不要 UAF

即，我们应该让 callback 在被调用的时候，判断一下自己是否已经被 free 回收掉。

Chromium 已经为我们提供上述功能了，就是 `WeakPtr`

在 Bind Callback 的时候，我们将裸指针改为 WeakPtr, Chromium 在 callback 执行之时会检查该 WeakPtr 宿主是否已经销毁，如果是，则不执行任何操作。写法如下：

```
class Student {
  Student(LibraryProxy* proxy) : proxy_(proxy) {}
  void Reset() { proxy_ = nullptr; delete this; }
  void BorrowBook() {
    if (proxy_) {
      proxy_->borrowBook(Random());  // 学什么知识看命，命运改变知识（   // Line 01
    }
  }
  void study() {
    // studying
    if (GotProblem()) {
      base::ThreadPool::PostTask(
        IO,
        // base::BindOnce(&Student::BorrowBook, base::Unretained(this));  // Line 02
        base::BindOnce(&Student::BorrowBook, weak_ptr_factory_.GetWeakPtr());
      )
    }
  }

  LibraryProxy* proxy_ = nullptr;
  /* +++++++++ */
  base::WeakPtrFactory<Student> weak_ptr_factory_{this};
  /* +++++++++ */
};
```

// 调查那一段 code 阻止 WeakPtr 运行 callback。

解了，详看 [PostTask and WeakPtr.md](threading/PostTask%20and%20WeakPtr.md)



挺有意思的一段经历的。希望对后面的工作有所启迪。
