# CSP in Extension

最近，被一个关于 CSP 在 Extension 到底怎么工作的问题问倒了，当时意识到自己过于想当然了，原来关于 Extension 怎么处理 CSP 的细节 remain mystery to me。比如：

* CSP 最小 apply 单位是什么。是 WebContent，IsolatedWorld, 抑或是其他的。
* CSP 使用的 timing 是什么。对于 Extension，是否存在某个时机，你可以提前改写 bypass CSP。

I need further investigation. And our journey begins.

[TOC]

## Start from error message

> Refused to execute inline script because it violates the following Content Security Policy directive: "script-src 'self' 'wasm-unsafe-eval'". Either the 'unsafe-inline' keyword, a hash ('sha256-5jFwrAK0UV47oFbVg/iCCBbxD8X1w+QvoOUepu4C2YA='), or a nonce ('nonce-...') is required to enable inline execution.

上面这个语句则是我在 Extension manifest v3 的情况下，往 DOM 注入 `<script>` 引发的错误。

通过搜索 "Refused to ..." 字样，并不能找到使用者，反而是搜索 `script-src 'self' 'wasm-unsafe-eval'` 了解到一些端倪。

```c++
//extensions/common/manifest_handlers/csp_info.cc
// The default CSP to be used if no CSP provided.
static const char kDefaultMV3CSP[] = "script-src 'self'; object-src 'self';";
// The minimum CSP to be used in order to prevent remote scripts.
static const char kMinimumMV3CSP[] =
    "script-src 'self' 'wasm-unsafe-eval'; object-src 'self';";
const char kDefaultSandboxedPageContentSecurityPolicy[] =
    "sandbox allow-scripts allow-forms allow-popups allow-modals; "
    "script-src 'self' 'unsafe-inline' 'unsafe-eval'; child-src 'self';";
const char kDefaultContentSecurityPolicy[] =
    "script-src 'self' blob: filesystem:; "
    "object-src 'self' blob: filesystem:;";

const char* GetDefaultExtensionPagesCSP(Extension* extension) {
  if (extension->manifest_version() >= 3)
    return kDefaultMV3CSP;
  if (extension->GetType() == Manifest::TYPE_PLATFORM_APP)
    return kDefaultPlatformAppContentSecurityPolicy;
  return kDefaultContentSecurityPolicy;
}
```

实际上，除了上述的 `GetDefaultExtensionPagesCSP` 方法之外，还有 `GetExtensionPagesCSP` `GetSandboxContentSecurityPolicy` `GetIsolatedWorldCSP` 等等方法，其调用者又五花八门，容易被整晕。

对此，我想了一个折子。

既然是 CSP 规则阻止了 `<script>` 的注入，实际上，如果你成功改写 CSP 规则，比如添加一个 `'unsafe-inline'` 的话，是能够 bypass `<script>` 注入的。

所以我需要逐一修改上面提到的方法，往返回值中加入 `'unsafe-inline'`, 再测试的 manifest v3 Extension 能否注入 `<script>`，只要能成功注入, 这不就意味着此时的方法就是负责 CSP 规则的吗？

通过以上方法，很快定位到其函数为： `GetIsolatedWorldCSP`

```
// static
const std::string* CSPInfo::GetIsolatedWorldCSP(const Extension& extension) {
  if (extension.manifest_version() >= 3) {
    // The isolated world will use its own CSP which blocks remotely hosted
    // code.
    static const base::NoDestructor<std::string> default_isolated_world_csp(
        kMinimumMV3CSP);
    return default_isolated_world_csp.get();
  }

  Manifest::Type type = extension.GetType();
  bool bypass_main_world_csp = type == Manifest::TYPE_PLATFORM_APP ||
                               type == Manifest::TYPE_EXTENSION ||
                               type == Manifest::TYPE_LEGACY_PACKAGED_APP;
  if (!bypass_main_world_csp) {
    // The isolated world will use the main world CSP.
    return nullptr;
  }

  // The isolated world will bypass the main world CSP.
  return &base::EmptyString();
}
```

很有趣的实现，可以看到 如果是 v3, 则恒为 kMinimumMV3CSP. 否则，则为空。

好，我们继续看是最后谁用了它。

## ScriptInjection::Inject

通过 cs.chromium.org 得知，最终实现在 script_injection.cc 中

```
//extensions/renderer/script_injection.cc
int GetIsolatedWorldIdForInstance(const InjectionHost* injection_host) {
  ...
  const std::string* csp = injection_host->GetContentSecurityPolicy();
  if (csp)
    info.content_security_policy = blink::WebString::FromUTF8(*csp);

  // Even though there may be an existing world for this |injection_host|'s key,
  // the properties may have changed (e.g. due to an extension update).
  // Overwrite any existing entries.
  blink::SetIsolatedWorldInfo(id, info);

  return id;
}
```

可以看到 CSP 是赋予给 IsolatedWorld 的。

再往上，可以看到其最终调用时机为 ScriptInjection::Inject 中。

Extension Script Inject 之前，需要 Get/Create IsoatedWorld, 从而调用了 GetIsolatedWorldIdForInstance.

所以 CSP apply 的时机完全取决于 Script Inject 的时机。

从 [README.md](./README.md) 可知，Script Inject 的时机为：RenderFrameObserver::DidCreateDocumentElement, 或者 DidDispatchDOMContentLoadedEvent.

至此，我们了解了 Extension CSP apply 的最小单位 -- IsolatedWorld 以及 apply 的时机 DidCreateDocumentElement 或 DidDispatchDOMContentLoadedEvent.

## 结语

CSP 是一个神奇的方法，他能让一些常见的 XSS 攻击失效，保护你我。

Extension 中的 CSP 规则，据现在的 code 而言，v3 固定为 kMinimumMV3CSP，v2 则恒为空。你无法改变之。

这么一说，我还挺期待看看这些在 v2 上面各种魔改 往 MainWorld 中注入 script 的 Extension, 在 2023 年，会不会找出新漏洞，继续注入 script。



TODO 有空看看 `blink::SetIsolatedWorldInfo` 的实现。



## 参考

https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Security-Policy/script-src

https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Security-Policy/Sources#sources

