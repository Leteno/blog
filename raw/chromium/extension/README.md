[TOC]

# Chromium Extension 101

你可能用过 Chrome Extension, 你会很好奇插件是怎么写的。其实，如果你了解 HTML 怎么写，javascript 怎么写，你已经就知道怎么去写插件了。你额外需要的只是一点点的配置。

## 配置

Chrome Extension 的配置文件是: `manifest.json`, 以 [Leteno/TranslatorExtension: A browser extension to translate English to Chinese (github.com)](https://github.com/Leteno/TranslatorExtension) 为例，长这样：

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
        "content_scripts/bing_api_parser.css",
        "content_scripts/main.css"
      ],
      "js": [
        "content_scripts/bing_api_parser.js",
        "content_scripts/main.js"
      ]
    }
  ],
  "background": {
    "service_worker": "background_scripts/background.js"
  }
}
```

像 `name` `version` 之类的基础配置名称，无需多讲。

你会发现 `action` 内配置了头像图标之类的，还设置了 html 文件。

`content_scripts` 里面还设置了 `js` `css`, 以及似乎是启动插件的条件`matches`

`background` 里面还配置了后台启动应该有的 `service_worker`.

你可能会不满足于这个例子设置的配置。你想知道还有哪些配置，以及可以怎么配，能做什么。你可以去看一下这个地址：[Manifest file format - Chrome Developers](https://developer.chrome.com/docs/extensions/mv3/manifest/)

这上面举了一个 manifest.json 例子，每个配置项都有一个链接，你可以了解到这个配置项是什么，能做什么，有哪些子配置项。

看到这，你或许已经知道了 manifest.json, 你也了解了基本的配置有哪些，有哪些文档可以去看这些配置项的作用以及配法。



## content_scripts

以上面的例子来说，`content_scripts`配置的是会在网页内执行的 script，css, 以及哪些网站受影响。

所以，如果你的 extension 想在网页里面监听某些事件，显示一个额外的文字，插入一个文字框之类的，你只需要往 content_script 里面写 js 即可，这些 javascript 都会在网页内执行。（关于命名重名的担忧大可不必，我估计已经是 Isolated world）

如果你想要例子，可以看这个：[TranslatorExtension/main.js at master · Leteno/TranslatorExtension (github.com)](https://github.com/Leteno/TranslatorExtension/blob/master/src/content_scripts/main.js)

例子里面会有一些跟通讯有关的 code，可以暂时跳过，这里我们列出 content_scripts 插入文本框的例子：

```javascript

var div = document.createElement('div');
div.className = "my_translation_div";
div.style.left = x + "px";
div.style.top = y + "px";
...

var t = document.createElement('p');
t.className = "my_translation_text";
t.innerText = `Translating...`;

div.appendChild(t);
document.documentElement.appendChild(div);
```

其实非常简单，就是 javascript 如何新建 HTML 元素, 怎么放入 DOM tree 中。



## background

`background` 则是配置 Extension 自己的 javascript, 执行于 Extension 自己的空间内。

你或许会很奇怪，`content_scripts` 已经能够在网页内执行 javascript 了，为啥还需要 background 呢？

这是因为网页有可能由于自己的一些设置，对 javascript 某些 api 有所影响，比如 [CORS](https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS) 可能会阻止你从 domain-a.com 获取 domain-b.com 的资源。

以 TranslatorExtension 为例，translate 需要使用 bing.com 的 api，如果你将发送 bing.com/translate 的请求放在 content_script 来做，会使得你在其他网站上面无法使用该插件，请求会失败。所以一旦你有调用 api 的需求，你也不希望被 CORS 所限，你一般会将调 api 的代码，放在 `background` 来做，`content_scripts` 这边只负责跟 `background` 通信，告诉它应该传输什么样的数据，`background` 则负责发送请求，以及在请求结束时，发送数据，告知 content_scripts



## content_scripts 与 background 的通信

具体可以参考官方文档 [Message passing - Chrome Developers](https://developer.chrome.com/docs/extensions/mv3/messaging/)

以 TranslatorExtension 为例，这里面用到了 `Long live connection`. 

```js
// content_scripts

// step 02
var port = chrome.runtime.connect({name: "translator"});

// step 03
port.postMessage({type: 'query', data: escape(`${text}`.trim())});

// step 05
port.onMessage.addListener(function(msg) {
  console.log(`main.js receive msg ${JSON.stringify(msg)}`)
  if (msg.type === "update_div") {
    if (my_translation_active_div) {
      show(my_translation_active_div, msg.data)
    }
  }
});

// background

// step 01
chrome.runtime.onConnect.addListener(function(port) {
  console.assert(port.name === "translator");
  port.onMessage.addListener(function(msg) {
    if (msg.type === "query") {
      queryByBing(msg.data, function(data) {
        // step 04
        port.postMessage({"type": "update_div", "data": data})
      })
    }
  });
});

```



1. `background` 监听 `content_scripts`的 connect 请求

2. 当 `content_scripts` 执行了 connect 请求后，会获取到一个 connect 对象

3. 可以通过 connect.PostMessage 的方式向 `background` 发送信息

4. `background` 接受信息，处理信息后，将结果通过 PostMessage 的方式返回结果

5. `content_scripts` 可以事先在 connect 对象 监听 onMessage 事件，了解返回结果。



## 后语

我希望你已经知道了怎么去配 manifest.json, 也知道怎么往网页内插入控件，为什么要有 background 的 script，以及 content_script 与 background 是怎么通信的。



Enjoy it.

