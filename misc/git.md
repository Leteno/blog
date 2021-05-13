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

文档是很好的工具，不过我浏览了一下 README 并没有只是知道了大概，像上述方法描述，debug 一下 code：

发现有一处 setup_path:

```c
// void setup_path(void) 
const char *exec_path = git_exec_path();  // home/juzhen/libexec/git-core    git --exec-path 可得
const char *old_path = getenv("PATH");
git_set_exec_path(exec_path);
add_path(&new_path, exec_path);
if (old_path)                                                                                                         strbuf_addstr(&new_path, old_path);
setenv("PATH", new_path.buf, 1);
```

其中 git-core 内含各种 git 命令：

```shell
juzhen@MININT-PTJ3NTI:/mnt/c/Users/juzhen/Workspace/code/git/git$ ls ~/libexec/git-core/
git                   git-commit-tree               git-fsck-objects        git-merge-one-file           git-reflog             git-sparse-checkout   
git-add               git-config                    git-gc                  git-merge-ours               git-remote             git-stage
git-add--interactive  git-count-objects             git-get-tar-commit-id   git-merge-recursive          git-remote-ext         git-stash
git-am                git-credential                git-grep                git-merge-resolve            git-remote-fd          git-status
git-annotate          git-credential-cache          git-gui                 git-merge-subtree            git-remote-ftp         git-stripspace        
git-apply             git-credential-cache--daemon  git-gui--askpass        git-merge-tree               git-remote-ftps        git-submodule
git-archimport        git-credential-store          git-hash-object         git-mergetool                git-remote-http        git-submodule--helper 
git-archive           git-cvsexportcommit           git-help                git-mergetool--lib           git-remote-https       git-svn
git-bisect            git-cvsimport                 git-http-backend        git-mktag                    git-repack             git-switch
```

盲猜 git log 最后选中 git-log, 那么 比如 git log -n 1 是怎么选中 git-log ? 中间的规则是怎么一回事呢？

```c
// git.c
int run_argv()
  handle_builtin(argv)  // if could be handled by builtin, will exit.
  ...
  /* try external one*/
  execv_dashed_external(argv)

commands = {
  {"add", cmd_add, RUN_SETUP | NEED_WORK_TREE},
  ...
  {"log", cmd_log, RUN_SETUP},
  ...
};
handle_builtin(char* name):  // name: such as "log"
  auto command = find_if(begin(commands), end(commands), [](command c) { return str-equal(name, c->name) });
  if (command != end(commands)) {
      // found
      exit(run_command(command));
  }
cmd_log():
  ...
  // log.c
  log_tree_commit(rev, commit)
```

可以看到 一开始会交给内置命令处理，不行再用外置的，内置命令的 map 在 git.c `commands` 之中。执行内置命令结束后，会直接退出。

我们在深入了解 handle_builtin 之前，我们先看一下 external 部分怎么做的（ms 这边有  git ms xxx 命令，用于日常开发，我们都想知道这块怎么做的，估计逻辑就在 external 部分）

```c
execv_dashed_external(argv):
  cmd = "git-" + argv[0] + rest_of_args(argv+1) // such as git-ms format
  run_command(cmd)

run_command(cmd):
  // 会 fork 一个进程出来运行 cmd
  // 执行函数是：execve，也就是会从 PATH 中找寻
```

所以我们 `git xx` 等外置命令，其实就是需在 PATH 中有 `git-xx` 这么一个命令即可, git 会帮我们 handle 整个匹配过程。

试验一下：

```shell
// git-hello
#!/bin/bash
echo "Hello juzhen"
```

```shell
// output:
juzhen@juzhen-ubuntu18:~/Workspace/code/git/git$ git-hello
Hello juzhen
juzhen@juzhen-ubuntu18:~/Workspace/code/git/git$ git hello
Hello juzhen
```

回过头来看 handle_builtin 的逻辑。

如上文，我们有一个映射表，如：`add` 对应 `cmd_add`, `log` 对应 `cmd_log`

以 cmd_add 为例，它的实现位于 buildin/add.c 中，而这个跟我们之前顾忌的 `git-add` 有什么关系呢？

看了一下 Makefile:

```makefile
BUILTIN_OBJS += builtin/add.o
BUILTIN_OBJS += builtin/am.o
...
BUILTIN_OBJS += builtin/log.o
...
# List built-in command $C whose implementation cmd_$C() is not in
# builtin/$C.o but is linked in as part of some other command.
BUILT_INS += $(patsubst builtin/%.o,git-%$X,$(BUILTIN_OBJS))

$(BUILT_INS): git$X
	$(QUIET_BUILT_IN)$(RM) $@ && \
	ln $< $@ 2>/dev/null || \
	ln -s $< $@ 2>/dev/null || \
	cp $< $@
```

其中各个符号的含义：

```
all: library.cpp main.cpp
---  -----------
$@       $<
     --------------------
              $^
```

本地跑一下 `make -n` 命令，  也可以输出一下 build 一个 `git-log` 需要做什么：

```
juzhen@MININT-PTJ3NTI:/mnt/c/Users/juzhen/Workspace/code/git/git$ make -n git-log
...
ln git git-log 2>/dev/null || \
ln -s git git-log 2>/dev/null || \
cp git git-log
```

 好奇查了一下  md5:

```
juzhen@MININT-PTJ3NTI:/mnt/c/Users/juzhen/Workspace/code/git/git$ md5sum git
6ec8c6cd0298d626bb8c2a956fa58d69  git
juzhen@MININT-PTJ3NTI:/mnt/c/Users/juzhen/Workspace/code/git/git$ md5sum git-add
6ec8c6cd0298d626bb8c2a956fa58d69  git-add
juzhen@MININT-PTJ3NTI:/mnt/c/Users/juzhen/Workspace/code/git/git$ md5sum git-log
6ec8c6cd0298d626bb8c2a956fa58d69  git-log
```

上文提到的猜测：`git log` 会使用 `git-log`，其实应该是否命题。

* 没必要，由于 `git = git-log`, 再转一次，其实相当于再套娃一层，显得多此一举

不过还是以 code 服人，关键就在于 `cmd_log` 的实现具体是怎么一回事：

我其实在之前的试验中尝试 debug，无奈 gdb 过程中遇到这样的错误：

```
Program received signal SIGTTOU, Stopped (tty output).
```

而我尝试使用网上的做法关闭 tty 的配置： `stty -tostop` 无果。

而在看 cmd_log 的实现在 log-tree.c 中，我有预感，我们文章开头谈及的 tree blob 会在下面揭开面纱。

### 曙光？

查阅了资料，signal 可以让程序处理，也可以让 terminal 自行处理。而当我这么做就好了：

```shell
(gdb) handle SIGTTOU nostop
Signal        Stop      Print   Pass to program Description
SIGTTOU       No        Yes     Yes             Stopped (tty output)
(gdb) handle SIGTTOU ignore
Signal        Stop      Print   Pass to program Description
SIGTTOU       No        Yes     No              Stopped (tty output)
```

这样 遇到 SIGTTOU，我们 (gdb) 也不处理，也不 stop. 可以继续 debug。

退出 gdb 之后，还会被卡住，是由于我们都不管 SIGTTOU，丢给 terminal 管，此时只要在这之前设置好 nostop（-tostop） 就好：

```shell
> stty -tostop
```



