#include "linepods_ui.h"
#include "linepods_status.h"

#include "esp_log.h"
#include "lvgl.h"
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

LV_FONT_DECLARE(linepods_font_body_20);
LV_FONT_DECLARE(linepods_font_compare_noto_20_1);
LV_FONT_DECLARE(linepods_font_compare_pixel_24_1);
LV_FONT_DECLARE(linepods_font_title_20);
LV_FONT_DECLARE(linepods_font_ui_18);

#define COLOR_INK 0x181817
#define COLOR_MUTED 0x5B5B56
#define COLOR_BACKGROUND 0xE9E8E1
#define COLOR_SURFACE 0xF3F1EA
#define COLOR_PAPER 0xEFEDE5
#define COLOR_PAPER_BACK 0xDCDAD3
#define COLOR_PAPER_BACK_2 0xCBCAC3
#define COLOR_ACCENT 0x3F3F3B
#define COLOR_ACCENT_SOFT 0x9B9A92
#define COLOR_RED 0x1D1D1B
#define COLOR_LINE 0xB8B6AE
#define DETAIL_TEXT_WIDTH 192
#define DETAIL_TEXT_LINES 6
#define BATTERY_SEGMENT_WIDTH 4
#define BATTERY_SEGMENT_HEIGHT 6

static const char *TAG = "linepods_ui";
static lv_obj_t *s_screen;
static lv_obj_t *s_time_label;
static lv_obj_t *s_battery_segments[LINEPODS_BATTERY_SEGMENT_COUNT];
static int s_battery_percent = -1;
static time_t s_system_time;
static bool s_fonts_checked;

static void style_screen(lv_obj_t *screen)
{
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_shadow_width(screen, 0, 0);
}

static lv_obj_t *plain_container(lv_obj_t *parent)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_radius(object, 0, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_set_style_shadow_width(object, 0, 0);
    return object;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int color)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_obj_set_style_text_font(object, &linepods_font_ui_18, 0);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    lv_label_set_text(object, text ? text : "");
    return object;
}

static lv_obj_t *body_label(lv_obj_t *parent, const char *text, int color)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_obj_set_style_text_font(object, &linepods_font_body_20, 0);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    lv_label_set_text(object, text ? text : "");
    return object;
}

static void set_single_line(lv_obj_t *object, int height)
{
    lv_obj_set_size(object, lv_pct(100), height);
    lv_label_set_long_mode(object, LV_LABEL_LONG_DOT);
}

static void style_panel(lv_obj_t *panel, int color, int radius)
{
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, radius, 0);
    lv_obj_set_style_shadow_width(panel, 0, 0);
}

