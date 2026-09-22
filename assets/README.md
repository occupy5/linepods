<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

### Noto Sans SC for Linepods

[`fonts/linepods_font_body_20.c`](fonts/linepods_font_body_20.c) is the generated LVGL
20 px, 2-bpp Medium-weight font for dynamic Weixin Read content. It uses strong autohinting
and is the on-device-selected `3/4` reading treatment: the extra gray levels retain more
stroke detail while stronger grid fitting keeps glyphs stable on the 240 × 320 RGB565
panel. It covers ASCII, the middle dot,
common punctuation, CJK Unified Ideographs and Extension A, compatibility ideographs,
and full-width punctuation:
`0x20-0x7E,0x00B7,0x2013-0x2026,0x3000-0x3030,0x3036-0x303F,0x3400-0x4DBF,0x4E00-0x9FFF,0xF900-0xFAFF,0xFF01-0xFF65`.
The vertical-only iteration marks U+3031–U+3035 are omitted because their vertical metrics
inflate every horizontal line without benefiting Chinese prose.

The source is the official Noto Sans CJK Simplified Chinese Medium OTF from
[`notofonts/noto-cjk`](https://github.com/notofonts/noto-cjk), distributed under the SIL
Open Font License 1.1. The bundled license is
[`fonts/Noto-CJK-OFL.txt`](fonts/Noto-CJK-OFL.txt). The source OTF SHA-256 was
`7633f5a016d4dd95e685a69633d818aabc4644c4b08e26bd35b1b30c45ed5dda`;
the generated C source SHA-256 is
`1af1d12a6a03de8456934c8ca19974a06a1d8d88e4271f50da63a4091317abde`.
Regenerate with:

```bash
npx --yes lv_font_conv@1.5.3 \
  --font NotoSansSC-Medium.otf \
  --range 0x20-0x7E,0x00B7,0x2013-0x2026,0x3000-0x3030,0x3036-0x303F,0x3400-0x4DBF,0x4E00-0x9FFF,0xF900-0xFAFF,0xFF01-0xFF65 \
  --size 20 --bpp 2 --format lvgl --no-compress --no-kerning --autohint-strong \
  --lv-font-name linepods_font_body_20 --lv-include lvgl.h \
  --output assets/fonts/linepods_font_body_20.c
```

[`fonts/linepods_font_ui_18.c`](fonts/linepods_font_ui_18.c) is a compact 18 px,
1-bpp subset for fixed controls, counters, status copy, and the `1/4` comparison sample.
Keeping fixed chrome separate prevents a second full CJK font from consuming Flash while
the 20 px production font handles book titles, authors, chapters, and quote text. Its exact
code-point inventory is recorded in the generated header. The generated C source SHA-256
is `1af40b7073d44bbf1d61159d4ee2c46027390444058d008abf0f2dc3581c9afe`.

[`fonts/linepods_font_title_20.c`](fonts/linepods_font_title_20.c) is a small 20 px,
1-bpp Bold subset containing the twelve fixed page-title glyphs. The source OTF SHA-256 was
`c6cb5a93abaa9edc8ee7463b7ebb7f42d618d40e6ed2f7a5371c97b0b64767c0`;
the generated C SHA-256 is
`c99896a3b273e74844cfe80f796d17b119ec58cca86f38f4f7e929aac46c1a6f`.
Regenerate it from the same licensed family with:

```bash
npx --yes lv_font_conv@1.5.3 \
  --font NotoSansSC-Bold.otf \
  --range 0x4F53,0x5212,0x540C,0x5B57,0x60C5,0x6B65,0x7EBF,0x7F6E,0x8BBE,0x8BD5,0x8BE6,0x91CD \
  --size 20 --bpp 1 --format lvgl --no-compress --no-kerning \
  --lv-font-name linepods_font_title_20 --lv-include lvgl.h \
  --output assets/fonts/linepods_font_title_20.c
```

#### On-device font comparison subsets

The font comparison page uses the compact UI font for its former 18 px baseline,
reuses the production 20 px/2-bpp body font for the selected `3/4` option, and adds
two diagnostic-only subsets containing the same fixed probe text:

| Generated source | Rasterization | SHA-256 |
| --- | --- | --- |
| [`fonts/linepods_font_compare_noto_20_1.c`](fonts/linepods_font_compare_noto_20_1.c) | Noto Sans SC Medium, 20 px, 1 bpp, strong autohint | `3c014c028df908c5db31517598f37467b8bbbdc9bacfadb2e1c67f2aa4ba8be7` |
| [`fonts/linepods_font_compare_pixel_24_1.c`](fonts/linepods_font_compare_pixel_24_1.c) | Fusion Pixel 12px Proportional SC rendered at an integer 2× scale, 24 px, 1 bpp, autohint disabled | `a8e1ac38009e202dec97b9983894b0389c1ef917b1ab6b22cfe6ac8c8718b9a5` |

The Noto diagnostic subset uses the same licensed source and converter as the body font.
The exact code-point inventory and pinned conversion command are retained in each
generated source header. The pixel subset uses the official Fusion Pixel Font
`2026.09.01` release. Its source archive SHA-256 is
`dc2fbfeaca89c1a87195354df92c9914e2b731583cf3ace157b4edb71b561f33`,
and the selected `fusion-pixel-12px-proportional-zh_hans.otf` SHA-256 is
`82e05156031524165202ccdae6c284aaeecd798271e32c65cbf0c6568ade8cfa`.
Fusion Pixel Font is distributed under SIL OFL 1.1; see
[`fonts/Fusion-Pixel-OFL.txt`](fonts/Fusion-Pixel-OFL.txt).

These small assets are for choosing a production rasterization on the physical
panel. They do not provide general Chinese coverage and must not replace the full
body font for network or book content.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |
| [`images/linepods-sync-poster.png`](images/linepods-sync-poster.png) | 1086 × 1448, PNG | Linepods v0.1.0 sync-page promotional poster in a monochrome e-ink editorial style. SHA-256: `7a709117dab8e28bedbe43822c76ef786fe6a7eeefc9151f159a3f62ac7947fd`. |
| [`images/linepods-card-poster.png`](images/linepods-card-poster.png) | 1086 × 1448, PNG | Linepods v0.1.0 highlight-card promotional poster in a monochrome e-ink editorial style. SHA-256: `99542c50bb1c62d54dcb96f6e6ae7b1c8760ff6aa553e2e22f885866f343f98c`. |
| [`images/linepods-detail-poster.png`](images/linepods-detail-poster.png) | 1254 × 1254, PNG | Linepods v0.1.0 paginated-detail promotional poster in a monochrome e-ink editorial style. SHA-256: `e751b12c4cc3ef18c342ff21f09cefbdeaab04cb07a4e9c9f7eadfa95b55bcaa`. |

The three Linepods posters were generated for this project on 2026-09-22 with
the built-in image-generation tool, using the developer-supplied AI Passport
device photos and promotional compositions as references. They are intended for
Linepods project promotion; verify reuse rights for the supplied references
before redistributing them outside this project.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
