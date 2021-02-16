# Get Start

Boost 是一套很优秀的 C++ 库，值得学习。

## 编译

boost 可以在 github 上下载：https://github.com/boostorg/boost , 其 wiki 上面也描述了如何 下载并编译：

```
git clone --recursive https://github.com/boostorg/boost.git
cd boost
git checkout develop # or whatever branch you want to use
./bootstrap.sh
./b2 headers
```

其中  `--recursive` 是因为 boost 还有很多子模块，需要一并 clone 下来，忘记加上的话，可以使用 `git submodule update` 下载子模块。  [boostorg/boost/.gitmodules](https://github.com/boostorg/boost/blob/master/.gitmodules)

## 使用

编译完会在根目录下出现：`./boost` 目录，像我们平时用到的 `#include <boost/shared_ptr.hpp>` 可以在 `./boost/shared_ptr.hpp` 找到。所以我们使用 boost 库可以直接 include 项目根目录，像这样：

```
g++ -o a.out HelloWorld.cpp -I~/Workspace/code/boostorg/boost/
```

搞起