static lv_obj_t *add_header(lv_obj_t *page)
{
    lv_obj_t *header = plain_container(page);
    lv_obj_set_size(header, lv_pct(100), 30);

    s_time_label = label(header, "--:--", COLOR_INK);
    lv_obj_set_size(s_time_label, 72, 25);
    lv_label_set_long_mode(s_time_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_time_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(s_time_label, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *battery_group = plain_container(header);
    lv_obj_set_size(battery_group, 32, 22);
    lv_obj_set_flex_flow(battery_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(battery_group, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_align(battery_group, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t *battery_icon = plain_container(battery_group);
    lv_obj_set_size(battery_icon, 28, 12);

    lv_obj_t *battery_shell = lv_obj_create(battery_icon);
    lv_obj_remove_flag(battery_shell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(battery_shell, 24, 12);
    lv_obj_set_pos(battery_shell, 0, 0);
    lv_obj_set_style_bg_opa(battery_shell, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(battery_shell, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_border_width(battery_shell, 1, 0);
    lv_obj_set_style_radius(battery_shell, 1, 0);
    lv_obj_set_style_pad_all(battery_shell, 0, 0);

    for (unsigned i = 0; i < LINEPODS_BATTERY_SEGMENT_COUNT; i++) {
        s_battery_segments[i] = lv_obj_create(battery_shell);
        style_panel(s_battery_segments[i], COLOR_ACCENT_SOFT, 0);
        lv_obj_set_size(s_battery_segments[i], BATTERY_SEGMENT_WIDTH,
                        BATTERY_SEGMENT_HEIGHT);
        lv_obj_set_pos(s_battery_segments[i], 2 + (int)i * 5, 2);
        lv_obj_set_style_bg_opa(s_battery_segments[i], LV_OPA_30, 0);
    }

    lv_obj_t *battery_cap = lv_obj_create(battery_icon);
    style_panel(battery_cap, COLOR_MUTED, 0);
    lv_obj_set_size(battery_cap, 3, 6);
    lv_obj_set_pos(battery_cap, 25, 3);

    return header;
}

static lv_obj_t *new_screen(lv_obj_t **page_out)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    style_screen(screen);

    lv_obj_t *page = plain_container(screen);
    lv_obj_set_size(page, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_left(page, 12, 0);
    lv_obj_set_style_pad_right(page, 12, 0);
    lv_obj_set_style_pad_top(page, 10, 0);
    lv_obj_set_style_pad_bottom(page, 8, 0);
    lv_obj_set_style_pad_row(page, 6, 0);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    add_header(page);
    *page_out = page;
    return screen;
}

static lv_obj_t *add_paper_stack(lv_obj_t *page)
{
    lv_obj_t *stack = plain_container(page);
    lv_obj_set_size(stack, lv_pct(100), 170);

    lv_obj_t *back_2 = lv_obj_create(stack);
    style_panel(back_2, COLOR_PAPER_BACK_2, 10);
    lv_obj_set_size(back_2, 200, 158);
    lv_obj_set_pos(back_2, 8, 8);

    lv_obj_t *back_1 = lv_obj_create(stack);
    style_panel(back_1, COLOR_PAPER_BACK, 9);
    lv_obj_set_size(back_1, 208, 162);
    lv_obj_set_pos(back_1, 4, 4);

    lv_obj_t *paper = lv_obj_create(stack);
    style_panel(paper, COLOR_PAPER, 8);
    lv_obj_set_size(paper, 216, 162);
    lv_obj_set_pos(paper, 0, 0);
    lv_obj_set_style_border_color(paper, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(paper, 1, 0);
    lv_obj_set_style_pad_all(paper, 0, 0);
    return paper;
}

static lv_obj_t *add_separator(lv_obj_t *parent, int x, int y, int width)
{
    lv_obj_t *line = lv_obj_create(parent);
    style_panel(line, COLOR_LINE, 0);
    lv_obj_set_size(line, width, 1);
    lv_obj_set_pos(line, x, y);
    return line;
}

static void add_quote_text(lv_obj_t *paper, const char *text, int line_space)
{
    lv_obj_t *mark = label(paper, "“", COLOR_ACCENT_SOFT);
    lv_obj_set_size(mark, 26, 22);
    lv_obj_set_pos(mark, 16, 7);

    lv_obj_t *quote = body_label(paper, text, COLOR_INK);
    lv_obj_set_size(quote, 184, 116);
    lv_obj_set_pos(quote, 16, 29);
    lv_label_set_long_mode(quote, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_line_space(quote, line_space, 0);
}

static uint16_t detail_glyph_width(uint32_t codepoint, uint32_t next_codepoint,
                                   void *context)
{
    (void)context;
    if (codepoint == '\t') {
        return (uint16_t)(lv_font_get_glyph_width(&linepods_font_body_20, ' ', ' ') * 4);
    }
    return lv_font_get_glyph_width(&linepods_font_body_20, codepoint, next_codepoint);
}

static lv_obj_t *add_metadata(lv_obj_t *page, const char *primary,
                              const char *secondary)
{
    lv_obj_t *meta = plain_container(page);
    lv_obj_set_size(meta, lv_pct(100), 64);
    lv_obj_set_flex_flow(meta, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(meta, 0, 0);

    lv_obj_t *primary_label = body_label(meta, primary, COLOR_INK);
    set_single_line(primary_label, 30);
    /* Authors are dynamic Linepods content and require the full CJK body font. */
    lv_obj_t *secondary_label = body_label(meta, secondary, COLOR_MUTED);
    /* The body font's line height is exactly 29 px. Keep additional descent
     * room so glyphs touching that bound are not clipped by the label or meta. */
    set_single_line(secondary_label, 34);
    return meta;
}

static void add_stats_footer(lv_obj_t *page, size_t index, size_t item_count,
                             unsigned book_count)
{
    lv_obj_t *footer = plain_container(page);
    lv_obj_set_size(footer, lv_pct(100), 24);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    char counter[24];
    snprintf(counter, sizeof(counter), "%02u / %02u",
             (unsigned)(index + 1), (unsigned)item_count);
    lv_obj_t *count = label(footer, counter, COLOR_ACCENT);
    lv_obj_set_size(count, 104, 24);
    lv_label_set_long_mode(count, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(count, LV_TEXT_ALIGN_LEFT, 0);

    char total[24];
    snprintf(total, sizeof(total), "共 %u 本", book_count);
    lv_obj_t *total_label = label(footer, total, COLOR_MUTED);
    lv_obj_set_size(total_label, 104, 24);
    lv_label_set_long_mode(total_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(total_label, LV_TEXT_ALIGN_RIGHT, 0);
}

static void add_footer(lv_obj_t *page, const char *text)
{
    lv_obj_t *footer = label(page, text, COLOR_MUTED);
    set_single_line(footer, 20);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
}

static void add_body_footer(lv_obj_t *page, const char *text)
{
    lv_obj_t *footer = body_label(page, text, COLOR_MUTED);
    lv_obj_set_size(footer, lv_pct(100), 50);
    lv_label_set_long_mode(footer, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(footer, 0, 0);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
}

static void add_action_button(lv_obj_t *page, const char *text)
{
    lv_obj_t *button = lv_obj_create(page);
    lv_obj_set_size(button, lv_pct(100), 40);
    style_panel(button, COLOR_INK, 7);
    lv_obj_set_style_pad_all(button, 0, 0);

    lv_obj_t *button_text = body_label(button, text, COLOR_PAPER);
    lv_obj_set_size(button_text, lv_pct(100), 30);
    lv_obj_center(button_text);
    lv_label_set_long_mode(button_text, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(button_text, LV_TEXT_ALIGN_CENTER, 0);
}

static void load_screen(lv_obj_t *screen)
{
    lv_obj_t *old = s_screen;
    s_screen = screen;
    lv_screen_load(screen);
    if (old && old != screen) lv_obj_delete(old);
    linepods_ui_set_battery(s_battery_percent);
    linepods_ui_set_time(s_system_time);
}

static bool font_has_glyph(const lv_font_t *font, uint32_t codepoint)
{
    lv_font_glyph_dsc_t glyph = {0};
    return lv_font_get_glyph_dsc(font, &glyph, codepoint, 0)
        && !glyph.is_placeholder;
}

static void check_required_glyphs(const lv_font_t *font, const uint32_t *glyphs,
                                  size_t count, const char *font_name)
{
    bool complete = true;
    for (size_t i = 0; i < count; i++) {
        if (!font_has_glyph(font, glyphs[i])) {
            ESP_LOGE(TAG, "%s missing U+%04" PRIX32, font_name, glyphs[i]);
            complete = false;
        }
    }
    if (complete) ESP_LOGI(TAG, "%s fixed glyph coverage: PASS", font_name);
}

static void check_fonts_once(void)
{
    if (s_fonts_checked) return;
    static const uint32_t title_glyphs[] = {
        0x4F53, 0x5212, 0x540C, 0x5B57, 0x60C5, 0x6B65,
        0x7EBF, 0x7F6E, 0x8BBE, 0x8BD5, 0x8BE6, 0x91CD,
    };
    static const uint32_t body_punctuation[] = {
        0x00B7, 0x201C, 0x2026, 0x3000, 0xFF0C, 0x3002,
    };
    static const uint32_t onboarding_glyphs[] = {
        0x4E00, 0x4E8C, 0x4EE3, 0x5341, 0x5C0F, 0x5C81,
        0x5929, 0x65F6, 0x6CE2, 0x738B, 0x91D1, 0x9EC4, 0x300A, 0x300B,
    };
    static const uint32_t comparison_glyphs[] = {
        0x0041, 0x00B7, 0x3002, 0x8D62, 0x85CF, 0x9B13, 0x8584,
        0x66DC, 0xFF0C, 0xFF1A,
    };
    check_required_glyphs(&linepods_font_title_20, title_glyphs,
                          sizeof(title_glyphs) / sizeof(title_glyphs[0]),
                          "linepods_font_title_20");
    check_required_glyphs(&linepods_font_body_20, body_punctuation,
                          sizeof(body_punctuation) / sizeof(body_punctuation[0]),
                          "linepods_font_body_20");
    check_required_glyphs(&linepods_font_body_20, onboarding_glyphs,
                          sizeof(onboarding_glyphs) / sizeof(onboarding_glyphs[0]),
                          "linepods_font_body_20 onboarding");
    check_required_glyphs(&linepods_font_ui_18, comparison_glyphs,
                          sizeof(comparison_glyphs) / sizeof(comparison_glyphs[0]),
                          "linepods_font_ui_18");
    check_required_glyphs(&linepods_font_compare_noto_20_1, comparison_glyphs,
                          sizeof(comparison_glyphs) / sizeof(comparison_glyphs[0]),
                          "linepods_font_compare_noto_20_1");
    check_required_glyphs(&linepods_font_compare_pixel_24_1, comparison_glyphs,
                          sizeof(comparison_glyphs) / sizeof(comparison_glyphs[0]),
                          "linepods_font_compare_pixel_24_1");
    s_fonts_checked = true;
}

void linepods_ui_init(void)
{
    check_fonts_once();
    linepods_ui_show_loading("正在准备阅读空间…");
}

void linepods_ui_show_welcome(void)
{
    lv_obj_t *page;
    lv_obj_t *screen = new_screen(&page);
    lv_obj_set_style_pad_top(page, 6, 0);
    lv_obj_set_style_pad_bottom(page, 6, 0);
    lv_obj_set_style_pad_row(page, 4, 0);

    lv_obj_t *title = body_label(page, "把划线装进口袋", COLOR_INK);
    set_single_line(title, 30);

    lv_obj_t *paper = lv_obj_create(page);
    lv_obj_set_size(paper, lv_pct(100), 146);
    style_panel(paper, COLOR_PAPER, 8);
    lv_obj_set_style_border_color(paper, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(paper, 1, 0);
    lv_obj_set_style_pad_all(paper, 0, 0);

    lv_obj_t *mark = label(paper, "“", COLOR_ACCENT_SOFT);
    lv_obj_set_size(mark, 26, 22);
    lv_obj_set_pos(mark, 16, 7);

    lv_obj_t *quote = body_label(
        paper, "那一天我二十一岁，\n在我一生的黄金时代。", COLOR_INK);
    lv_obj_set_size(quote, 184, 94);
    lv_obj_set_pos(quote, 16, 32);
    lv_label_set_long_mode(quote, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(quote, 5, 0);

    lv_obj_t *source = body_label(page, "《黄金时代》 · 王小波", COLOR_MUTED);
    set_single_line(source, 30);
    lv_obj_set_style_text_align(source, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t *spacer = plain_container(page);
    lv_obj_set_size(spacer, 1, 0);
    lv_obj_set_flex_grow(spacer, 1);
    add_action_button(page, "确定键 · 同步划线");
    load_screen(screen);
}

void linepods_ui_show_setup(const char *ssid, const char *password)
{
    lv_obj_t *page;
    lv_obj_t *screen = new_screen(&page);
    lv_obj_set_style_pad_top(page, 6, 0);
    lv_obj_set_style_pad_bottom(page, 6, 0);
    lv_obj_set_style_pad_row(page, 4, 0);

    lv_obj_t *title = body_label(page, "连接 Linepods", COLOR_INK);
    set_single_line(title, 30);

    lv_obj_t *intro = body_label(page,
        "1  手机连接下方热点\n2  打开 192.168.4.1", COLOR_MUTED);
    lv_obj_set_size(intro, lv_pct(100), 58);
    lv_label_set_long_mode(intro, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(intro, 0, 0);

    lv_obj_t *card = lv_obj_create(page);
    lv_obj_set_size(card, lv_pct(100), 116);
    style_panel(card, COLOR_SURFACE, 8);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_set_style_pad_row(card, 0, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *network_label = label(card, "热点", COLOR_RED);
    set_single_line(network_label, 20);
    lv_obj_t *network = body_label(card, ssid, COLOR_INK);
    set_single_line(network, 29);
    lv_obj_t *password_label = label(card, "密码", COLOR_RED);
    set_single_line(password_label, 20);
    lv_obj_t *password_value = body_label(card, password, COLOR_INK);
    set_single_line(password_value, 29);

    add_body_footer(page, "提交后回到设备\n查看验证结果");
    load_screen(screen);
}

void linepods_ui_show_loading(const char *message)
{
    lv_obj_t *page;
    lv_obj_t *screen = new_screen(&page);

    lv_obj_t *card = lv_obj_create(page);
    lv_obj_set_size(card, lv_pct(100), 184);
    style_panel(card, COLOR_SURFACE, 8);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_pad_row(card, 8, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *quote = label(card, "“", COLOR_ACCENT_SOFT);
    set_single_line(quote, 24);
    lv_obj_t *status = body_label(card, message ? message : "正在连接…", COLOR_INK);
    lv_obj_set_size(status, lv_pct(100), 112);
    lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(status, 4, 0);

    lv_obj_t *note = label(page, "第一次同步可能需要一点时间", COLOR_MUTED);
    set_single_line(note, 22);
    add_footer(page, "请保持 2.4 GHz Wi-Fi 可用");
    load_screen(screen);
}

void linepods_ui_show_sync_success(const linepods_content_t *content)
{
    if (!content) return;
    lv_obj_t *page;
    lv_obj_t *screen = new_screen(&page);

    lv_obj_t *title = body_label(page, "摘录已装进口袋", COLOR_INK);
    set_single_line(title, 30);

    lv_obj_t *card = lv_obj_create(page);
    lv_obj_set_size(card, lv_pct(100), 154);
    style_panel(card, COLOR_PAPER, 8);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *status = body_label(card, "同步完成", COLOR_ACCENT);
    set_single_line(status, 30);

    char summary[64];
    snprintf(summary, sizeof(summary), "已带回 %u 本书\n共 %u 条划线",
             (unsigned)content->loaded_book_count,
             (unsigned)content->item_count);
    lv_obj_t *counts = body_label(card, summary, COLOR_INK);
    lv_obj_set_size(counts, lv_pct(100), 68);
    lv_label_set_long_mode(counts, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(counts, 4, 0);

    lv_obj_t *note = body_label(card, "以后会自动使用这份配置", COLOR_MUTED);
    set_single_line(note, 30);

    lv_obj_t *spacer = plain_container(page);
    lv_obj_set_size(spacer, 1, 0);
    lv_obj_set_flex_grow(spacer, 1);
    add_action_button(page, "确定键 · 开始阅读");
    load_screen(screen);
}

void linepods_ui_show_setup_failure(const char *message)
{
    lv_obj_t *page;
    lv_obj_t *screen = new_screen(&page);
    lv_obj_set_style_pad_top(page, 6, 0);
    lv_obj_set_style_pad_bottom(page, 6, 0);
    lv_obj_set_style_pad_row(page, 4, 0);

    lv_obj_t *title = body_label(page, "还差一步，没有连接成功", COLOR_INK);
    lv_obj_set_size(title, lv_pct(100), 50);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);

    lv_obj_t *card = lv_obj_create(page);
    lv_obj_set_size(card, lv_pct(100), 138);
    style_panel(card, COLOR_SURFACE, 8);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *detail = body_label(
        card, message && message[0] ? message : "暂时无法完成同步。", COLOR_INK);
    lv_obj_set_size(detail, lv_pct(100), 68);
    lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(detail, 2, 0);

    lv_obj_t *hint = body_label(card,
        "请检查 Wi-Fi、密码和 API Key", COLOR_MUTED);
    lv_obj_set_size(hint, lv_pct(100), 34);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);

    add_body_footer(page, "确定键重试\n长按上键返回配置");
    load_screen(screen);
}

void linepods_ui_show_card(const linepods_content_t *content, size_t index)
{
    if (!content || index >= content->item_count || index != content->item_index) return;
    const linepods_item_t *item = &content->item;
    const linepods_quote_t *quote = &item->quote;
    lv_obj_t *page;
    lv_obj_t *screen = new_screen(&page);
    /* Keep the taller paper, unclipped author line, and stats inside 240x320. */
    lv_obj_set_style_pad_top(page, 8, 0);
    lv_obj_set_style_pad_bottom(page, 8, 0);
    lv_obj_set_style_pad_row(page, 4, 0);
    lv_obj_t *paper = add_paper_stack(page);

    add_quote_text(paper, quote->preview, 0);

    add_metadata(page, item->book.title, item->book.author);
    lv_obj_t *spacer = plain_container(page);
    lv_obj_set_size(spacer, 1, 0);
    lv_obj_set_flex_grow(spacer, 1);
    add_stats_footer(page, index, content->item_count,
                     (unsigned)content->loaded_book_count);
    load_screen(screen);
}

bool linepods_ui_prepare_detail_pager(const char *text, linepods_pager_t *pager)
{
    return linepods_pager_build(pager, text, DETAIL_TEXT_WIDTH,
                              DETAIL_TEXT_LINES, detail_glyph_width, NULL);
}

void linepods_ui_show_detail(const linepods_content_t *content, size_t index,
                           const char *text, const linepods_pager_t *pager,
                           size_t page_index)
{
    if (!content || index >= content->item_count || index != content->item_index
        || !text || !pager) return;
    const linepods_item_t *item = &content->item;
    const linepods_quote_t *quote = &item->quote;
    lv_obj_t *page;
    lv_obj_t *screen = new_screen(&page);

    lv_obj_t *context = body_label(page,
        quote->chapter[0] ? quote->chapter : item->book.title, COLOR_MUTED);
    set_single_line(context, 30);

    lv_obj_t *reader = lv_obj_create(page);
    lv_obj_set_size(reader, lv_pct(100), 230);
    style_panel(reader, COLOR_PAPER, 8);
    lv_obj_set_style_border_color(reader, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(reader, 1, 0);
    lv_obj_set_style_pad_all(reader, 0, 0);

    size_t offset = 0;
    size_t length = 0;
    char *page_text = NULL;
    if (linepods_pager_slice(pager, page_index, &offset, &length)) {
        page_text = malloc(length + 1);
        if (page_text) {
            memcpy(page_text, text + offset, length);
            page_text[length] = '\0';
        }
    }
    lv_obj_t *body = body_label(reader,
        page_text ? page_text : "这一页暂时无法显示", COLOR_INK);
    free(page_text);
    lv_obj_set_size(body, DETAIL_TEXT_WIDTH, 184);
    lv_obj_set_pos(body, 12, 10);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(body, 1, 0);

    add_separator(reader, 12, 201, DETAIL_TEXT_WIDTH);
    char counter[24];
    snprintf(counter, sizeof(counter), "%02u/%02u",
             (unsigned)(index + 1), (unsigned)content->item_count);
    lv_obj_t *count = label(reader, counter, COLOR_ACCENT);
    lv_obj_set_size(count, 54, 20);
    lv_obj_set_pos(count, 12, 207);

    lv_obj_t *hint = label(reader, "上下翻页", COLOR_MUTED);
    lv_obj_set_size(hint, 82, 20);
    lv_obj_set_pos(hint, 67, 207);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

    char pages[24];
    snprintf(pages, sizeof(pages), "%02u/%02u",
             (unsigned)(page_index + 1), (unsigned)pager->page_count);
    lv_obj_t *page_count = label(reader, pages, COLOR_MUTED);
    lv_obj_set_size(page_count, 54, 20);
    lv_obj_set_pos(page_count, 150, 207);
    lv_obj_set_style_text_align(page_count, LV_TEXT_ALIGN_RIGHT, 0);
    load_screen(screen);
}

void linepods_ui_show_error(const char *message)
{
    lv_obj_t *page;
    lv_obj_t *screen = new_screen(&page);

    lv_obj_t *stamp = label(page, "暂时没取到内容", COLOR_RED);
    set_single_line(stamp, 24);

    lv_obj_t *card = lv_obj_create(page);
    lv_obj_set_size(card, lv_pct(100), 160);
    style_panel(card, COLOR_SURFACE, 8);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_t *detail = body_label(card, message ? message : "发生未知错误", COLOR_INK);
    lv_obj_set_size(detail, lv_pct(100), 130);
    lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(detail, 5, 0);

    add_footer(page, "确定重试　长按上键设置");
    load_screen(screen);
}

void linepods_ui_show_font_compare(size_t index)
{
    static const lv_font_t *const fonts[LINEPODS_FONT_COMPARE_COUNT] = {
        &linepods_font_ui_18,
        &linepods_font_compare_noto_20_1,
        &linepods_font_body_20,
        &linepods_font_compare_pixel_24_1,
    };
    static const char *const descriptions[LINEPODS_FONT_COMPARE_COUNT] = {
        "1/4 旧版方案\nNoto 18px · 1bpp · light",
        "2/4 大字锐利\nNoto 20px · 1bpp · strong",
        "3/4 当前方案\nNoto 20px · 2bpp · strong",
        "4/4 像素方案\nFusion 24px · 1bpp · 2x",
    };
    static const char *const sample =
        "阅读清晰度测试\n"
        "复杂：赢藏鬓薄曜\n"
        "在具体问题中\n"
        "重要的是清晰\n"
        "辨认每一笔。\n"
        "Linepods 2026 · Aa";

    index %= LINEPODS_FONT_COMPARE_COUNT;
    lv_obj_t *page;
    lv_obj_t *screen = new_screen(&page);

    lv_obj_t *description = label(page, descriptions[index], COLOR_ACCENT);
    lv_obj_set_size(description, lv_pct(100), 50);
    lv_label_set_long_mode(description, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(description, 0, 0);

    lv_obj_t *reader = lv_obj_create(page);
    lv_obj_set_size(reader, lv_pct(100), 180);
    style_panel(reader, COLOR_PAPER, 8);
    lv_obj_set_style_border_color(reader, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(reader, 1, 0);
    lv_obj_set_style_pad_all(reader, 0, 0);

    lv_obj_t *sample_label = lv_label_create(reader);
    lv_obj_set_style_text_font(sample_label, fonts[index], 0);
    lv_obj_set_style_text_color(sample_label, lv_color_hex(COLOR_INK), 0);
    lv_obj_set_size(sample_label, 192, 158);
    lv_obj_set_pos(sample_label, 12, 8);
    lv_label_set_long_mode(sample_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(sample_label, 0, 0);
    lv_label_set_text(sample_label, sample);

    add_footer(page, "上下切换　长按确定返回");
    load_screen(screen);
}

void linepods_ui_set_battery(int percent)
{
    s_battery_percent = percent;
    unsigned active = linepods_status_battery_segments(percent);
    bool low = percent >= 0 && percent <= 20;
    for (unsigned i = 0; i < LINEPODS_BATTERY_SEGMENT_COUNT; i++) {
        if (!s_battery_segments[i]) continue;
        bool filled = i < active;
        lv_obj_set_style_bg_color(
            s_battery_segments[i],
            lv_color_hex(filled ? (low ? COLOR_INK : COLOR_ACCENT)
                                : COLOR_ACCENT_SOFT), 0);
        lv_obj_set_style_bg_opa(s_battery_segments[i],
                                filled ? LV_OPA_COVER : LV_OPA_30, 0);
    }
}

void linepods_ui_set_time(time_t timestamp)
{
    s_system_time = timestamp;
    if (!s_time_label) return;
    char text[6];
    (void)linepods_status_format_time(timestamp, text);
    lv_label_set_text(s_time_label, text);
}
