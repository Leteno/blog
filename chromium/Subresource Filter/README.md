# Subresource Filtering

> Subresource Filtering 是 chromium 屏蔽广告的方案。
>
> 跟他打交道已经有一年多了，想要写点什么，概述一下这个 project

[TOC]

## What

Subresource Filtering 是 chromium 屏蔽广告的方案，它能够有效屏蔽 image iframe 等广告资源。如果你在 console 看到这些字样，全是出自 Subresource Filtering 之手。

```
Chrome blocked resource https://ads.pubmatic.com/AdServer/js/pwt/159928/2891/pwt.js on this site because this site tends to show ads that interrupt, distract, mislead, or prevent user control. Learn more at https://www.chromestatus.com/feature/5738264052891648
```

但是这个方案有些残缺的部分，就是如果你去访问诸如 facebook 之类的网址，你会发现它屏蔽不了这里面的广告。

原因是他只屏蔽网络广告资源 [Resource like image iframe]，但是对于网页元素 [HTML] 本身，是不屏蔽的。

接下来我们将带着俩个问题继续往下看：

* Subresource Filter 怎么屏蔽广告资源
* 不屏蔽的那部分是怎么回事，怎么办

## How - 怎么屏蔽广告资源

屏蔽广告听着玄乎，如果有人告诉你：

* 一个广告名单，所有的广告 url 都在这个名单
* 一个截获网络资源请求的 Listener

估计很多人都可以实现屏蔽效果。

没错，就是：

* 解析名单，得到一个 map 或是 list
* 监听网络请求，每当有网络请求时，query 一下名单，有则扣留，无则放行。

Subresource Filter 额外做的是什么呢？

### 名单解析 + 存储

原始名单来源于：https://easylist.to/easylist/easylist.txt

