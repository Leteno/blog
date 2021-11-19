# Python 中的 with

> Python 中的 with 会不会参考了 C++ 的 RAII

## 前言

我们经常看到 Python 会有这样的代码：

```python
with open('filename', 'r') as f:
	f.read()
```

“约"等于

```python
f = open('filename', 'r')
f.read()
f.close()
```

而 C++ 的 RAII 是 (Resource Acquisition Is Initialization) 利用对象的生命周期控制资源的释放。形如：

```c++
{
   Resource *r = AquireResource();
   Lock lock(r);
   doSomething(r);
}
```

而 Lock 的定义常常是：

```c++
class Lock {
public:
  Lock(Resource* r): _r(r) {}
  ~Lock() { r->Release(); }
private:
  Resource* _r;
};
```

由于 Lock 是一个局部变量，当程序离开其作用域时，由于 C++ 的机制，会被销毁，运行析构函数，此时便执行 r->Release()。这种不需要手动释放资源的方法，还有一个好处 就是 安全。 对比这个：

```c++
{
  Resource *r = AquireResource();
  doSomething(r);
  r->Release();
}
```

如果 doSomething 中遇到 Exception, r->Release() 便不会被执行。

本文将探索 python 的源码：

* 印证 Python 是否使用了 RAII 类似的机制释放资源
* 释放资源使用的接口是什么，close() ? release() ? 方便将来套用

## 长路漫漫伴我闯

Python 的源码在： https://github.com/python/cpython.git

