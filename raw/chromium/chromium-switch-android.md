## Chromium Android 设置 Switch 参数



chromium 有些功能可以通过打开开关使用 [command_line.cc - Chromium Code Search](https://source.chromium.org/chromium/chromium/src/+/main:base/command_line.cc;l=1?q=command_line.cc&sq=&ss=chromium)

而 switch 各个平台的打开方式可以参考

[Run Chromium with flags - The Chromium Projects](https://www.chromium.org/developers/how-tos/run-chromium-with-flags)



（Android）如果你发现这上面的方法不 work，请往下看。

#### 非 Rooted device，且 chrome://flags 里面不含 "Enable command line on non-rooted devices"

其实这部分代码在：[CommandLineInitUtil.java - Chromium Code Search](https://source.chromium.org/chromium/chromium/src/+/main:base/android/java/src/org/chromium/base/CommandLineInitUtil.java;l=1?q=commandLineInitUtil.java&ss=chromium)

```java
public static void initCommandLine(
    String fileName, @Nullable Supplier<Boolean> shouldUseDebugFlags) {
    assert !CommandLine.isInitialized();
    File commandLineFile = new File(COMMAND_LINE_FILE_PATH_DEBUG_APP, fileName);
    // shouldUseDebugCommandLine() uses IPC, so don't bother calling it if no flags file exists.
    boolean debugFlagsExist = commandLineFile.exists();
    if (!debugFlagsExist || !shouldUseDebugCommandLine(shouldUseDebugFlags)) {
        commandLineFile = new File(COMMAND_LINE_FILE_PATH, fileName);
    }
    CommandLine.initFromFile(commandLineFile.getPath());
}
```

这里面有俩个文件

* COMMAND_LINE_FILE_PATH_DEBUG_APP, filename
  * 通常为 `/data/local/tmp/chrome-command-line`
* COMMAND_LINE_FILE_PATH, filename
  * 通常为 `/data/local/chrome-command-line`


区别是上一个可以随意更改，如:

`adb shell 'echo --xxx-switch=whatever > /data/local/tmp/chrome-command-line'`

而下一个是不能随意更改的（由于非 root）

为了能够让 command-line 使用我们能更改的文件，我们需要尽可能的使得该语句不被调用：

`commandLineFile = new File(COMMAND_LINE_FILE_PATH, fileName);`

可以看 code 得知，需要：

* 开启 developer option
* 设置该 app 为 debug app （也在 developer option 这一页）



### 其他

#### 关于 chrome_public_apk argv 执行了什么

参考：

* [apk_operations.py - Chromium Code Search](https://source.chromium.org/chromium/chromium/src/+/main:build/android/apk_operations.py;l=643?q=apk_operations.py&sq=&ss=chromium): _ArgvCommand
* [flag_changer.py - Chromium Code Search](https://source.chromium.org/chromium/chromium/src/+/main:third_party/catapult/devil/devil/android/flag_changer.py;l=1?q=flag_changer.py&sq=&ss=chromium): ReplaceFlags

如果 argv 后面没有参数，则 DisplayArgs, 否则执行 ReplaceFlags.

其内容是直接修改 `/data/local/{tmp/}chrome-command-line`

等价于 `adb shell 'echo --some-switch=abc > /data/local{tmp/}chrome-command-line'`

所以 如果你是 非 root device，argv 不能帮到你什么，你还是需要开启 chrome://flags `Enable command line on non-rooted devices`