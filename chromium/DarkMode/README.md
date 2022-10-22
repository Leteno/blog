# Auto Dark Web Content

Chromium 提供了自动黑夜模式的功能。笔者最近需要在 Edge 上实现黑夜模式，了解 Chromium 的机制，可以知道之前的方向是什么，也好了解局限性在哪。本文将讲述 Chromium dark mode 的实现机制，将从源码的角度，从 browser 层一步步推导到最终调用的 blink code。

# Start from content setting

Auto-Dark-Web-Content 在 chromium 里面实现, 由 content-setting 控制。

位于：`components/content_settings/core/common/content_settings_types.h` 中的 `AUTO_DARK_WEB_CONTENT`

谁用它呢？通过 cs.chromium.org 很容易知道：

*  `components/content_settings/core/browser/content_settings_utils.cc` GetRendererContentSettingRules

* `components/content_settings/browser/page_specific_content_settings.cc` WebContentsHandler::ReadyToCommitNavigation

  ```
    mojo::AssociatedRemote<content_settings::mojom::ContentSettingsAgent> agent;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&agent);
    agent->SendRendererContentSettingRules(std::move(rules));
  ```

SendRendererContentSettingRules 是一个 mojo 调用，最后是由 `components/content_settings/renderer/content_settings_agent_impl.cc` 处理

`agent` 会保存传过来的 `rules`，提供 query setting rules 的方法，比如我们关注的：`ContentSettingsAgentImpl::AllowAutoDarkWebContent`

mojo 的实现方在 render 层。

接下来 我们看看在 render 层里，是怎么实现 dark mode 的。

# Render process

上文提到一路从 browser 传递到 render，目前在 ContentSettingsAgentImpl，往后给谁呢？

有意思的是，后续除了 test，居然没有人用到 `AutoDarkWebContent` 这个方法，即没人使用这个值。。。

对比同一层级的 AllowImage 方法,  它被用在 `third_party/blink/renderer/core/html/image_document.cc` 中，判断如果是 off，则直接 skip 掉 request image resource 的代码。



在看看作者的历史记录：

有意思的是：

chrome/browser/chrome_content_browser_client.cc  里面提到：

`web_prefs->force_dark_mode_enabled`

我记得网页有时候会有 light/dark mode, 就像 responsive website 一样，只要浏览器告诉网页是日夜间模式，网页就会渲染成规定的一套颜色。

AllowAutoDarkWebContent 最后没人用这个值，我们去看看跟他相关的 force_dark_mode_enabled，最后是怎么使用的。

# force_dark_mode_enabled

可以看到 `force_dark_mode_enabled` 这个 setting 会影响 ColorScheme.

third_party/blink/renderer/core/style/computed_style.cc 在各种条件之后，设置 sheme 为 dark_scheme.

```
bool dark_scheme =
      ...||
      (force_dark && !prefers_dark);
SetDarkColorScheme(dark_scheme);
```

再往下就真找到答案了：

SetDarkColorScheme 之后，网站的默认颜色会改：

`third_party/blink/renderer/core/layout/layout_theme.cc`  `LayoutTheme::DefaultSystemColor()`

```
// third_party/blink/renderer/core/layout/layout_theme.cc
Color LayoutTheme::DefaultSystemColor(...) {
  switch (css_value_id) {
    case CSSValueID::kActiveborder:
       return color_scheme == mojom::blink::ColorScheme::kDark
                 ? Color::FromRGBA32(0xFF6B6B6B)
                 : Color::FromRGBA32(0xFF767676);
    case CSSValueID::kText:
      return color_scheme == mojom::blink::ColorScheme::kDark
                 ? Color::FromRGBA32(0xFFFFFFFF)
                 : Color::FromRGBA32(0xFF000000);
    ...
  }
}
```



有意思的是，dark mode 也能影响 image，白的变黑的：

third_party/blink/renderer/core/paint/image_painter.cc