而关于 python 的句法在：[Parser/Python.asdl](https://github.com/python/cpython/blob/091951a67c832db83c60f4eb22f1fb474b70e635/Parser/Python.asdl)， with 条目：

```
With(withitem* items, stmt* body, string? type_comment)
```

搜一下 withitem, 有一处很好，把 python 语句对应的代码列出来了： [Doc/library/ast.rst#L1215-L1239](https://github.com/python/cpython/blob/b37c994e5ac73268abe23c52005b80cdca099793/Doc/library/ast.rst#L1215-L1239)

语法：[Grammar/python.gram#L184-L196](https://github.com/python/cpython/blob/06f8c3328dcd81c84d1ee2b3a57b5381dcb38482/Grammar/python.gram#L184-L196) 

而程序怎么处理 with_stmt 结构的：[Parser/parser.c#L4071](https://github.com/python/cpython/blob/06f8c3328dcd81c84d1ee2b3a57b5381dcb38482/Parser/parser.c#L4071)

先是读取到各个 Token（毕竟是解释型语言，边 Parse 边 Execute）接着调用 `_Py_With()` 传入 with_items `a` 以及语句块 `b`.

`_Py_With` 定义的地方： [Include/Python-ast.h#L602-L605](https://github.com/python/cpython/blob/a5634c406767ef694df49b624adf9cfa6c0d9064/Include/Python-ast.h#L602-L605),  [Python/Python-ast.c#L2317-L2319](https://github.com/python/cpython/blob/a5634c406767ef694df49b624adf9cfa6c0d9064/Python/Python-ast.c#L2317-L2319) 

这个方法其实没干什么事，就是把所有传入的参数包裹成一个结构 `stmt_ty`, 最后作为 with_stmt_rule() 的返回值，而其调用顺序为：

with_stmt_rule <= compound_stmt_rule <= statement_rule <= _loop1_11_rule <= statements_rule <= block_rule ...

出现 block_rule 的地方太多，分支太多。 既然是包裹成 stmt_ty, 自然有用到里面内容的地方，搜一下 With.Item，发现这一处：

[Python/compile.c](https://github.com/python/cpython/blob/c9bc290dd6e3994a4ead2a224178bcba86f0c0e4/Python/compile.c#L4904-L4926)

而 compile.c 在开头写了自己的职责，生成 Python bytecode 的. 生看看不了，整理一下各个结构之间的关系(文件开头就有定义)：

```
compiler:
  - compiler_unit* u
  - PyObject* c_stack
compiler_unit:
  - PyObject *u_varnames; // local variables
  - basicblock *u_blocks; // most recent allocated block, by b_list, you could iterate all allocated blocks
  - basicblock *u_curblock;
basicblock:
  - basicblock* b_list; // next node
  - instr *b_instr;  // instruction arrays !!!
instr:    // instruction 指令
  - u-char i_opcode;
  - int i_oparg;
  - struct basicblock_ *i_target; /* target block (if jump instruction) */
  - int i_lineno;   // Debugging ?
PyObject:
  - Py_ssize_t ob_refcnt;  // 卧槽，protobuf ？
  - PyTypeObject *ob_type;
```

所以

* 每个代码块(code block) 都映射到单独的 basicblock 上，basicblock 里面存放代码块里面的所有翻译的指令
* 每个指令的操作数只有一个 `i_oparg`, 而跳转则是直接传 basicblock 指针 O.O  太硬核了。

由于签名是 `compiler_with(struct compiler *c, stmt_ty s, int pos)`，所以我们观察 c->u->u_curblock->b_instr 之类的变化。

compiler_with 更多的是怎么去组织各个块的位置 block final exit，以及中间穿插一些其他指令 SETUP_WITH POP_TOP .

```
比如：
VISIT(compiler, expr, expr_ty)  // 背后调用 compiler_visit_expr，而做法也是往 u_curblock->b_instr 添加指令
compiler_use_next_block(compiler, basicblock)  // 将 basicblock 加入到 c->u 中 作为 c->u->u_curblock
```

由于不理解这里面各个指令（SETUP_WITH, POP_TOP）意味着什么，这个地方没法继续看，我们还得继续看，看 python 怎么处理自己的 bytecode.

## 独上高楼 望尽天涯路

搜一下指令 SETUP_WITH，有俩块地方值得去看：

* [Objects/frameobject.c](https://github.com/python/cpython/blob/cb9879b948a19c9434316f8ab6aba9c4601a8173/Objects/frameobject.c#L167)
* [Python/ceval.c](https://github.com/python/cpython/blob/0564aafb71a153dd0aca4b9266dfae9336a4f2cb/Python/ceval.c#L1486)

看了一下他们开头的描述，应该是 ceval.c 了。

ceval.c 在执行 bytecode 的时候，会用一个栈存储中间产物，比如 1 + 2 + 3

实际的 bytecode 有可能是(右边是栈的情况)：

```
PUSH 1                   // 1
PUSH 2                   // 1, 2
ADD                      // 3
PUSH 3                   // 3, 3
ADD                      // 6
```

而他们对栈的操作在这： [Python/ceval.c#L1163-L1175](https://github.com/python/cpython/blob/0564aafb71a153dd0aca4b9266dfae9336a4f2cb/Python/ceval.c#L1163-L1175)

而栈的定义及其赋值是：

```
PyObject **stack_pointer;  /* Next free slot in value stack */
stack_pointer = f->f_valuestack + f->f_stackdepth;
```

观察 stack_pointer 的赋值变化，看到一处有意思的地方：

```
case TARGET(CALL_METHOD):
  PyObject **sp;
  sp = stack_pointer;
  ...
  res = call_function(tstate, &sp, oparg, NULL); // sp 会被人修改的？
  stack_pointer = sp;
```

由于 stack_pointer 容易被各个地方染指，所以函数 call_function 结束后，需要恢复原来的位置，把函数产生的变量忽略不计，比较奇怪的是为啥不是

```
PyObject **sp, **remember_sp;
sp = remember_sp = stack_pointer;
...
res = call_function(tstate, &sp, oparg, NULL);
stack_pointer = remember_sp;    // reset sp
```

看了一下 call_function 的实现：

```
Py_LOCAL_INLINE(PyObject *) _Py_HOT_FUNCTION
call_function(PyThreadState *tstate, PyObject ***pp_stack, Py_ssize_t oparg, PyObject *kwnames)
{
  PyObject **pfunc = (*pp_stack) - oparg - 1;

  /* call function implementations */

  /* Clear the stack of the function object. */
  while ((*pp_stack) > pfunc) {
      w = EXT_POP(*pp_stack);  // (*--pp_stack)
      Py_DECREF(w);
  }
  ...
}
```

所以 区别在他信任 call_function 会把 sp 指针地址回复原状，而且多了一步 **DEC**rease **REF**erence 的操作，回收不再使用的 PyObject

(所以 Python 的回收机制是引用计数 O.O)  [Include/object.h](https://github.com/python/cpython/blob/c5cb077ab3c88394b7ac8ed4e746bd31b53e39f1/Include/object.h#L422-L451) 有几个时机可以注意一下 ref 值：退出作用域，作为参数传递给函数

跑题了，看一个各个指令做的事情：

| Instruction       | comment                                                      |
| ----------------- | ------------------------------------------------------------ |
| SETUP_WITH        | 查找 `__enter__` `__exit__`；  将原来的栈顶替换成 exit 对象；调用 `__enter__`； SETUP_FINNALLY 绑定当前栈顶元素（exit 对象）；将`__enter__`调用结果压栈， |
| POP_TOP           | 去除 stack_pointer 栈顶元素，并 De-ref 之，ref-count 为 0 则回收 |
| WITH_EXCEPT_START | 运行 exit function，exit function 处于栈的往下第七个元素 0.0 |

感觉这个"第七个元素" 就是 SETUP_WITH 塞入的 `__exit__` 函数，中间那么多调用，感觉像是走钢丝，万一弄错一步。。。强耦合。

`compiler_with` 函数虽然有些细节还需考究，但是大体会运行什么我们也知道了，也回答了我们第二个问题：

* with 会调用对象的 `__enter__`(开始时) 以及 `__exit__` 函数（结束时）

验证一下：

```
>>> f = open('Makefile', 'r')
>>> print(f)
<_io.TextIOWrapper name='Makefile' mode='r' encoding='UTF-8'>
>>> help(f)
 |  Methods inherited from _IOBase:
 |  
 |  __del__(...)
 |  
 |  __enter__(...)
 |  
 |  __exit__(...)


// 查看了一下 python 代码：
_pyio.py  class IOBase:
    def __enter__(self):  # That's a forward reference
        """Context management protocol.  Returns self (an instance of IOBase)."""
        self._checkClosed()
        return self

    def __exit__(self, *args):
        """Context management protocol.  Calls close()"""
        self.close()
```

随便写段 code 验证一下：

```
class juzhenTest:
    def __enter__(self):
        print("I am enter = =")
    def __exit__(self, *args):
        print("I am exit 0v0")

with juzhenTest() as h:
    print("Hello world !")
    

===== 输出结果 ====
I am enter = =
Hello world !
I am exit 0v0
```

哈哈 第二问搞定。

至于第一问，埋坑不填 开溜，召唤师峡谷等着我。