# Extension 从解析到调用

Chrome Extension 是一个 Chrome 开放给用户，可以通过注入自制脚本到网页，达到 geek 定制化自己浏览器的作用。

而了解 Chrome 内部是如何解析我们的 Extension 工程以及最后是如何注入我们的 Extension script，将有利于我们了解 Extension 的能力，知道 Extension 什么可为，什么不可为。

本文也划分为三部分，稍微探讨一下 Extension：

* Chrome 是如何解析 Extension 的
* Chrome 是如何注入 Extension 相关的 script 的
* Chrome 具体在哪个时机注入 Extension 脚本以及怎么注入

[TOC]

# Chrome 是如何解析 Extension 的

解析 Extension 的关键在如何解析 manifest.json 文件。

Extension 是通过 manifest.json 关联工程中的所有 script，css 等资源。通过读取 manifest.json, 将其他资源串起来。

下面是一个 manifest.json example：

```json
{
  "manifest_version": 3,
  "name": "Translator",
  "version": "0.0.1",
  "action": {
    "default_icon": {
      "16": "images/Hello_World.png"
    },
    "default_title": "Am I title",
    "default_popup": "popup.html"
  },
  "content_scripts": [
    {
      "matches": [
        "<all_urls>"
      ],
      "css": [
        "content_scripts/x_api_parser.css",
        "content_scripts/main.css"
      ],
      "js": [
        "content_scripts/x_api_parser.js",
        "content_scripts/main.js"
      ]
    }
  ],
  "permissions": [
    "storage"
  ],
  "background": {
    "service_worker": "background_scripts/background.js"
  }
}
```

可以看到这里面描述了一些基本信息，以及关键的 `content_scripts` `background` 等。