```
auto image_auto_dark_mode = ImageClassifierHelper::GetImageAutoDarkMode(
    *layout_image_.GetFrame(), layout_image_.StyleRef(),
    gfx::RectF(pixel_snapped_dest_rect), src_rect);
context.DrawImage(
    image.get(), decode_mode, image_auto_dark_mode,
    ComputeImagePaintTimingInfo(layout_image_, *image, image_content, context,
                                pixel_snapped_dest_rect),
    gfx::RectF(pixel_snapped_dest_rect), &src_rect, SkBlendMode::kSrcOver,
    respect_orientation);
```

改得是真彻底。



## 附：bitmap_image apply dark theme:

附上 bitmap_image 修改颜色的做法

```
// third_party/blink/renderer/platform/graphics/bitmap_image.cc
void BitmapImage::Draw() {
  ...
  if (draw_options.dark_mode_filter) {
    dark_mode_flags = flags;
    draw_options.dark_mode_filter->ApplyFilterToImage(
        this, &dark_mode_flags.value(), gfx::RectFToSkRect(src_rect));
    image_flags = &dark_mode_flags.value();
  }
  ...
}
// third_party/blink/renderer/platform/graphics/dark_mode_filter.cc
void DarkModeFilter::ApplyFilterToImage(Image* image, ...) {
  ...
  sk_sp<SkColorFilter> color_filter =
      GetDarkModeFilterForImageOnMainThread(this, image, src.roundOut());
  if (color_filter)
    flags->setColorFilter(std::move(color_filter));
  ...
}
sk_sp<SkColorFilter> DarkModeFilter::GenerateImageFilter(...) {
  return (immutable_.image_classifier->Classify(pixmap, src) ==
          DarkModeResult::kApplyFilter)
             ? immutable_.image_filter
             : nullptr;
}

```

有意思，有几种 invert color 的方法，都在 `third_party/blink/renderer/platform/graphics/dark_mode_color_filter.cc`:

```
std::unique_ptr<DarkModeColorFilter> DarkModeColorFilter::FromSettings(
    const DarkModeSettings& settings) {
  switch (settings.mode) {
    case DarkModeInversionAlgorithm::kSimpleInvertForTesting:
      uint8_t identity[256], invert[256];
      for (int i = 0; i < 256; ++i) {
        identity[i] = i;
        invert[i] = 255 - i;
      }
      return SkColorFilterWrapper::Create(
          SkTableColorFilter::MakeARGB(identity, invert, invert, invert));

    case DarkModeInversionAlgorithm::kInvertBrightness:
      return SkColorFilterWrapper::Create(
          SkHighContrastConfig::InvertStyle::kInvertBrightness, settings);

    case DarkModeInversionAlgorithm::kInvertLightness:
      return SkColorFilterWrapper::Create(
          SkHighContrastConfig::InvertStyle::kInvertLightness, settings);

    case DarkModeInversionAlgorithm::kInvertLightnessLAB:
      return std::make_unique<LABColorFilter>();
  }
}
```

以 LABColorFilter 为例，通过 sRGB 的变换，再修剪一下 x 值，最后再变换回去：

```
  SkColor InvertColor(SkColor color) const override {
    SkV3 rgb = {SkColorGetR(color) / 255.0f, SkColorGetG(color) / 255.0f,
                SkColorGetB(color) / 255.0f};
    SkV3 lab = transformer_.SRGBToLAB(rgb);
    lab.x = std::min(110.0f - lab.x, 100.0f);
    rgb = transformer_.LABToSRGB(lab);

    SkColor inverted_color = SkColorSetARGB(
        SkColorGetA(color), static_cast<unsigned int>(rgb.x * 255 + 0.5),
        static_cast<unsigned int>(rgb.y * 255 + 0.5),
        static_cast<unsigned int>(rgb.z * 255 + 0.5));
    return AdjustGray(inverted_color);
  }
```

只能大呼数学太能玩弄人了。



