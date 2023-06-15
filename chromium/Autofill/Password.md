# 代码流程
![image](https://github.com/Leteno/blog/assets/25839908/0d5932c5-04af-43bc-9423-5023a0d69d7c)

可以看到：
* Chromium 在用户点击 form 到弹出 autofill 经历了多少流程
* Chromium 在用户点击选择 autofill 到 对应的 TextBox 被填充 经历了多少流程

这里面比较重要的信息有俩点

一个是数据的流向，哪些数据是在哪一步创建的，将要传到哪里。

如下图，你可以看出流入的时候，只有 username password, 流出的时候已经有 control_element 了。说明 control_element 这个数据是保存在 renderer 层的 AutofillAgent
里面的，并由函数 FillPasswordSuggestion 中得到。

![image](https://github.com/Leteno/blog/assets/25839908/b0d5a460-b69d-4e35-99de-e5b7e30d075b)

一个是 PasswordAutofillManager 中的 BuildSuggestions, 这块将影响用户 autofill 中看到的内容是什么。理论上可以在上面加入其他内容，比如"点我登陆账号，同步数据"之类的。只要在 PasswordAutofillManager::DidAcceptSuggestion 中二次处理，不让他往下走，去"登陆账号"逻辑即可。

不过我比较感兴趣的是，这块 Suggestions 内容包含什么，不同网站之后是怎么隔离的，友商网站是否可以 share password data，应该是一块可以看看的逻辑。
