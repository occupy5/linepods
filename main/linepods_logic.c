#include "linepods_logic.h"

#include <string.h>

void linepods_logic_init(linepods_logic_t *logic)
{
    if (!logic) return;
    *logic = (linepods_logic_t) {
        .view = LINEPODS_VIEW_WELCOME,
        .quote_count = 0,
        .quote_index = 0,
        .browse_mode = LINEPODS_BROWSE_ALL,
    };
}

void linepods_logic_set_quotes(linepods_logic_t *logic, size_t quote_count)
{
    if (!logic) return;
    logic->quote_count = quote_count;
    logic->quote_index = 0;
    logic->range_start = 0;
    logic->range_count = quote_count;
    logic->detail_page_index = 0;
    logic->detail_page_count = 0;
    logic->browse_menu_index = 0;
    logic->book_index = 0;
    logic->settings_index = 0;
    logic->browse_mode = LINEPODS_BROWSE_ALL;
    logic->detail_open_from_end = false;
    logic->view = quote_count > 0 ? LINEPODS_VIEW_CARD : LINEPODS_VIEW_ERROR;
}

void linepods_logic_open_setting_value(linepods_logic_t *logic,
                                      linepods_setting_t setting,
                                      size_t value_index, size_t value_count)
{
    if (!logic || value_count == 0) return;
    logic->pending_setting = setting;
    logic->settings_value_count = value_count;
    logic->settings_value_index = value_index < value_count ? value_index : 0;
    logic->view = LINEPODS_VIEW_SETTINGS_VALUE;
}

static size_t settings_item_count(size_t section)
{
    return section == LINEPODS_SETTINGS_SECTION_CONNECTION ? 3 : 2;
}

static linepods_setting_t selected_setting(const linepods_logic_t *logic)
{
    if (logic->settings_index == LINEPODS_SETTINGS_SECTION_CONNECTION) {
        static const linepods_setting_t items[] = {
            LINEPODS_SETTING_UPDATE_WIFI,
            LINEPODS_SETTING_UPDATE_API_KEY,
            LINEPODS_SETTING_CLEAR_CONFIG,
        };
        return items[logic->settings_item_index % 3];
    }
    if (logic->settings_index == LINEPODS_SETTINGS_SECTION_POWER) {
        static const linepods_setting_t items[] = {
            LINEPODS_SETTING_SCREEN_TIMEOUT,
            LINEPODS_SETTING_DEEP_SLEEP,
        };
        return items[logic->settings_item_index % 2];
    }
    static const linepods_setting_t items[] = {
        LINEPODS_SETTING_INFO,
        LINEPODS_SETTING_CLEAR_CACHE,
    };
    return items[logic->settings_item_index % 2];
}

void linepods_logic_set_all_scope(linepods_logic_t *logic)
{
    if (!logic) return;
    logic->browse_mode = LINEPODS_BROWSE_ALL;
    logic->range_start = 0;
    logic->range_count = logic->quote_count;
    if (logic->quote_index >= logic->quote_count) logic->quote_index = 0;
}

void linepods_logic_set_book_scope(linepods_logic_t *logic, size_t range_start,
                                  size_t range_count, size_t book_index)
{
    if (!logic || range_count == 0 || range_start >= logic->quote_count
        || range_count > logic->quote_count - range_start) {
        return;
    }
    logic->browse_mode = LINEPODS_BROWSE_BOOK;
    logic->range_start = range_start;
    logic->range_count = range_count;
    logic->book_index = book_index;
    logic->quote_index = range_start;
}

void linepods_logic_set_book_count(linepods_logic_t *logic, size_t book_count)
{
    if (!logic) return;
    logic->book_count = book_count;
    if (book_count == 0) logic->book_index = 0;
    else if (logic->book_index >= book_count) logic->book_index = book_count - 1;
}

bool linepods_logic_set_quote_index(linepods_logic_t *logic, size_t quote_index)
{
    if (!logic || quote_index >= logic->quote_count
        || quote_index < logic->range_start
        || quote_index >= logic->range_start + logic->range_count) {
        return false;
    }
    logic->quote_index = quote_index;
    return true;
}

