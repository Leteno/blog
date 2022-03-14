## Chromium 为什么会有 sequence_checker 这么反人类的事情存在呢？

他是为了解决没有锁存在的情况下能够在多线程中解决并发问题。锁对解决多线程共享的数据读写可见性，原子性等有很重要的作用，最简单的例子就是多个线程同时发出更新银行账号的指令。不带锁，这种操作还能安全?

不带锁当然不安全，但是如果我告诉你 我有一项技术，能让这些“多线程”的请求，统统交给一个线程轮流来做，你看还会有问题吗？

比如 同时有很多人对一个账号发出 “存100元” “取99元” “存3元”。。。等指令，我就把这些指令统统扔到一个线程来做，逐个指令逐个指令来做，那不就没有问题了？

那有人会说了，废话，当然没问题了，这哪门子的多线程啊，都单线程了。

还是多线程啊，对于不同作业，依旧是多线程，只是对于同类作业单线程了。Chromium 有个思想，叫做高并发，但不一定并行。 [highly concurrent, but not necessarily parallel](https://stackoverflow.com/questions/1050222/what-is-the-difference-between-concurrency-and-parallelism#:~:text=Concurrency is when two or,e.g.%2C on a multicore processor.)， 也就是尽可能做到不 block 线程，但是不要求必须并行运行线程。想想那些有指令在身的 thread 们，要是都用 lock 的话，那不得一堆 thread 扎堆排队等，这样还浪费宝贵的线程让它们干等。与其这样，还不如扔给单个线程，自己还落得个自在，所在线程得到解放（俗称甩锅

（“如何扔给单线程”：对于你不需要知道结果的，直接调 PostTask, 对于需要知道结果的，调 PostTaskAndReply, 而且他已经帮你做好了**“将reply task放在合适的线程上运行”**的工作，你只需要传 Func 即可）

得勒，我无话可说。那什么时候回到 sequence_checker 了呢？

对，这个时候得回去了。在 Chromium， 不会直接使用上物理线程, 而是使用虚拟的线程, 叫 sequence.

他们的映射关系是 多个 thread 对应多个 sequence. [Chromium's Concurrency model.pptx - Google Slides](https://docs.google.com/presentation/d/1ujV8LjIUyPBmULzdT2aT9Izte8PDwbJi/edit#slide=id.p11)

每个 sequence 都有一个独特的 token。sequence_checker 就是检查当前的 token 值，与之前存的是否一致，来判断跟之前是不是同一个 “线程”（其实此处应该用 sequence，为了方便记忆以及严谨，所以用“线程”），来确保“同类”数据只在同一个“线程”中运行，这也就是为什么会有特别蛋疼的 sequence_checker... 因为他不放心你把“同类”数据放在不同“线程”，这样不就坏了他不用 lock 的百年大计了？

所以，当你处理一些自带 sequence_checker 的类的时候，不免心里发怵，他们就是强调你前后必须使用同样的“线程”来处理的“同类”数据。

那为啥 chromium 非得大费周章，不用 lock ？ 因为是真的快啊（虽然找不到数据支撑（

问题来了，如果我遇到 sequence_checker 的问题，我也能找到他所在的 sequence，我怎么把 callback 扔到那个 sequence 上呢？

关注下集，人间自有真情在，爱因为在心中



ref: 

[Threading and Tasks in Chrome (googlesource.com)](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/threading_and_tasks.md)

[Chromium's Concurrency model.pptx - Google Slides](https://docs.google.com/presentation/d/1ujV8LjIUyPBmULzdT2aT9Izte8PDwbJi/edit#slide=id.p5)