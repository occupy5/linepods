<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

### 微信读书摘录应用使用的 Noto Sans SC

[`fonts/linepods_font_body_20.c`](fonts/linepods_font_body_20.c) 是为微信读书动态内容生成的
LVGL 20 px、2 bpp Medium 字重字库。它使用强 hinting，并对应真机选中的 `3/4`
阅读方案：额外灰阶保留更多笔画细节，强网格对齐则让字形在 240 × 320 RGB565
屏幕上更稳定。字库覆盖 ASCII、
间隔号、常用标点、CJK 统一汉字与扩展 A、兼容汉字及全角标点：
`0x20-0x7E,0x00B7,0x2013-0x2026,0x3000-0x3030,0x3036-0x303F,0x3400-0x4DBF,0x4E00-0x9FFF,0xF900-0xFAFF,0xFF01-0xFF65`。
竖排专用的 U+3031–U+3035 被排除，因为它们会抬高所有横排文本的行高，却不会改善中文正文显示。

源字体是 [`notofonts/noto-cjk`](https://github.com/notofonts/noto-cjk) 官方发布的
Noto Sans CJK 简体中文 Medium OTF，采用 SIL Open Font License 1.1；仓库内附
[`fonts/Noto-CJK-OFL.txt`](fonts/Noto-CJK-OFL.txt)。源 OTF 的 SHA-256 为
`7633f5a016d4dd95e685a69633d818aabc4644c4b08e26bd35b1b30c45ed5dda`；
生成 C 文件的 SHA-256 为
`1af1d12a6a03de8456934c8ca19974a06a1d8d88e4271f50da63a4091317abde`。
执行：

```bash
npx --yes lv_font_conv@1.5.3 \
  --font NotoSansSC-Medium.otf \
  --range 0x20-0x7E,0x00B7,0x2013-0x2026,0x3000-0x3030,0x3036-0x303F,0x3400-0x4DBF,0x4E00-0x9FFF,0xF900-0xFAFF,0xFF01-0xFF65 \
  --size 20 --bpp 2 --format lvgl --no-compress --no-kerning --autohint-strong \
  --lv-font-name linepods_font_body_20 --lv-include lvgl.h \
  --output assets/fonts/linepods_font_body_20.c
```

[`fonts/linepods_font_ui_18.c`](fonts/linepods_font_ui_18.c) 是用于固定控件、计数、状态文案和
`1/4` 对比样本的紧凑 18 px、1 bpp 子集。固定界面与正文分离，可以避免第二套完整中文
字体占用 Flash；书名、作者、章节和划线正文统一交给 20 px 正式字体。精确码点清单保留在
生成文件头部，生成的 C 源文件 SHA-256 为
`1af40b7073d44bbf1d61159d4ee2c46027390444058d008abf0f2dc3581c9afe`。

[`fonts/linepods_font_title_20.c`](fonts/linepods_font_title_20.c) 是一个小型
20 px、1 bpp Bold 标题子集，覆盖 `划线详情设置同步重试字体`。源 OTF 的 SHA-256 为
`c6cb5a93abaa9edc8ee7463b7ebb7f42d618d40e6ed2f7a5371c97b0b64767c0`；
生成 C 文件的 SHA-256 为
`c99896a3b273e74844cfe80f796d17b119ec58cca86f38f4f7e929aac46c1a6f`。
使用同一许可字体家族重新生成：

```bash
npx --yes lv_font_conv@1.5.3 \
  --font NotoSansSC-Bold.otf \
  --range 0x4F53,0x5212,0x540C,0x5B57,0x60C5,0x6B65,0x7EBF,0x7F6E,0x8BBE,0x8BD5,0x8BE6,0x91CD \
  --size 20 --bpp 1 --format lvgl --no-compress --no-kerning \
  --lv-font-name linepods_font_title_20 --lv-include lvgl.h \
  --output assets/fonts/linepods_font_title_20.c
```

#### 真机字体对比子集

字体对比页使用紧凑 UI 字体作为原 18 px 基线，`3/4` 直接复用正式的
20 px/2-bpp 正文字体，另外加入两套只包含同一段固定测试文字的诊断子集：

| 生成源码 | 光栅化参数 | SHA-256 |
| --- | --- | --- |
| [`fonts/linepods_font_compare_noto_20_1.c`](fonts/linepods_font_compare_noto_20_1.c) | Noto Sans SC Medium，20 px，1 bpp，强 hinting | `3c014c028df908c5db31517598f37467b8bbbdc9bacfadb2e1c67f2aa4ba8be7` |
| [`fonts/linepods_font_compare_pixel_24_1.c`](fonts/linepods_font_compare_pixel_24_1.c) | Fusion Pixel 12px Proportional SC 按 2 倍整数比例渲染为 24 px，1 bpp，关闭 hinting | `a8e1ac38009e202dec97b9983894b0389c1ef917b1ab6b22cfe6ac8c8718b9a5` |

Noto 诊断子集复用正文字体的许可源和转换器；每个生成源码的文件头保留了精确字符清单和
固定版本转换命令。像素字体来自 Fusion Pixel Font 官方 `2026.09.01` Release，源压缩包
SHA-256 为 `dc2fbfeaca89c1a87195354df92c9914e2b731583cf3ace157b4edb71b561f33`，
选用的 `fusion-pixel-12px-proportional-zh_hans.otf` SHA-256 为
`82e05156031524165202ccdae6c284aaeecd798271e32c65cbf0c6568ade8cfa`。
Fusion Pixel Font 采用 SIL OFL 1.1，许可见
[`fonts/Fusion-Pixel-OFL.txt`](fonts/Fusion-Pixel-OFL.txt)。

这些小型资源只用于在物理屏幕上选择最终光栅化方案，不覆盖动态中文内容，不能替代正式
正文字库。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。 |
| [`images/linepods-sync-poster.png`](images/linepods-sync-poster.png) | 1086 × 1448，PNG | Linepods v0.1.0 同步页宣传图，采用黑白墨水屏编辑风格。SHA-256：`7a709117dab8e28bedbe43822c76ef786fe6a7eeefc9151f159a3f62ac7947fd`。 |
| [`images/linepods-card-poster.png`](images/linepods-card-poster.png) | 1086 × 1448，PNG | Linepods v0.1.0 划线卡片宣传图，采用黑白墨水屏编辑风格。SHA-256：`99542c50bb1c62d54dcb96f6e6ae7b1c8760ff6aa553e2e22f885866f343f98c`。 |
| [`images/linepods-detail-poster.png`](images/linepods-detail-poster.png) | 1254 × 1254，PNG | Linepods v0.1.0 分页详情宣传图，采用黑白墨水屏编辑风格。SHA-256：`e751b12c4cc3ef18c342ff21f09cefbdeaab04cb07a4e9c9f7eadfa95b55bcaa`。 |

三张 Linepods 宣传图于 2026-09-22 使用内置图像生成工具为本项目制作，
以开发者提供的 AI Passport 设备照片和宣传图构图为参考。这些图片用于
Linepods 项目宣传；若要在本项目之外二次分发，
需先确认所有参考素材的复用权利。

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。
