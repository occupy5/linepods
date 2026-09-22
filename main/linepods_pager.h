#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LINEPODS_PAGER_MAX_PAGES 512

typedef uint16_t (*linepods_pager_glyph_width_fn)(uint32_t codepoint,
                                                 uint32_t next_codepoint,
                                                 void *context);

typedef struct {
    size_t offsets[LINEPODS_PAGER_MAX_PAGES + 1];
    size_t page_count;
} linepods_pager_t;

/* Splits UTF-8 text into display pages without cutting a code point. Widths
 * come from the active display font, keeping the pure pagination policy
 * independent from LVGL and testable on the host. */
bool linepods_pager_build(linepods_pager_t *pager, const char *text,
                        uint16_t line_width, uint16_t lines_per_page,
                        linepods_pager_glyph_width_fn glyph_width,
                        void *glyph_context);

bool linepods_pager_slice(const linepods_pager_t *pager, size_t page_index,
                        size_t *offset, size_t *length);
