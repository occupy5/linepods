#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LINEPODS_FONT_COMPARE_COUNT 4

typedef enum {
    LINEPODS_VIEW_WELCOME = 0,
    LINEPODS_VIEW_SETUP,
    LINEPODS_VIEW_LOADING,
    LINEPODS_VIEW_CARD,
    LINEPODS_VIEW_DETAIL,
    LINEPODS_VIEW_FONT_COMPARE,
    LINEPODS_VIEW_SYNC_SUCCESS,
    LINEPODS_VIEW_ERROR,
} linepods_view_t;

typedef enum {
    LINEPODS_INPUT_UP_CLICK = 0,
    LINEPODS_INPUT_DOWN_CLICK,
    LINEPODS_INPUT_OK_CLICK,
    LINEPODS_INPUT_UP_LONG,
    LINEPODS_INPUT_DOWN_LONG,
    LINEPODS_INPUT_OK_LONG,
} linepods_input_t;

typedef enum {
    LINEPODS_ACTION_NONE = 0,
    LINEPODS_ACTION_SHOW_CARD,
    LINEPODS_ACTION_SHOW_DETAIL,
    LINEPODS_ACTION_SHOW_DETAIL_NEW_ITEM,
    LINEPODS_ACTION_SHOW_FONT_COMPARE,
    LINEPODS_ACTION_SYNC,
    LINEPODS_ACTION_RETRY,
    LINEPODS_ACTION_START_SETUP,
} linepods_action_t;

typedef struct {
    linepods_view_t view;
    size_t quote_count;
    size_t quote_index;
    size_t detail_page_index;
    size_t detail_page_count;
    size_t font_compare_index;
    bool detail_open_from_end;
} linepods_logic_t;

void linepods_logic_init(linepods_logic_t *logic);
void linepods_logic_set_quotes(linepods_logic_t *logic, size_t quote_count);
void linepods_logic_set_view(linepods_logic_t *logic, linepods_view_t view);
linepods_action_t linepods_logic_handle(linepods_logic_t *logic, linepods_input_t input);

/* Supplies the page count after the newly selected quote has been loaded and
 * paginated. Entering it through UP selects its last page; all other paths
 * select the first page. */
void linepods_logic_detail_ready(linepods_logic_t *logic, size_t page_count);

/* Copies UTF-8 without splitting the final code point. The result is always
 * NUL-terminated when destination_size is non-zero. Returns copied bytes. */
size_t linepods_utf8_copy(char *destination, size_t destination_size,
                        const char *source);
