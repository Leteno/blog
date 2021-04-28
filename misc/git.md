## git

近期，有个印度老哥讲了 git fundamental, 觉得挺有意思的。他讲了 .git/objects 里面这几种类型之间的关系:

* commit
* tree
* blob

以及 `git cat-file` 这个命令。我确实是想知道 `cherry-pick` 是怎么 patch 上去的，我们的 diff，是怎么做的之类的。

### Start up

第一步是下 git 源码以及 build:

```shell
git clone https://github.com/git/git.git
cd git/
make
make install
```

我是用 wsl，make 时候新装了：

```shell
sudo apt update
sudo apt install libcurl-openssl1.0-dev zlib1g-dev gettext
```

最后怎么让命令行用上自己 build 的 git 命令：

```shell
# 修改 ~/.bashrc
# 在末尾加上:   (注：make install 会安装至 ~/bin/ )
export PATH=~/bin/:$PATH
```

使用 gdb，就可以 debug (debug 是一个看代码的工具)

```shell
> gdb git
(gdb) l
--- common-main.c -----
(gdb) b 30
# add break point in L30  common-main.c
(gdb) r log -n 1
# r(un) arg1 arg2 ... => git log -n 1
```

### 天地茫茫何所往

