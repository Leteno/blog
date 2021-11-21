

> Organize the program, expecially the large one, we need to break it up into small piecies that we could manage correctly. Often we use two technics: Abstraction and "Divide and conqure"

1000 行的代码，比 10 个 100 行的代码更容易出错。所以切分是程序数量级较大时必然的选项。可以用以下技术帮助你切分：抽象，以及分治。

通常，如果一件事可以分成很多步骤，此时可以用分治，譬如:

```c
void GoToSchool() {
    GetPackage();
    WellDress();
    TakeBus();
}
void GetPackage() {
    GetMeal();
    GetBook();
    ForgetTestBook();
}
...
```

这肯定比下面的显得清晰:

```c
void GoToSchool() {
    GetMeal();
    GetBook();
    ForgetTestBook();
    DressCloth();
    TakeShoes();
    GetOnBus();
    GetOffBus();
    ...
}
```

抽象，则是将多个相似的事物抽离其共同点，共用一套处理方法。

关于分治，我觉得测试很重要，如果每个模块都加了合理的测试，出问题的时候，我们也可以快速定位。尤其是增加新功能的时候，能不能 pass 过去的测试，是一个很重要的衡量新功能对旧有框架的冲击。



> lvalue vs rvalue

本意是在赋值/初始化语句中用到的 = 号，= 号左边的是 lvalue, = 号右边的是 rvalue.

引申为 lvalue 能修改，rvalue 只用于读取



> type(val) vs type{val}

如果你是 `int a = 3.14`, 这种叫 narrowing conversion，一般编译器会报错，而 c++11 提供了 `type{val}`, 如果你写 `int a{3.14};` 编译器会报错，以此来防止 narrowing conversion.



> listen to compiler's warning and advise

是的，毕竟他们是老手，我们应该多听听他们的意见。Don't insist.



