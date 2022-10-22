# Chromium 的 Sequence Checker （迷思）

当你修改 Chromium 的时候，你有可能会遇到 Sequence Checker 的错误。

当你很烦躁的时候，你可能会很懊恼，Chromium 为什么会有 sequence_checker 这么反人类的事情存在呢？

## Why（其中一个可能原因）

他是为了解决**不用锁**的情况下，能够在**多线程**中解决**并发**问题。

锁对解决多线程共享的数据读写可见性，原子性等有很重要的作用，最简单的例子就是多个线程同时发出更新银行账号余额的指令。不带锁，这些操作还能安全?

不带锁当然不安全，但是如果我告诉你 我有一项技术，能让这些“多线程”发出的指令，统统转交给一个线程依次来做，做完一个又一个，你看这样还会有问题吗？

比如 同时有很多人对一个账号发出 “存100元” “取99元” “存3元”。。。等指令，我就把这些指令统统扔到一个线程来做，逐个指令逐个指令来做，那不就没有问题了？

那有人会说了，那当然没问题了，这哪门子的多线程啊，都几乎约等于单线程了。

还是多线程啊，每次发送完指令，程序就往下继续执行，只是最后实际执行指令的是交由单独的线程**依次**跑。

Chromium 有个思想，叫做高并发，但不一定并行。 [highly concurrent, but not necessarily parallel](https://stackoverflow.com/questions/1050222/what-is-the-difference-between-concurrency-and-parallelism#:~:text=Concurrency is when two or,e.g.%2C on a multicore processor.)， 也就是尽可能做到不 block 线程，但是不要求必须并行运行线程。想想原先那些有指令在身的 thread 们，要是都用 lock 的话，那不得这些 thread 扎堆排队等。与其这样，还不如扔给单个线程，自己还落得个自在（俗称甩锅

（“扔给单线程”：对于你不需要知道结果的，直接调 PostTask, 对于需要知道结果的，调 PostTaskAndReply, 而且他已经帮你做好了“**将reply task放在合适的线程上运行**”的工作，你只需要传 Func 即可）

得勒，我无话可说。那什么时候回到 sequence_checker 了呢？

对，这个时候得回去了。在 Chromium， 不会直接使用上物理线程, 而是使用虚拟的线程, 叫 sequence.

他们的映射关系是 多个 thread 对应多个 sequence. [Chromium's Concurrency model.pptx - Google Slides](https://docs.google.com/presentation/d/1ujV8LjIUyPBmULzdT2aT9Izte8PDwbJi/edit#slide=id.p11)

每个 sequence 都有一个独特的 token。sequence_checker 就是检查当前 sequence 的 token 值，与之前存的是否一致，来判断跟之前是不是同一个 “线程”（其实此处应该用 sequence，为了方便记忆以及严谨，所以用“线程”），来确保“同类”数据只在同一个“线程”中运行，这也就是为什么会有特别蛋疼的 sequence_checker... 因为他觉得你有可能把“同类”数据放在不同“线程”操作，这样不就坏了他不用 lock 的百年大计了？

所以，当你处理一些自带 sequence_checker 的类的时候，不免心里发怵，其实没关系的，他们就是 强调你前后必须使用同样的“线程”来处理 的“同类”数据。

那为啥 chromium 非得大费周章，不用 lock ？ 因为是真的快啊（虽然找不到数据支撑，理论上就是强（



【TODO】可以继续看 sequence 与当前线程是如何映射的，Sequence 存了 SequenceLocalStorageMap, 据说起到了 tls 的作用，不知道具体是什么



问题来了，如果我遇到 sequence_checker 的问题，我也能找到他所在的 sequence，我怎么把 callback 扔到那个 sequence 上呢？

先记个【TODO】先