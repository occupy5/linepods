#include "linepods_logic.h"

#include <string.h>

void linepods_logic_init(linepods_logic_t *logic)
{
    if (!logic) return;
    *logic = (linepods_logic_t) {
        .view = LINEPODS_VIEW_WELCOME,
        .quote_count = 0,
        .quote_index = 0,
    };
}

void linepods_logic_set_quotes(linepods_logic_t *logic, size_t quote_count)
{
    if (!logic) return;
    logic->quote_count = quote_count;
    logic->quote_index = 0;
    logic->detail_page_index = 0;
    logic->detail_page_count = 0;
    logic->font_compare_index = 0;
    logic->detail_open_from_end = false;
    logic->view = quote_count > 0 ? LINEPODS_VIEW_CARD : LINEPODS_VIEW_ERROR;
}

void linepods_logic_set_view(linepods_logic_t *logic, linepods_view_t view)
{
    if (logic) logic->view = view;
}

linepods_action_t linepods_logic_handle(linepods_logic_t *logic, linepods_input_t input)
{
    if (!logic) return LINEPODS_ACTION_NONE;

    if (logic->view == LINEPODS_VIEW_WELCOME) {
        if (input == LINEPODS_INPUT_OK_CLICK) {
            return LINEPODS_ACTION_START_SETUP;
        }
    }

    if (logic->view == LINEPODS_VIEW_SYNC_SUCCESS) {
        if (input == LINEPODS_INPUT_OK_CLICK) {
            logic->view = LINEPODS_VIEW_CARD;
            return LINEPODS_ACTION_SHOW_CARD;
        }
    }

    if (logic->view == LINEPODS_VIEW_CARD && logic->quote_count > 0) {
        if (input == LINEPODS_INPUT_UP_CLICK) {
            logic->quote_index = (logic->quote_index + logic->quote_count - 1)
                               % logic->quote_count;
            return LINEPODS_ACTION_SHOW_CARD;
        }
        if (input == LINEPODS_INPUT_DOWN_CLICK) {
            logic->quote_index = (logic->quote_index + 1) % logic->quote_count;
            return LINEPODS_ACTION_SHOW_CARD;
        }
        if (input == LINEPODS_INPUT_OK_CLICK) {
            logic->view = LINEPODS_VIEW_DETAIL;
            logic->detail_page_index = 0;
            logic->detail_page_count = 0;
            logic->detail_open_from_end = false;
            return LINEPODS_ACTION_SHOW_DETAIL_NEW_ITEM;
        }
        if (input == LINEPODS_INPUT_UP_LONG) return LINEPODS_ACTION_SYNC;
        if (input == LINEPODS_INPUT_OK_LONG) return LINEPODS_ACTION_START_SETUP;
        if (input == LINEPODS_INPUT_DOWN_LONG) {
            logic->view = LINEPODS_VIEW_FONT_COMPARE;
            logic->font_compare_index = 0;
            return LINEPODS_ACTION_SHOW_FONT_COMPARE;
        }
    }

    if (logic->view == LINEPODS_VIEW_DETAIL && logic->quote_count > 0) {
        if (input == LINEPODS_INPUT_UP_CLICK) {
            if (logic->detail_page_index > 0) {
                logic->detail_page_index--;
                return LINEPODS_ACTION_SHOW_DETAIL;
            }
            logic->quote_index = (logic->quote_index + logic->quote_count - 1)
                               % logic->quote_count;
            logic->detail_page_count = 0;
            logic->detail_open_from_end = true;
            return LINEPODS_ACTION_SHOW_DETAIL_NEW_ITEM;
        }
        if (input == LINEPODS_INPUT_DOWN_CLICK) {
            if (logic->detail_page_index + 1 < logic->detail_page_count) {
                logic->detail_page_index++;
                return LINEPODS_ACTION_SHOW_DETAIL;
            }
            logic->quote_index = (logic->quote_index + 1) % logic->quote_count;
            logic->detail_page_index = 0;
            logic->detail_page_count = 0;
            logic->detail_open_from_end = false;
            return LINEPODS_ACTION_SHOW_DETAIL_NEW_ITEM;
        }
        if (input == LINEPODS_INPUT_OK_LONG) {
            logic->view = LINEPODS_VIEW_CARD;
            logic->detail_page_index = 0;
            logic->detail_page_count = 0;
            logic->detail_open_from_end = false;
            return LINEPODS_ACTION_SHOW_CARD;
        }
    }

    if (logic->view == LINEPODS_VIEW_FONT_COMPARE) {
        if (input == LINEPODS_INPUT_UP_CLICK) {
            logic->font_compare_index =
                (logic->font_compare_index + LINEPODS_FONT_COMPARE_COUNT - 1)
                % LINEPODS_FONT_COMPARE_COUNT;
            return LINEPODS_ACTION_SHOW_FONT_COMPARE;
        }
        if (input == LINEPODS_INPUT_DOWN_CLICK) {
            logic->font_compare_index = (logic->font_compare_index + 1)
                                      % LINEPODS_FONT_COMPARE_COUNT;
            return LINEPODS_ACTION_SHOW_FONT_COMPARE;
        }
        if (input == LINEPODS_INPUT_OK_LONG) {
            logic->view = LINEPODS_VIEW_CARD;
            return LINEPODS_ACTION_SHOW_CARD;
        }
    }

    if (logic->view == LINEPODS_VIEW_ERROR) {
        if (input == LINEPODS_INPUT_OK_CLICK) return LINEPODS_ACTION_RETRY;
        if (input == LINEPODS_INPUT_UP_LONG) return LINEPODS_ACTION_START_SETUP;
    }

    return LINEPODS_ACTION_NONE;
}

void linepods_logic_detail_ready(linepods_logic_t *logic, size_t page_count)
{
    if (!logic) return;
    logic->detail_page_count = page_count > 0 ? page_count : 1;
    logic->detail_page_index = logic->detail_open_from_end
                             ? logic->detail_page_count - 1 : 0;
    logic->detail_open_from_end = false;
}

static size_t utf8_sequence_length(unsigned char lead)
{
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

size_t linepods_utf8_copy(char *destination, size_t destination_size,
                        const char *source)
{
    if (!destination || destination_size == 0) return 0;
    destination[0] = '\0';
    if (!source) return 0;

    size_t written = 0;
    const unsigned char *cursor = (const unsigned char *)source;
    while (*cursor) {
        size_t sequence = utf8_sequence_length(*cursor);
        size_t available = strlen((const char *)cursor);
        if (sequence > available || written + sequence >= destination_size) break;

        bool valid = true;
        for (size_t i = 1; i < sequence; i++) {
            if ((cursor[i] & 0xC0) != 0x80) {
                valid = false;
                break;
            }
        }
        if (!valid) sequence = 1;

        memcpy(destination + written, cursor, sequence);
        written += sequence;
        cursor += sequence;
    }
    destination[written] = '\0';
    return written;
}