size_t linepods_logic_select_random(linepods_logic_t *logic, uint32_t random_value)
{
    if (!logic || logic->quote_count == 0) return 0;
    linepods_logic_set_all_scope(logic);
    if (logic->range_count == 1) {
        logic->quote_index = 0;
        return 0;
    }
    size_t candidate = random_value % (logic->range_count - 1);
    if (candidate >= logic->quote_index) candidate++;
    logic->quote_index = candidate;
    return candidate;
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
            size_t offset = logic->quote_index - logic->range_start;
            offset = (offset + logic->range_count - 1) % logic->range_count;
            logic->quote_index = logic->range_start + offset;
            return LINEPODS_ACTION_SHOW_CARD;
        }
        if (input == LINEPODS_INPUT_DOWN_CLICK) {
            size_t offset = logic->quote_index - logic->range_start;
            logic->quote_index = logic->range_start
                               + (offset + 1) % logic->range_count;
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
        if (input == LINEPODS_INPUT_OK_LONG) {
            logic->view = LINEPODS_VIEW_SETTINGS;
            logic->settings_index = 0;
            return LINEPODS_ACTION_SHOW_SETTINGS;
        }
        if (input == LINEPODS_INPUT_DOWN_LONG) {
            logic->view = LINEPODS_VIEW_BROWSE_MENU;
            logic->browse_menu_index = 0;
            return LINEPODS_ACTION_SHOW_BROWSE_MENU;
        }
    }

    if (logic->view == LINEPODS_VIEW_DETAIL && logic->quote_count > 0) {
        if (input == LINEPODS_INPUT_UP_CLICK) {
            if (logic->detail_page_index > 0) {
                logic->detail_page_index--;
                return LINEPODS_ACTION_SHOW_DETAIL;
            }
            size_t offset = logic->quote_index - logic->range_start;
            offset = (offset + logic->range_count - 1) % logic->range_count;
            logic->quote_index = logic->range_start + offset;
            logic->detail_page_count = 0;
            logic->detail_open_from_end = true;
            return LINEPODS_ACTION_SHOW_DETAIL_NEW_ITEM;
        }
        if (input == LINEPODS_INPUT_DOWN_CLICK) {
            if (logic->detail_page_index + 1 < logic->detail_page_count) {
                logic->detail_page_index++;
                return LINEPODS_ACTION_SHOW_DETAIL;
            }
            size_t offset = logic->quote_index - logic->range_start;
            logic->quote_index = logic->range_start
                               + (offset + 1) % logic->range_count;
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

    if (logic->view == LINEPODS_VIEW_BROWSE_MENU) {
        if (input == LINEPODS_INPUT_UP_CLICK) {
            logic->browse_menu_index =
                (logic->browse_menu_index + LINEPODS_READING_OPTION_COUNT - 1)
                % LINEPODS_READING_OPTION_COUNT;
            return LINEPODS_ACTION_SHOW_BROWSE_MENU;
        }
        if (input == LINEPODS_INPUT_DOWN_CLICK) {
            logic->browse_menu_index =
                (logic->browse_menu_index + 1) % LINEPODS_READING_OPTION_COUNT;
            return LINEPODS_ACTION_SHOW_BROWSE_MENU;
        }
        if (input == LINEPODS_INPUT_OK_CLICK) {
            if (logic->browse_menu_index == 0) {
                logic->view = LINEPODS_VIEW_CARD;
                return LINEPODS_ACTION_SELECT_RANDOM;
            }
            if (logic->browse_menu_index == 1) {
                logic->view = LINEPODS_VIEW_CARD;
                return LINEPODS_ACTION_SELECT_ALL;
            }
            if (logic->browse_menu_index == 2) {
                logic->view = LINEPODS_VIEW_BOOK_PICKER;
                return LINEPODS_ACTION_SHOW_BOOK_PICKER;
            }
        }
        if (input == LINEPODS_INPUT_OK_LONG) {
            logic->view = LINEPODS_VIEW_CARD;
            return LINEPODS_ACTION_SHOW_CARD;
        }
    }

    if (logic->view == LINEPODS_VIEW_SETTINGS) {
        if (input == LINEPODS_INPUT_UP_CLICK) {
            logic->settings_index =
                (logic->settings_index + LINEPODS_SETTINGS_SECTION_COUNT - 1)
                % LINEPODS_SETTINGS_SECTION_COUNT;
            return LINEPODS_ACTION_SHOW_SETTINGS;
        }
        if (input == LINEPODS_INPUT_DOWN_CLICK) {
            logic->settings_index = (logic->settings_index + 1)
                                  % LINEPODS_SETTINGS_SECTION_COUNT;
            return LINEPODS_ACTION_SHOW_SETTINGS;
        }
        if (input == LINEPODS_INPUT_OK_CLICK) {
            logic->settings_item_index = 0;
            logic->view = LINEPODS_VIEW_SETTINGS_ITEMS;
            return LINEPODS_ACTION_SHOW_SETTINGS;
        }
        if (input == LINEPODS_INPUT_OK_LONG) {
            logic->view = LINEPODS_VIEW_CARD;
            return LINEPODS_ACTION_SHOW_CARD;
        }
    }

    if (logic->view == LINEPODS_VIEW_SETTINGS_ITEMS) {
        size_t item_count = settings_item_count(logic->settings_index);
        if (input == LINEPODS_INPUT_UP_CLICK) {
            logic->settings_item_index =
                (logic->settings_item_index + item_count - 1) % item_count;
            return LINEPODS_ACTION_SHOW_SETTINGS;
        }
        if (input == LINEPODS_INPUT_DOWN_CLICK) {
            logic->settings_item_index = (logic->settings_item_index + 1)
                                       % item_count;
            return LINEPODS_ACTION_SHOW_SETTINGS;
        }
        if (input == LINEPODS_INPUT_OK_CLICK) {
            linepods_setting_t setting = selected_setting(logic);
            switch (setting) {
            case LINEPODS_SETTING_UPDATE_WIFI:
                return LINEPODS_ACTION_UPDATE_WIFI;
            case LINEPODS_SETTING_UPDATE_API_KEY:
                return LINEPODS_ACTION_UPDATE_API_KEY;
            case LINEPODS_SETTING_CLEAR_CONFIG:
            case LINEPODS_SETTING_CLEAR_CACHE:
                logic->pending_setting = setting;
                logic->confirm_index = 0;
                logic->view = LINEPODS_VIEW_SETTINGS_CONFIRM;
                return LINEPODS_ACTION_SHOW_SETTINGS;
            case LINEPODS_SETTING_SCREEN_TIMEOUT:
                return LINEPODS_ACTION_EDIT_SCREEN_TIMEOUT;
            case LINEPODS_SETTING_DEEP_SLEEP:
                return LINEPODS_ACTION_EDIT_DEEP_SLEEP;
            case LINEPODS_SETTING_INFO:
                logic->view = LINEPODS_VIEW_SETTINGS_INFO;
                return LINEPODS_ACTION_SHOW_SETTINGS_INFO;
            }
        }
        if (input == LINEPODS_INPUT_OK_LONG) {
            logic->view = LINEPODS_VIEW_SETTINGS;
            return LINEPODS_ACTION_SHOW_SETTINGS;
        }
    }

    if (logic->view == LINEPODS_VIEW_SETTINGS_CONFIRM) {
        if (input == LINEPODS_INPUT_UP_CLICK || input == LINEPODS_INPUT_DOWN_CLICK) {
            logic->confirm_index = logic->confirm_index ? 0 : 1;
            return LINEPODS_ACTION_SHOW_SETTINGS;
        }
        if (input == LINEPODS_INPUT_OK_CLICK) {
            logic->view = LINEPODS_VIEW_SETTINGS_ITEMS;
            if (logic->confirm_index == 0) return LINEPODS_ACTION_SHOW_SETTINGS;
            return logic->pending_setting == LINEPODS_SETTING_CLEAR_CACHE
                 ? LINEPODS_ACTION_CLEAR_CACHE : LINEPODS_ACTION_CLEAR_CONFIG;
        }
        if (input == LINEPODS_INPUT_OK_LONG) {
            logic->view = LINEPODS_VIEW_SETTINGS_ITEMS;
            return LINEPODS_ACTION_SHOW_SETTINGS;
        }
    }

    if (logic->view == LINEPODS_VIEW_SETTINGS_VALUE
        && logic->settings_value_count > 0) {
        if (input == LINEPODS_INPUT_UP_CLICK) {
            logic->settings_value_index =
                (logic->settings_value_index + logic->settings_value_count - 1)
                % logic->settings_value_count;
            return LINEPODS_ACTION_SHOW_SETTINGS_VALUE;
        }
        if (input == LINEPODS_INPUT_DOWN_CLICK) {
            logic->settings_value_index = (logic->settings_value_index + 1)
                                        % logic->settings_value_count;
            return LINEPODS_ACTION_SHOW_SETTINGS_VALUE;
        }
        if (input == LINEPODS_INPUT_OK_CLICK) {
            logic->view = LINEPODS_VIEW_SETTINGS_ITEMS;
            return logic->pending_setting == LINEPODS_SETTING_DEEP_SLEEP
                 ? LINEPODS_ACTION_SAVE_DEEP_SLEEP
                 : LINEPODS_ACTION_SAVE_SCREEN_TIMEOUT;
        }
        if (input == LINEPODS_INPUT_OK_LONG) {
            logic->view = LINEPODS_VIEW_SETTINGS_ITEMS;
            return LINEPODS_ACTION_SHOW_SETTINGS;
        }
    }

    if (logic->view == LINEPODS_VIEW_SETTINGS_INFO
        && input == LINEPODS_INPUT_OK_LONG) {
        logic->view = LINEPODS_VIEW_SETTINGS_ITEMS;
        return LINEPODS_ACTION_SHOW_SETTINGS;
    }

    if (logic->view == LINEPODS_VIEW_BOOK_PICKER && logic->book_count > 0) {
        if (input == LINEPODS_INPUT_UP_CLICK) {
            logic->book_index = (logic->book_index + logic->book_count - 1)
                              % logic->book_count;
            return LINEPODS_ACTION_SHOW_BOOK_PICKER;
        }
        if (input == LINEPODS_INPUT_DOWN_CLICK) {
            logic->book_index = (logic->book_index + 1) % logic->book_count;
            return LINEPODS_ACTION_SHOW_BOOK_PICKER;
        }
        if (input == LINEPODS_INPUT_OK_CLICK) {
            logic->view = LINEPODS_VIEW_CARD;
            return LINEPODS_ACTION_SELECT_BOOK;
        }
        if (input == LINEPODS_INPUT_OK_LONG) {
            logic->view = LINEPODS_VIEW_BROWSE_MENU;
            return LINEPODS_ACTION_SHOW_BROWSE_MENU;
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