如何读写这些名单，可以参考这个：[How to write filters | Adblock Plus Help Center (eyeo.com)](https://help.eyeo.com/adblockplus/how-to-write-filters)

而 chromium 的 parse 逻辑，位于 [components/subresource_filter/tools/rule_parser/rule_parser.cc](https://source.chromium.org/chromium/chromium/src/+/main:components/subresource_filter/tools/rule_parser/rule_parser.cc)

存储有什么讲究的呢？

如果有人问你 `"abc"` 在不在 `["a", "abc", "abcd"]` 之中，你可能会很容易得出结论，只要运行一下 `word in array` 即可。

但是如果有人问你 `www.bing.com/hello/world` 是不是命中这些规则之一 `["cn.bing.com/*", "www.bing.com/nohello*", "bing.com*",...]` ，你可能要皱眉头仔细想想，这里面还有正则，怎么低成本地实现一堆正则表达式的匹配呢 @-@...

更有甚者，有些资源在这个网站下是 ads，在别的网站下不是；或者有些规则干脆就是 "bing.com" 下是 ads，在 "bing.com/special" 下 不是。

要做到方便地记录 + 方便地查询，这就麻烦了。

chromium 怎么做的呢？

首先，存储形式选用 protobuf，空间极度节约，没毛病。

其次，对于有些网站是 ads，有些网站则不是的例子，干脆设立 `allow_list`_, `block_list_` 分开存，查询时只需先查在不在 allowlist, 不在再往下查 blocklist, 这样每次查询简单许多。

最后，对于需要对一个 url 进行一堆正则匹配的问题，用的是 N-Gram 算法，N = 5, seperator 为 `*|^`, 即

* 一开始有个 map <特征值，array>
* 每个 pattern string：
  * 每次读取 5 个 char，将其逐一放入到 int64 中，作为特征值。这样一个 pattern string 就能得到一个特征值数组
  * 遍历数组内所有特征值
    * 每个分别找到在 map 上对应的 array
    * 哪个特征值的 array size 最小，那个特征值就是最能代表该 pattern（独一无二）
  * 将 pattern string 放入对应的 array 之中
  * 处理下一个 pattern
* 这样能得到稍微平衡，内容平铺的 map
* 将 map 拍平成 `vector<pair<特征值，array>>`，写入 protobuf 中，这样就得到一组组具有相似规则的 array 了。
* 那好，那怎么查询呢？
  * 对于一个 query string，首先根据上文的方法得到一个特征值数组
  * 对于每个特征值
    * 找到对应的 `pair<特征值,array>`, 遍历 array 中的 pattern
      * 对于 pattern 跟 query 的比对，有能全文的就全文，不行再用正则解析再匹配
      * 看似还是 heavy work，实则已经有巨大优化了，原本rule 的总个数高达 28108，每条都正则匹配那不得猴年马月。能通过特征值缩小几个数量级的计算量，已经很不错了（

这就是 N-gram match 的算法，他是通过提取特征值的方式，减少 regex match 计算量。

这些代码都在： [components/url_pattern_index/url_pattern_index.cc](https://source.chromium.org/chromium/chromium/src/+/main:components/url_pattern_index/url_pattern_index.cc)

### 监测网络请求

我们现在有个 dict 可以随意查询某 url 是不是 ads 了，下一步就是监听了。

在讲 Chromium 的做法前，先说说常规操作。

Chromium 暴露了一些 throttle，方便我们去做一些监听/终止网络请求等操作，（而不要随便直接改动他们网络库的代码（逃），比如 `URLLoaderThrottle`。

常规的操作是：

* 派生 URLLoaderThrottle 的子类，注册到 chrome/browser/chrome_content_browser_client.cc 之中
* 重载其 `WillStartRequest` 方法，`defer` 之，期间询问 ads dict 看是不是 ads，再决定是 `Resume`, 还是 `Cancel`.

这个有什么弊端呢？

实话说，我还觉得这个挺干净的（比起下文提到的方法）至少并没有污染多少 chromium 代码。Throttle 独立于 chromium 之外，想要就注册，不想要就不注册，都是一句代码可以控制的，很干净。

挑刺地说，只能说目前注册过的 Throttle 有很多，假设有一个请求过来，很多 throttle 都在跑，最终由于我们判断是 ads，请求终止，其他 throttle 的工作白跑了。所以自然就会有将检测 ads 的工作提到前面做的想法，也就是下面 chromium Subresource filter 的实现。

### 在 blink 层，屏蔽网络请求

具体是怎么阻止 request 发送的呢？

原来我们每一次解析网页后，都会按需请求网页中子资源，比如 image 资源 ，js 资源等（这些就是 **Subresource** Filtering)

这些资源需要发送额外请求，而 Subresource Filter 就是 filter 这些 subresource 请求。

具体代码怎么做的呢？

基础类 base_fetch_context:

```
// third_party/blink/renderer/core/loader/base_fetch_context.cc
absl::optional<ResourceRequestBlockedReason>
BaseFetchContext::CanRequestInternal(                              // <=== 判断是否可以发送 Request
    ResourceType type,
    const ResourceRequest& resource_request,
    const KURL& url,
    const ResourceLoaderOptions& options,
    ReportingDisposition reporting_disposition,
    const absl::optional<ResourceRequest::RedirectInfo>& redirect_info) {
  ...
  if (GetSubresourceFilter()) {
    if (!GetSubresourceFilter()->AllowLoad(url, request_context,   // <=== 此处判断是否为 ads
                                           reporting_disposition)) {
      return ResourceRequestBlockedReason::kSubresourceFilter;
    }
  }
  return absl::nullopt;
}
```

使用 BaseFetchContext 的有谁呢？[ScriptResource](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/loader/resource/script_resource.cc), [CSSStyleSheetResource](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.cc), [ImageResource](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/loader/resource/image_resource.cc)... 基本上涵盖了大部分网页资源，甚至连 websocket 也不放过：

```
bool WebSocketChannelImpl::ShouldDisallowConnection(const KURL& url) {
  SubresourceFilter* subresource_filter =
      GetBaseFetchContext()->GetSubresourceFilter();
  if (!subresource_filter)
    return false;
  return !subresource_filter->AllowWebSocketConnection(url);
}
```

（不过比较震惊的是，都已经写这么细了，为什么 iframe 资源却没在 blink 处拦截，而是在 browser 层拦的。。。[child_frame_navigation_filtering_throttle.cc - Chromium Code Search](https://source.chromium.org/chromium/chromium/src/+/main:components/subresource_filter/content/browser/child_frame_navigation_filtering_throttle.cc)， 可能在 blink 眼里，iframe 不属于 subresource 范畴）

由此也可见，这个代码渗透的区域过于广，到处都有改动，与此相对的 throttle 技术，真是侵入性低，干净无痕。

## What's missing ?

代码都写到这份上了，还有什么缺失的呢？

上文提到的屏蔽名单 https://easylist.to/easylist/easylist.txt 很大，而上文提到的技术，只使用了其二分之一内容.(28108 vs 27403)，另外一部分是什么呢？

举个例子

```
<div id="ads" style="width:400px; height:400px;">
  <img src="ads.com/good_shoe.png" />
</div>
```

我们可以用上文提到的技术，屏蔽 img 资源的加载，但是你会发现即使图片不加载，网页还是空出一块区域，原因是 div 写死了 width 以及 height。怎么办？

如果我们可以注入这样的 css：

```
#ads { display: none !important; }
```

这个 div 不就消失了吗？

这个其实就是 subresource filter 缺失的部分了（那部分也叫 CSSRule）这部分规则占了 1/2, 缺失实现确实很遗憾。

可以从他的 code 可以看出，其实他们是打算加的，有不少 TODO，但是 2016 年至今，仍未推动，估计是有某些其他原因不能实现。

笔者虽然完善过其实现，出于工作原因，不能透露太多。只能大概说一下工作方向：

* 需要 parser 中的 CSSRules 那部分的逻辑
  * 解析 easylist 部分，上游已有半成品，但需要补充将 CSSRule 保存成原始 protobuf 格式的逻辑，
* 存储与读取
  * 修改 indexed_ruleset, protobuf 格式，添加 CSSRule 部分
  * 仿照 components/url_pattern_index/url_pattern_index.cc 中的 Indexer 和 Matcher，编写在新 protobuf 文件中写入/读取 CSSRule 的逻辑。
* 使用 WebContentObserver，在合适的时机，读取 CSSRule，将其转成 css stylesheet 并注入。
  * 现有的 api 可能只能注入 js，如果将 css 写成 js 再解析，恐怕过于费时
  * 可以考虑从 blink 层开一个 api 给 RenderFrameHost，方便我们注入 css stylesheet。

## 后话

Subresource Filtering 是 chromium 的功能，其在功能实现上使用侵入性极强的实现方式，令笔者大为震撼，也从中受益匪浅。

即使有所缺失，但其功能已经能很好地阻止那些 stiky annoying ads 了。

希望有所帮助，欢迎点赞。转发请标记出处。 Thanks for listening.