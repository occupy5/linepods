#include "linepods_pager.h"

#include <string.h>

static size_t decode_utf8(const char *text, size_t remaining, uint32_t *codepoint)
{
    const unsigned char *bytes = (const unsigned char *)text;
    if (!remaining || !bytes[0]) return 0;
    if (bytes[0] < 0x80) {
        *codepoint = bytes[0];
        return 1;
    }
    if ((bytes[0] & 0xE0) == 0xC0 && remaining >= 2
        && (bytes[1] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(bytes[0] & 0x1F) << 6)
                   | (uint32_t)(bytes[1] & 0x3F);
        return 2;
    }
    if ((bytes[0] & 0xF0) == 0xE0 && remaining >= 3
        && (bytes[1] & 0xC0) == 0x80 && (bytes[2] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(bytes[0] & 0x0F) << 12)
                   | ((uint32_t)(bytes[1] & 0x3F) << 6)
                   | (uint32_t)(bytes[2] & 0x3F);
        return 3;
    }
    if ((bytes[0] & 0xF8) == 0xF0 && remaining >= 4
        && (bytes[1] & 0xC0) == 0x80 && (bytes[2] & 0xC0) == 0x80
        && (bytes[3] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(bytes[0] & 0x07) << 18)
                   | ((uint32_t)(bytes[1] & 0x3F) << 12)
                   | ((uint32_t)(bytes[2] & 0x3F) << 6)
                   | (uint32_t)(bytes[3] & 0x3F);
        return 4;
    }
    *codepoint = 0xFFFD;
    return 1;
}

static bool start_page(linepods_pager_t *pager, size_t offset)
{
    if (pager->page_count >= LINEPODS_PAGER_MAX_PAGES) return false;
    pager->offsets[pager->page_count] = offset;
    pager->page_count++;
    return true;
}

bool linepods_pager_build(linepods_pager_t *pager, const char *text,
                        uint16_t line_width, uint16_t lines_per_page,
                        linepods_pager_glyph_width_fn glyph_width,
                        void *glyph_context)
{
    if (!pager || !text || !line_width || !lines_per_page || !glyph_width) {
        return false;
    }
    *pager = (linepods_pager_t) {0};
    size_t text_length = strlen(text);
    pager->offsets[0] = 0;
    pager->page_count = 1;
    if (text_length == 0) {
        pager->offsets[1] = 0;
        return true;
    }

    size_t cursor = 0;
    uint16_t line = 1;
    uint16_t width = 0;
    while (cursor < text_length) {
        uint32_t codepoint = 0;
        size_t sequence = decode_utf8(text + cursor, text_length - cursor,
                                      &codepoint);
        if (!sequence) break;

        uint32_t next = 0;
        if (cursor + sequence < text_length) {
            (void)decode_utf8(text + cursor + sequence,
                              text_length - cursor - sequence, &next);
        }

        if (codepoint == '\r') {
            cursor += sequence;
            continue;
        }
        if (codepoint == '\n') {
            cursor += sequence;
            width = 0;
            line++;
            if (line > lines_per_page && cursor < text_length) {
                if (!start_page(pager, cursor)) return false;
                line = 1;
            }
            continue;
        }

        uint16_t advance = glyph_width(codepoint, next, glyph_context);
        if (advance == 0) advance = 1;
        if (width > 0 && (uint32_t)width + advance > line_width) {
            line++;
            width = 0;
            if (line > lines_per_page) {
                if (!start_page(pager, cursor)) return false;
                line = 1;
            }
        }
        width = (uint16_t)(width + advance);
        cursor += sequence;
    }
    pager->offsets[pager->page_count] = text_length;
    return true;
}

bool linepods_pager_slice(const linepods_pager_t *pager, size_t page_index,
                        size_t *offset, size_t *length)
{
    if (!pager || !offset || !length || page_index >= pager->page_count) {
        return false;
    }
    *offset = pager->offsets[page_index];
    *length = pager->offsets[page_index + 1] - pager->offsets[page_index];
    return true;
}
