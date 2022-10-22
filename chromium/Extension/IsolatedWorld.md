# 绕不过去的 Isolated world

你可曾想过，我装了插件，插件能窃取我什么，能修改我什么，以及不能看 不能修改我什么。

这个问题得看我们插件的脚本，运行在哪里。

插件的脚本 content_script 运行在 Isolated World, 跟我们主网页的运行环境 Main World 之间是不能相互 access 的，所以如果你在主网页里面声明了 auth 变量，content_script 那边是没法 access 到的。

但是，Isolated World 跟 Main World share 同一个 DOM tree

也就是你在 Isolated World 里面往 DOM 里面插入一个新节点，在 Main World 里面同样也会加。

这也是为什么我们经常看到插件能插入网页元素，做一些 fancy 的事情。

但是，如果你真的很想在插件里面，访问到 Main World 里面的 js 数据，修改要执行的 js 方法，那有办法吗？

（Oh, welcome, follow me :-）

AdBlockPlus 的做法，就是通过在 IsolateWorld 里面修改 DOM tree，插入 `<script>` 节点，由于 DOM tree 是 share 的，进而影响 Main World 也就是主网页。

(居然有这种空子，就没人管管吗？)

只能在 Extension v2 可以这样，明年 expire，现在新的插件只能使用 v3，期待看看 AdBlockPlus 的创新。



其实 Chromium 是有这个打算支持 main world 执行 script，不过目前尚未通车：

2022-04-14 [【Extensions】 Add main world injections for dynamic content scripts](https://source.chromium.org/chromium/chromium/src/+/e5ad3451c17b21341b0b9019b074801c44c92c9f)

目前 contents_script 定义了 world 属性, [content_scripts.idl - Chromium Code Search](https://source.chromium.org/chromium/chromium/src/+/main:extensions/common/api/content_scripts.idl;l=1;bpv=1;bpt=0)，有解析，也有后续应该怎么调用。

但是最后没有用上 world 属性，查到的原因是：中间桥梁 content_scripts_handler 并没有把解析出来的 world 数据继续往下传，所以目前是用默认值 kIsolatedWorld 的。

可以关注一下 [extensions/common/manifest_handlers/content_scripts_handler.cc](https://source.chromium.org/chromium/chromium/src/+/main:extensions/common/manifest_handlers/content_scripts_handler.cc)



Chrome 会不会支持 Extension 的 content_script 在 MainWorld 上面运行呢？

个人觉得可能性不大，风险太高，security review 过不去。一旦插件可以随意篡改 main world 的数据，main world 的所有东西，网站内的所有方法，都能泄露给插件，无法无天，风险过高。

所以，Adblock Plus 插件最终只能通过钻空子获取 main world 权限(这点对于视频屏蔽功能很重要)，而 Adblock Plus 最终的归宿，可能是 embeded 到浏览器中。



最后附上对 content_scripts_handler 的小改动，狗尾续貂，支持 manifest.json 中 `content_scripts[0]['world']` 的使用，为了我那小小的 extension 能 work（：

```
diff --git a/extensions/common/manifest_handlers/content_scripts_handler.cc b/extensions/common/manifest_handlers/content_scripts_handler.cc
index ab24478619e06..9b836d9ec6e00 100644
--- a/extensions/common/manifest_handlers/content_scripts_handler.cc
+++ b/extensions/common/manifest_handlers/content_scripts_handler.cc
@@ -33,6 +33,8 @@
 #include "ui/base/l10n/l10n_util.h"
 #include "url/gurl.h"
 
+#include "extensions/common/api/extension_types.h"
+#include "extensions/common/mojom/execution_world.mojom-shared.h"
 namespace extensions {
 
 namespace errors = manifest_errors;
@@ -147,6 +149,11 @@ std::unique_ptr<UserScript> CreateUserScript(
     return nullptr;
   }

+  if (extension->id() == "kj******************jg") {
+    // Only work for my extension
+    result->set_execution_world(content_script.world == extensions::api::extension_types::EXECUTION_WORLD_MAIN ? mojom::ExecutionWorld::kMain : mojom::ExecutionWorld::kIsolated);
+  }
+
   return result;
 }

```





 