而这些信息的读取方法，就藏在 [extensions/common/common_manifest_handlers.cc](https://source.chromium.org/chromium/chromium/src/+/main:extensions/common/common_manifest_handlers.cc) 以及 [chrome/common/extensions/chrome_manifest_handlers.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/common/extensions/chrome_manifest_handlers.cc;l=47;drc=d36aa0494c1d0d5238eb4fd0e3c7db748d95fcbc) 之中。

```
// extensions/common/common_manifest_handlers.cc 部分代码：
void RegisterCommonManifestHandlers() {
  ManifestHandlerRegistry* registry = ManifestHandlerRegistry::Get();
  ...
  registry->RegisterHandler(std::make_unique<ContentScriptsHandler>());
  registry->RegisterHandler(std::make_unique<CSPHandler>());
  registry->RegisterHandler(std::make_unique<IconsHandler>());
  ...
}
```

可以看到，manifest.json 上面的每个内容，都可以找到对应的 ManifestHandler 来处理。

以 `content_scripts` 为例，其处理类为：ContentScriptsHandler

其处理的结果为 ContentScriptInfo, 结构为：

```
// extensions/common/manifest_handlers/content_scripts_handler.h
class ContentScriptInfo {
  UserScriptList content_scripts;
};

// extensions/common/user_script.h
using UserScriptList = std::vector<std::unique_ptr<UserScript>>;
class UserScript {
  FileList js_scripts_;
  FileList css_scripts_;
  bool match_all_frames_ = false;
  mojom::ExecutionWorld execution_world_ = mojom::ExecutionWorld::kIsolated;
};
```

能对的上 manifest.json 中的配置：

```
{  // manifest
  "content_scripts": [  // UserScriptList
    {  // UserScript
      "matches": [
        "<all_urls>"
      ],
      "css": [          // js_scripts_
        "content_scripts/x_api_parser.css",
        "content_scripts/main.css"
      ],
      "js": [           // css_scripts_
        "content_scripts/x_api_parser.js",
        "content_scripts/main.js"
      ]
    }
  ], 
}
```

所以如果我们想知道 manifest.json 里面的某个参数最终怎么使用的，我们完全可以去看看对应的 ManifestHandler。



## CrxInstaller    ----  InstallCrx   InstallUnpackedCrx

当我们在 Extension 页面开了 Developer mode, 我们可以执行 **Install Unpacked**, 装入自己定制的 Extension 工程。

* 他跟 InstallCrx 有什么区别
* 这里面涉及到的检查，代码在何方

### 区别  Install Unpacked  vs  InstallCrx

殊途同归。

* InstallCrx InstallUnpackedCrx 分别将由  `SandboxedUnpacker` 的 `StartWithCrx` `StartWithDirectory` 来处理，不过最后二者调用的方法并无二意，还是 `SandboxedUnpacker::Unpack(const base::FilePath& directory)`.
* 调用过程中 InstallCrx 多了一步 unzip

### 对 extension 工程的检查

历经：

* `CrxInstaller::CheckInstall`(一堆的检查: PolicyCheck, RequirementCheck, BlockListCheck)
* `CrxInstaller::OnInstallChecksComplete`
* `CrxInstaller::ConfirmInstall` (有可能会出权限相关弹窗)
* `CrxInstaller::CompleteInstall`

最后调用的方法是：`ExtensionAssetsManager::InstallExtension`

而 InstallExtension 干的事情，只是将 unpacked 的项目工程代码挪到 extension 目录 `<user_data>\Default\Extensions`，比如某 blocker  extension 的路径为 `C:\Users\<username>\AppData\Local\Microsoft\Edge\User Data\Default\Extensions\padekgcemlokbadohgkifijomclgjgif\2.5.21_0`



# Chrome 注入 Extension 脚本的时机以及方式

了解了时机，我们就可以知道 Extension 束手无策的局限性在哪。

了解了方式，我们就可以知道 Extension 能作用的范围在哪。

注入的代码，位于： [script_injection.cc - Chromium Code Search](https://source.chromium.org/chromium/chromium/src/+/main:extensions/renderer/script_injection.cc;l=171;drc=2a2e871f4736827706d296c374120a23dfa8248f;bpv=1;bpt=1)

抓到**调用时机**和最后**注入代码**所用到的函数：

- ScriptInjectionManager::RFOHelper 继承于 RenderFrameObserver,
  - 监听事件： **DidCreateDocumentElement 以及 DidDispatchDOMContentLoadedEvent**
- 这俩个事件发生时，调用 ScriptInjectionManager::TryToInject
- 最后，在 ScriptInjection::Inject 中注入所有相关的 js css。
  - 注入 script 的方法是 **render_frame_->GetWebFrame()->RequestExecuteScript()**,
    - 其中 world 默认为 kMainDOMWorldId (但实际运行时都是 IsolatedWorld 参考 [绕不过去的 IsolatedWorld](./IsolatedWorld.md)).
    - 如果配置了 isolated, 则新增一个 isolated id, 存入全局的 map, 与自身的 id 一一对应, 可以看这个了解更多详细事项： [extensions/renderer/script_injection.cc GetIsolatedWorldIdForInstance](https://source.chromium.org/chromium/chromium/src/+/main:extensions/renderer/script_injection.cc;drc=d36aa0494c1d0d5238eb4fd0e3c7db748d95fcbc;bpv=1;bpt=1;l=60?gsn=GetIsolatedWorldIdForInstance&gs=kythe%3A%2F%2Fchromium.googlesource.com%2Fchromium%2Fsrc%3Flang%3Dc%2B%2B%3Fpath%3Dextensions%2Frenderer%2Fscript_injection.cc%23OjCl3jY3PUkHAjrP7eOXGeuWClaf64Zu5nXtCg6ejOI)
  - 而对于 css 的话，有点奇特，他是 InjectOrRemoveCss().
    - 删除用的是 web_frame->GetDocument().RemoveInsertedStyleSheet()
    - 添加用的是 **web_frame->GetDocument().InsertStyleSheet()**
    - 正常是只有添加，没有删除的。

所以我们知道了 onDOMcreated 或者 onDomLoaded 之后，才会开始注入 script，而且如果没有特殊配置的话，这个 script 将会进入 isolated 世界，与网页 script 隔绝，隔海相望。

