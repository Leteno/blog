[TOC]

# Overview

[overview.md - Chromium Code Search](https://source.chromium.org/chromium/chromium/src/+/main:extensions/docs/overview.md)

## Glossary

* **Extension**: a bundle of HTML, CSS, and Javascript that can interact with
  web contents and some parts of the browser UI. There are many types of
  extensions; see [extension types] for more details. Each extension has an
  **id**, which is a unique string identifying that extension.
* **Manifest**: a JSON file stored inside the extension bundle which describes
  the extension, the capabilities it needs, and many other things about it. The
  [manifest file format] doc gives a user-facing overview.
* **Action**: an action is one of the ways an extension can expose
  functionality to a user. There are three types: [page actions],
  [browser actions], and regular actions. Actions add buttons to the toolbar or
  to the extension menu.
* **Permission**: the ability of an extension to access a specific API. Most
  things extensions can do are controlled by permissions. See [permissions]
  for more details.
* **Extension renderer**: because extensions are logically their own web
  environments, each extension may have a renderer process that hosts its
  content. These renderers are annotated in the task manager as
  "Extension: Name".
* **Component extension**: an extension that is part of the browser. Component
  extensions are not user-visible, generally can't be configured or disabled,
  and may have special permissions. A component extension is so named because it
  is logically a *component of the browser* that happens to be an extension.



## Life cycle of Extension Function

Extension Function is ref-counted, and will not be collected even the related renderer or associated profile is shutdown.

Be careful as it could outlive object they depends on, such as renderer or profile.



## Manifest Handler

chromium register several manifest handlers in [chrome/common/extensions/chrome_manifest_handlers.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/common/extensions/chrome_manifest_handlers.cc;l=47;drc=d36aa0494c1d0d5238eb4fd0e3c7db748d95fcbc)

```c++
void RegisterChromeManifestHandlers() {
  // TODO(devlin): Pass in |registry| rather than Get()ing it.
  ManifestHandlerRegistry* registry = ManifestHandlerRegistry::Get();

  DCHECK(!ManifestHandler::IsRegistrationFinalized());

  registry->RegisterHandler(std::make_unique<AboutPageHandler>());
  registry->RegisterHandler(std::make_unique<AppIsolationHandler>());
  registry->RegisterHandler(std::make_unique<AppLaunchManifestHandler>());
  registry->RegisterHandler(std::make_unique<DevToolsPageHandler>());
  registry->RegisterHandler(std::make_unique<HomepageURLHandler>());
  registry->RegisterHandler(std::make_unique<LinkedAppIconsHandler>());
  registry->RegisterHandler(std::make_unique<MinimumChromeVersionChecker>());
  registry->RegisterHandler(std::make_unique<NativelyConnectableHandler>());
  registry->RegisterHandler(std::make_unique<OmniboxHandler>());
  registry->RegisterHandler(std::make_unique<OptionsPageManifestHandler>());
  registry->RegisterHandler(std::make_unique<SettingsOverridesHandler>());
  registry->RegisterHandler(std::make_unique<SidePanelManifestHandler>());
  registry->RegisterHandler(std::make_unique<StorageSchemaManifestHandler>());
  registry->RegisterHandler(std::make_unique<SystemIndicatorHandler>());
  registry->RegisterHandler(std::make_unique<ThemeHandler>());
  registry->RegisterHandler(std::make_unique<TtsEngineManifestHandler>());
  registry->RegisterHandler(std::make_unique<UrlHandlersParser>());
  registry->RegisterHandler(std::make_unique<URLOverridesHandler>());

  ...
}
```

It looks like the abilities of manifest, such as OmniboxHandler, ability to change Address bar ?



Every ManifestHandler contains Keys, such as `AppLaunchManifestHandler`, contains:

```
app.launch.container
app.launch.local_path
app.launch.web_url
app.launch.width
app.launch.height
```

And that is related to `manifest.json`

```
{
  "app": {
    "launch": {
      "web_url": "www.helloworld.com",
    }
  }
}
```

以上文为例，我们看看这个被 parse 之后 web_url 的走向.

该值将被存至： `launcher_web_url_` 之中。

```c++
URLPattern pattern(Extension::kValidWebExtentSchemes);  // scheme: http | https
pattern.SetHost(launch_web_url_.host());
pattern.SetPath("/*");
extension->AddWebExtentPattern(pattern);
```

可以看出，在 `AppLaunchManifestHandler` 中解析完的数据，将存至 extension 之中(通过 `AddWebExtentPattern`)

最后存放于 [extension.h - Chromium Code Search](https://source.chromium.org/chromium/chromium/src/+/main:extensions/common/extension.h;drc=d36aa0494c1d0d5238eb4fd0e3c7db748d95fcbc;l=423) 之中：

```
class Extension {
  std::string display_name_;
  int manifest_version_;
  base::FilePath path_;
  URLPatternSet extent_;
  std::unique_ptr<PermissionsParser> permissions_parser_;
  std::unique_ptr<PermissionsData> permissions_data_;
  ...
};
```

一堆数据。

不得其道，我还是看一下我的老朋友 `content_scripts`

老朋友的 ManifestHandler 不在 chrome_manifest_handler.cc, 而是在 [extensions/common/common_manifest_handlers.cc](https://source.chromium.org/chromium/chromium/src/+/main:extensions/common/common_manifest_handlers.cc;bpv=1;bpt=1;l=49?q=content_scripts -test -out&ss=chromium%2Fchromium%2Fsrc)

在这个 cc 文件能看到更多的 ManifestHandler 类型，比如 bluetooth，csp 等等。

我们先看 [content_scripts_handler.h - Chromium Code Search](https://source.chromium.org/chromium/chromium/src/+/main:extensions/common/manifest_handlers/content_scripts_handler.h;l=38;drc=d36aa0494c1d0d5238eb4fd0e3c7db748d95fcbc)

ContentScriptsHandler 与 AppLaunchManifestHandler 隶属同源，都是 ManifestHandler 的子类。



* 记得去看看最后 script 是怎么被注入到 WebContents 其中的。



其将 content_scripts 解析成 ContentScriptInfo

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

这就能对上我们之前的配置了：

```
{  // manifest
  "content_scripts": [  // UserScriptList
    {  // UserScript
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
}
```

数据最后是这样传递给上层：

`extension->SetManifestData(ContentScriptsKeys::kContentScripts, std::move(content_scripts_info));`

最后给谁用呢？

最简单的做法自然是看谁在读取 `kContentScripts`, 再顺藤摸瓜，找到目标：

* 嫌疑者一号：

位于该方法内：

```
// chrome/browser/extensions/convert_user_script.cc
root->SetKey(api::content_scripts::ManifestKeys::kContentScripts,
               std::move(content_scripts));
scoped_refptr<Extension> ConvertUserScriptToExtension(
    const base::FilePath& user_script_path,
    const GURL& original_url,
    const base::FilePath& extensions_dir,
    std::u16string* error)
// chrome/browser/extensions/crx_installer.cc
CrxInstaller::ConvertUserScriptOnSharedFileThread()
CrxInstaller::InstallUserScript(const base::FilePath& source_file,
                                     const GURL& download_url)
```

加上 log, 发现 对于 CrxInstaller 而言：

* install_directory_ 为：<user_data>\Default\Extensions，可以去这个地方了解到下载下来的 extension，甚至可以修改之。
  * 比如我本地的 Adblock Plus 插件位置：`C:\Users\<username>\AppData\Local\Microsoft\Edge\User Data\Default\Extensions\padekgcemlokbadohgkifijomclgjgif\2.5.21_0`
* `CrxInstaller::CrxInstaller()` 被调用了，但是 convert_user_script.cc 并没有被调用到，neither in installing from store nor in loading unpack crx.

看一下 CrxInstaller 调用它的地方

不要忘记了 去看 installer 之后，还会发生什么事。



### CrxInstaller

```c++
class CrxInstaller {
  void CrxInstaller::InstallCrx(const base::FilePath& source_file);  <=== download from store
  void CrxInstaller::InstallUnpackedCrx(const std::string& extension_id,
                                      const std::string& public_key,
                                      const base::FilePath& unpacked_dir); <=== Load unpacked
  void CrxInstaller::InstallUserScript(const base::FilePath& source_file,
                                     const GURL& download_url);      <=== mistery
};
```

其中前俩种会分别交给 `SandboxedUnpacker` 的 `StartWithCrx` `StartWithDirectory`

不过这俩者最后殊途同归：`SandboxedUnpacker::Unpack(const base::FilePath& directory)`

他会读取 Manifest 文件，读取完之后，执行 ImageSanitization 操作，最后回到 `CrxInstaller::OnUnpackSuccess`

历经 `CrxInstaller::CheckInstall`(一堆的 check: PolicyCheck, RequirementCheck, BlockListCheck), `CrxInstaller::OnInstallChecksComplete`,  `CrxInstaller::ConfirmInstall` `CrxInstaller::CompleteInstall`

最后调用的方法是：`ExtensionAssetsManager::InstallExtension`

```c++
ExtensionAssetsManager* assets_manager =
    ExtensionAssetsManager::GetInstance();
assets_manager->InstallExtension(
    extension(), unpacked_extension_root_, install_directory_, profile(),
    base::BindOnce(&CrxInstaller::ReloadExtensionAfterInstall, this),
    updates_from_webstore_or_empty_update_url);
```

再往下是 `extensions/common/file_util.cc InstallExtension` 其实就是把 unpack 文件夹下的所有文件都挪到 extension_folder, 也就是上面的参数 `install_directory` 之下。

Install 整个过程，其实就是检查 + 挪文件，不涉及 extension 的注册。

那 Extension 注册执行，估计是跟 install 分开的，只要你在 extension  目录之下，下次程序跑的时候，自然发现到。

还得继续看啊。



# Who in charge of applying extension scripts

我们直接看 install 时候的 installer_directory 是从哪来的

位于 chrome/browser/extensions/extension_system_impl.cc

```
extension_service_ = std::make_unique<ExtensionService>(
    profile_, base::CommandLine::ForCurrentProcess(),
    profile_->GetPath().AppendASCII(extensions::kInstallDirectoryName),
    ExtensionPrefs::Get(profile_), Blocklist::Get(profile_),
    autoupdate_enabled, extensions_enabled, &ready_);
```

漫无目的，直接搜关键字算了，还真找到一丝线索：

* extensions/browser/extension_web_contents_observer.h
* extensions/browser/extension_frame_host.cc
* chrome/browser/extensions/tab_helper.h

关于 extension_web_contents_observer, 有俩个地方：

* 是否存在注入 script 的地方
* ExtensionFrameHost 也在 observer 内 create，到底跟 RFH 有什么关联呢？



extension_web_contents_observer 并没有找到注入 script 直接证据。

又来一个： chrome/browser/extensions/chrome_extension_web_contents_observer.cc  是对 extension_web_contents_observer.cc 的进一步封装，嫌疑目前不高



现在看一下 extension_web_contents_observer 里面创建的神秘的 extension_frame_host.

extensions/browser/extension_frame_host.h

头昏眼花，遇事不决找 metrics：

extensions/common/identifiability_metrics.h  [RecordContentScriptInjection](https://source.chromium.org/chromium/chromium/src/+/main:extensions/common/identifiability_metrics.h;bpv=1;bpt=1;l=38?q=contentscript -test -out&ss=chromium%2Fchromium%2Fsrc&gsn=RecordContentScriptInjection&gs=kythe%3A%2F%2Fchromium.googlesource.com%2Fchromium%2Fsrc%3Flang%3Dc%2B%2B%3Fpath%3Dextensions%2Fcommon%2Fidentifiability_metrics.h%23soKzh1vc6B3r-QUeqiDQBr-fcVpJcv9GWopr0296_jM)

extensions/renderer/script_injection.cc 在这呢。



抓到**调用时机**和最后**注入代码**所用到的函数：

* ScriptInjectionManager::RFOHelper 继承于 RenderFrameObserver, 
  * 监听事件： **DidCreateDocumentElement 以及 DidDispatchDOMContentLoadedEvent**
* 这俩个事件发生时，调用 ScriptInjectionManager::TryToInject
* 最后，在 ScriptInjection::Inject 中注入所有相关的 js css。
  * 注入 script 的方法是 **render_frame_->GetWebFrame()->RequestExecuteScript()**, 
    * 其中 world 默认为 kMainDOMWorldId.
    * 如果配置了 isolated, 则新增一个 isolated id, 存入全局的 map, 与自身的 id 一一对应, 可以看这个了解更多详细事项： [extensions/renderer/script_injection.cc GetIsolatedWorldIdForInstance](https://source.chromium.org/chromium/chromium/src/+/main:extensions/renderer/script_injection.cc;drc=d36aa0494c1d0d5238eb4fd0e3c7db748d95fcbc;bpv=1;bpt=1;l=60?gsn=GetIsolatedWorldIdForInstance&gs=kythe%3A%2F%2Fchromium.googlesource.com%2Fchromium%2Fsrc%3Flang%3Dc%2B%2B%3Fpath%3Dextensions%2Frenderer%2Fscript_injection.cc%23OjCl3jY3PUkHAjrP7eOXGeuWClaf64Zu5nXtCg6ejOI)
  * 而对于 css 的话，有点奇特，他是 [InjectOrRemoveCss()](https://source.chromium.org/chromium/chromium/src/+/main:extensions/renderer/script_injection.cc;drc=d36aa0494c1d0d5238eb4fd0e3c7db748d95fcbc;bpv=1;bpt=1;l=433?gsn=InjectOrRemoveCss&gs=kythe%3A%2F%2Fchromium.googlesource.com%2Fchromium%2Fsrc%3Flang%3Dc%2B%2B%3Fpath%3Dextensions%2Frenderer%2Fscript_injection.h%23A7p1R9YyMX4P-61CTGC-rqnB2GOJ4mC-rP2RFFABOi0&gs=kythe%3A%2F%2Fchromium.googlesource.com%2Fchromium%2Fsrc%3Flang%3Dc%2B%2B%3Fpath%3Dextensions%2Frenderer%2Fscript_injection.cc%23nzop6TaeOoAUrcAmZLYoqY4JHtVzVmzsXEkEghJkvX4).
    * 删除用的是 web_frame->GetDocument().RemoveInsertedStyleSheet()
    * 添加用的是 **web_frame->GetDocument().InsertStyleSheet()**
    * 正常是只有添加，没有删除的。



