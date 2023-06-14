# Investigation

DarkMode 是如何影响最终绘制的颜色的呢？

![image](https://github.com/Leteno/blog/assets/25839908/ddf37bda-5865-428b-9aa1-8745ad129e31)

如上图，DarkMode 信息在 style 流程中 存放在 ComputedStyle 中，往后传递的过程中，以 AutoDarkMode DarkModeFlags 等结构存在，
最后影响了 paint 流程的 PaintFlags 的 `color` 以及 `color-filter` 值，进而影响 SkPaint, 影响 Skia 的绘制。

## ComputedStyle
chromium 引入了一些改动，允许前端代码自行指定该 element 的 theme：
[Plumb per-element auto dark mode options to GraphicsContext](https://source.chromium.org/chromium/chromium/src/+/c831ec8dc6805b2fcb1d60bbade3b000633877d1)

也就是 前端可以通过下面的 code 规避 browser dark mode 的影响。
```
<div style="color-scheme: light only; background-color: white;">
  this background will be white even with auto dark mode enabled.
</div>
```

除此之外呢，ComputedStyle 中将会存放跟 DarkMode 相关的信息，做到每个 Element 都可以定制 DarkMode！

ComputedStyle 中会存放 DarkColorScheme ColorSchemeForced 信息, 他们的值由俩个方面计算出来的：
* HTMLElemnt 的 color-scheme(就是 per-element commit 引入的)
* settings 里面的 ForceDarkModeEnabled, 也就是 我们常谈的 chromium feature flag kForceWebContentsDarkMode.

接下来 render 流程，他会以 AutoDarkMode 的结构存在，只保存了一个值 `enabled = computed_style.ForceDark()`, 其中 `ForceDark = DarkColorScheme && ColorSchemeForced`

之后你将看到，他会通过修改 PaintFlags 中的 `color` 以及 `color-filter` 的值，影响 SkPaint，进而影响 Skia 的绘制

## PaintFlags color - DrawRect

我们以 DrawRect 为例，看看 PaintFlags 的 `color` 值怎么被改变的，以及最后怎么影响 Skia

SVGShapePainter::FillShape， 可以看到此时 AutoDarkMode 结构被构建，往 GraphicsContext::DrawRect 传
```
AutoDarkMode auto_dark_mode(PaintAutoDarkMode(
      layout_svg_shape_.StyleRef(), DarkModeFilter::ElementRole::kSVG));
context.DrawRect(
          gfx::RectFToSkRect(layout_svg_shape_.ObjectBoundingBox()), flags,
          auto_dark_mode);
```

GraphicsContext::DrawRect 也只是简单构建了 DarkModeFlags
```
void GraphicsContext::DrawRect(const SkRect& rect,
                               const cc::PaintFlags& flags,
                               const AutoDarkMode& auto_dark_mode) {
  canvas_->drawRect(rect, DarkModeFlags(this, auto_dark_mode, flags));
}
```

其实 DarkModeFlags 构建完时，PaintFlags 的 color 已经被改变了。
```
GraphicsContext::DarkModeFlags::DarkModeFlags() {
  ...
  if (auto_dark_mode.enabled) {
      dark_mode_flags_ = context->GetDarkModeFilter()->ApplyToFlagsIfNeeded(
          flags, auto_dark_mode.role,
          SkColor4f::FromColor(auto_dark_mode.contrast_color));
      ...
  }
  ...
}

DarkModeFilter::ApplyToFlagsIfNeeded() {
  ...
  
  cc::PaintFlags dark_mode_flags = flags;
  SkColor4f flags_color = flags.getColor4f();
  if (ShouldApplyToColor(flags_color, role)) {
    flags_color = inverted_color_cache_->GetInvertedColor(
        immutable_.color_filter.get(), flags_color);
  }
  dark_mode_flags.setColor(AdjustDarkenColor(
      flags_color, role,
      InvertColorIfNeeded(contrast_background, ElementRole::kBackground)));
  ...
}
```
可以看到 DarkModeFlags 构建完之时，PaintFlags 的 color 已经被 Darken 了。而 PaintFlags 在后续会用于 SkPaint 的构建，进而影响 Skia 的绘制。

```
SkPaint PaintFlags::ToSkPaint() const {
  ...
  if (color_filter_) {
    paint.setColorFilter(color_filter_->GetSkColorFilter());
  }
  paint.setColor(color_);
  ...
}

void SkiaPaintCanvas::drawRect(const SkRect& rect, const PaintFlags& flags) {
  ScopedRasterFlags raster_flags(&flags, image_provider_,
                                 canvas_->getTotalMatrix(), GetMaxTextureSize(),
                                 1.0f);
  if (!raster_flags.flags())
    return;
  raster_flags.flags()->DrawToSk(
      canvas_,
      [&rect](SkCanvas* c, const SkPaint& p) { c->drawRect(rect, p); });   // <== 最后的绘制只传入 Rect 信息以及 SkPaint
  FlushAfterDrawIfNeeded();
}
```

所以对于 `DrawRect` 的黑化，通过修改了 PaintFlags 中的 color(DarkModeFlags::DarkModeFlags)，进而影响了 SkPaint 的 color 值，最终影响 Skia 的绘制。

## PaintFlags color-filter - DrawImage

关于 image，也是修改了 PaintFlags 的值(color-filter)来影响 Skia 的绘制。

起点也是从 ComputedStyle 构建 AutoDarkMode

```
void ImagePainter::PaintIntoRect() {
  ...
  auto image_auto_dark_mode = ImageClassifierHelper::GetImageAutoDarkMode(
      *layout_image_.GetFrame(), layout_image_.StyleRef(),
      gfx::RectF(pixel_snapped_dest_rect), src_rect);
  context.DrawImage(
      *image, decode_mode, image_auto_dark_mode,
      ComputeImagePaintTimingInfo(layout_image_, *image, image_content, context,
                                  pixel_snapped_dest_rect),
      gfx::RectF(pixel_snapped_dest_rect), &src_rect, SkBlendMode::kSrcOver,
      respect_orientation);
}
```

接着是 GraphicsContext::DrawImage. 可以看到此时会构建 DarkModeFilter
```
void GraphicsContext::DrawImage() {
  cc::PaintFlags image_flags = ImmutableState()->FillFlags();

  DarkModeFilter* dark_mode_filter = GetDarkModeFilterForImage(auto_dark_mode);
  ImageDrawOptions draw_options(dark_mode_filter, sampling,
                                should_respect_image_orientation, clamping_mode,
                                decode_mode, auto_dark_mode.enabled,
                                paint_timing_info.image_may_be_lcp_candidate);
  image.Draw(canvas_, image_flags, dest, src, draw_options);
  SetImagePainted(paint_timing_info.report_paint_timing);
}
```
接着 我们看到 BitmapImage::Draw 的实现，这里面开始替换 PaintFlags 中的 color_filter.
```
BitmapImage::Draw() {
  const cc::PaintFlags* image_flags = &flags;
  absl::optional<cc::PaintFlags> dark_mode_flags;
  if (draw_options.dark_mode_filter) {
    dark_mode_flags = flags;
    draw_options.dark_mode_filter->ApplyFilterToImage(        // 此处将修改 dark_mode_flags 的 ColorFilter
        this, &dark_mode_flags.value(), gfx::RectFToSkRect(src_rect));
    image_flags = &dark_mode_flags.value();         // image_flags 的 ColorFilter 也被改了
  }
  canvas->drawImageRect(
      std::move(image), gfx::RectFToSkRect(adjusted_src_rect),
      gfx::RectFToSkRect(adjusted_dst_rect), draw_options.sampling_options,
      image_flags,         // 此时 PaintFlags 里面的 color_filter 已经设为 DarkModeColorFilter 了
      WebCoreClampingModeToSkiaRectConstraint(draw_options.clamping_mode));
}

从 DarkModeFilter 中的 image_filter 替换掉 PaintFlags 的 color-filter
void DarkModeFilter::ApplyFilterToImage(Image* image,
                                        cc::PaintFlags* flags,
                                        const SkRect& src) {
     ...
     flags->setColorFilter(GetImageFilter()); // GetImageFilter = immutable_->image_filter
     ...
}

而 DarkModeFilter 中的 `immutable_->image_filter` 是来自于 DarkModeColorFilter
DarkModeFilter::ImmutableData::ImmutableData() {
  color_filter = DarkModeColorFilter::FromSettings(settings);
  if (!color_filter)
    return;

  image_filter = color_filter->ToColorFilter();
  ...
}

DarkModeColorFilter 根据不同的 Settings 给了不同的 filter:
std::unique_ptr<DarkModeColorFilter> DarkModeColorFilter::FromSettings(
    const DarkModeSettings& settings) {
  switch (settings.mode) {
    case DarkModeInversionAlgorithm::kInvertBrightness:
      return ColorFilterWrapper::Create(
          SkHighContrastConfig::InvertStyle::kInvertBrightness, settings);

    case DarkModeInversionAlgorithm::kInvertLightness:
      return ColorFilterWrapper::Create(
          SkHighContrastConfig::InvertStyle::kInvertLightness, settings);

    case DarkModeInversionAlgorithm::kInvertLightnessLAB:
      return std::make_unique<LABColorFilter>();
  }
  NOTREACHED();
}
```

整个流程走完，此时 PaintFlags 里面的 color_filter 已经被替换成 DarkModeColorFilter 了
而我们回过头看看 PaintFlags::ToSkPaint 的实现，SkPaint 的 color_filter 也变成了 DarkModeColorFilter 了，进而影响 Skia 的绘制。

不过我还没看到 Skia 在绘制 image 的时候，具体哪个 code 会使用 color_filter. 
冲过一次，迷路了。需要后续鼓起勇气再冲一次，继续看 Skia 的代码，又是一段未知之旅。

## 待续
