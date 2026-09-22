from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BODY = ROOT / "assets/fonts/linepods_font_body_20.c"
UI = ROOT / "assets/fonts/linepods_font_ui_18.c"
TITLE = ROOT / "assets/fonts/linepods_font_title_20.c"
NOTO_20_1 = ROOT / "assets/fonts/linepods_font_compare_noto_20_1.c"
PIXEL_24_1 = ROOT / "assets/fonts/linepods_font_compare_pixel_24_1.c"


def main() -> None:
    for path in (BODY, UI, TITLE, NOTO_20_1, PIXEL_24_1):
        source = path.read_text(encoding="utf-8")
        assert "--no-compress" in source, f"{path.name} was not generated uncompressed"
        assert ".bitmap_format = 0" in source, f"{path.name} requires a disabled decoder"
        assert ".bitmap_format = 1" not in source
        assert ".bitmap_format = 2" not in source
        assert "/private/tmp/" not in source

    for path in (UI, TITLE, NOTO_20_1, PIXEL_24_1):
        source = path.read_text(encoding="utf-8")
        assert "--bpp 1" in source, f"{path.name} must use crisp 1-bpp glyphs"
        assert ".bpp = 1" in source, f"{path.name} descriptor must be 1-bpp"

    body = BODY.read_text(encoding="utf-8")
    assert "NotoSansSC-Medium.otf" in body
    assert "--size 20" in body
    assert "--bpp 2" in body
    assert ".bpp = 2" in body
    assert "--autohint-strong" in body
    assert "0x3000-0x3030,0x3036-0x303F" in body
    assert "const lv_font_t linepods_font_body_20" in body

    ui = UI.read_text(encoding="utf-8")
    assert "NotoSansSC-Medium.otf" in ui
    assert "--size 18" in ui
    assert "--bpp 1" in ui
    assert "const lv_font_t linepods_font_ui_18" in ui

    title = TITLE.read_text(encoding="utf-8")
    assert "NotoSansSC-Bold.otf" in title
    assert "--size 20" in title
    assert "0x4F53" in title and "0x5B57" in title
    assert "const lv_font_t linepods_font_title_20" in title

    noto_20_1 = NOTO_20_1.read_text(encoding="utf-8")
    assert "NotoSansSC-Medium.otf" in noto_20_1
    assert "--size 20" in noto_20_1
    assert "--bpp 1" in noto_20_1
    assert "--autohint-strong" in noto_20_1
    assert "const lv_font_t linepods_font_compare_noto_20_1" in noto_20_1

    pixel = PIXEL_24_1.read_text(encoding="utf-8")
    assert "fusion-pixel-12px-proportional-zh_hans.otf" in pixel
    assert "--size 24" in pixel
    assert "--bpp 1" in pixel
    assert "--autohint-off" in pixel
    assert "const lv_font_t linepods_font_compare_pixel_24_1" in pixel

    assert not (ROOT / "assets/fonts/linepods_font_16.c").exists()
    assert not (ROOT / "assets/fonts/linepods_font_body_18.c").exists()
    assert not (ROOT / "assets/fonts/linepods_font_compare_noto_20_2.c").exists()
    print("Linepods font bitmap-format tests: PASS")


if __name__ == "__main__":
    main()
