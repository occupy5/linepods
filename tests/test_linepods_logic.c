#include "linepods_logic.h"

#include <assert.h>
#include <string.h>

static void test_first_run_starts_setup_only_after_confirmation(void)
{
    linepods_logic_t logic;
    linepods_logic_init(&logic);

    assert(logic.view == LINEPODS_VIEW_WELCOME);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_UP_CLICK)
           == LINEPODS_ACTION_NONE);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_START_SETUP);
}

static void test_sync_success_waits_for_confirmation(void)
{
    linepods_logic_t logic;
    linepods_logic_init(&logic);
    linepods_logic_set_quotes(&logic, 3);
    linepods_logic_set_view(&logic, LINEPODS_VIEW_SYNC_SUCCESS);

    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_CLICK)
           == LINEPODS_ACTION_NONE);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_SHOW_CARD);
    assert(logic.view == LINEPODS_VIEW_CARD);
}

static void test_card_navigation_wraps(void)
{
    linepods_logic_t logic;
    linepods_logic_init(&logic);
    linepods_logic_set_quotes(&logic, 3);

    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_UP_CLICK) == LINEPODS_ACTION_SHOW_CARD);
    assert(logic.quote_index == 2);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_CLICK) == LINEPODS_ACTION_SHOW_CARD);
    assert(logic.quote_index == 0);
}

static void test_detail_pages_then_moves_between_quotes(void)
{
    linepods_logic_t logic;
    linepods_logic_init(&logic);
    linepods_logic_set_quotes(&logic, 3);

    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_SHOW_DETAIL_NEW_ITEM);
    assert(logic.view == LINEPODS_VIEW_DETAIL);
    linepods_logic_detail_ready(&logic, 3);
    assert(logic.detail_page_index == 0);

    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_CLICK) == LINEPODS_ACTION_SHOW_DETAIL);
    assert(logic.detail_page_index == 1);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_CLICK) == LINEPODS_ACTION_SHOW_DETAIL);
    assert(logic.detail_page_index == 2);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_CLICK)
           == LINEPODS_ACTION_SHOW_DETAIL_NEW_ITEM);
    assert(logic.quote_index == 1);
    linepods_logic_detail_ready(&logic, 2);
    assert(logic.detail_page_index == 0);

    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_UP_CLICK)
           == LINEPODS_ACTION_SHOW_DETAIL_NEW_ITEM);
    assert(logic.quote_index == 0);
    linepods_logic_detail_ready(&logic, 3);
    assert(logic.detail_page_index == 2);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_LONG) == LINEPODS_ACTION_SHOW_CARD);
    assert(logic.view == LINEPODS_VIEW_CARD);
}

static void test_error_actions(void)
{
    linepods_logic_t logic;
    linepods_logic_init(&logic);

    linepods_logic_set_view(&logic, LINEPODS_VIEW_ERROR);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK) == LINEPODS_ACTION_RETRY);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_UP_LONG) == LINEPODS_ACTION_START_SETUP);
}

static void test_browse_menu_and_book_picker(void)
{
    linepods_logic_t logic;
    linepods_logic_init(&logic);
    linepods_logic_set_quotes(&logic, 3);
    linepods_logic_set_book_count(&logic, 2);

    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_LONG)
           == LINEPODS_ACTION_SHOW_BROWSE_MENU);
    assert(logic.view == LINEPODS_VIEW_BROWSE_MENU);
    assert(logic.browse_menu_index == 0);

    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_CLICK)
           == LINEPODS_ACTION_SHOW_BROWSE_MENU);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_CLICK)
           == LINEPODS_ACTION_SHOW_BROWSE_MENU);
    assert(logic.browse_menu_index == 2);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_SHOW_BOOK_PICKER);
    assert(logic.view == LINEPODS_VIEW_BOOK_PICKER);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_CLICK)
           == LINEPODS_ACTION_SHOW_BOOK_PICKER);
    assert(logic.book_index == 1);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_SELECT_BOOK);

    logic.view = LINEPODS_VIEW_BOOK_PICKER;
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_LONG)
           == LINEPODS_ACTION_SHOW_BROWSE_MENU);
    assert(logic.view == LINEPODS_VIEW_BROWSE_MENU);
}

static void test_settings_navigation_and_confirmations(void)
{
    linepods_logic_t logic;
    linepods_logic_init(&logic);
    linepods_logic_set_quotes(&logic, 3);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_LONG)
           == LINEPODS_ACTION_SHOW_SETTINGS);
    assert(logic.view == LINEPODS_VIEW_SETTINGS);

    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_SHOW_SETTINGS);
    assert(logic.view == LINEPODS_VIEW_SETTINGS_ITEMS);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_UPDATE_WIFI);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_LONG)
           == LINEPODS_ACTION_SHOW_SETTINGS);
    assert(logic.view == LINEPODS_VIEW_SETTINGS);

    logic.settings_index = LINEPODS_SETTINGS_SECTION_DATA;
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_SHOW_SETTINGS);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_CLICK)
           == LINEPODS_ACTION_SHOW_SETTINGS);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_SHOW_SETTINGS);
    assert(logic.view == LINEPODS_VIEW_SETTINGS_CONFIRM);
    assert(logic.confirm_index == 0);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_CLICK)
           == LINEPODS_ACTION_SHOW_SETTINGS);
    assert(logic.confirm_index == 1);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_CLEAR_CACHE);

    logic.view = LINEPODS_VIEW_SETTINGS;
    logic.settings_index = LINEPODS_SETTINGS_SECTION_POWER;
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_SHOW_SETTINGS);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_EDIT_SCREEN_TIMEOUT);
    linepods_logic_open_setting_value(&logic, LINEPODS_SETTING_SCREEN_TIMEOUT,
                                      1, 4);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_CLICK)
           == LINEPODS_ACTION_SHOW_SETTINGS_VALUE);
    assert(logic.settings_value_index == 2);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_SAVE_SCREEN_TIMEOUT);
    assert(logic.view == LINEPODS_VIEW_SETTINGS_ITEMS);

    logic.view = LINEPODS_VIEW_SETTINGS;
    logic.settings_index = LINEPODS_SETTINGS_SECTION_DATA;
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_SHOW_SETTINGS);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_CLICK)
           == LINEPODS_ACTION_SHOW_SETTINGS_INFO);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_LONG)
           == LINEPODS_ACTION_SHOW_SETTINGS);
    assert(logic.view == LINEPODS_VIEW_SETTINGS_ITEMS);
}

static void test_book_scope_wraps_and_random_avoids_current(void)
{
    linepods_logic_t logic;
    linepods_logic_init(&logic);
    linepods_logic_set_quotes(&logic, 6);
    linepods_logic_set_book_scope(&logic, 2, 3, 1);
    assert(logic.quote_index == 2);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_UP_CLICK)
           == LINEPODS_ACTION_SHOW_CARD);
    assert(logic.quote_index == 4);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_CLICK)
           == LINEPODS_ACTION_SHOW_CARD);
    assert(logic.quote_index == 2);
    assert(!linepods_logic_set_quote_index(&logic, 1));
    assert(linepods_logic_set_quote_index(&logic, 3));

    size_t random_index = linepods_logic_select_random(&logic, 2);
    assert(logic.browse_mode == LINEPODS_BROWSE_ALL);
    assert(random_index < 6);
    assert(random_index != 3);
}

static void test_card_long_press_actions(void)
{
    linepods_logic_t logic;
    linepods_logic_init(&logic);
    linepods_logic_set_quotes(&logic, 3);

    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_UP_LONG)
           == LINEPODS_ACTION_SYNC);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_LONG)
           == LINEPODS_ACTION_SHOW_SETTINGS);
    assert(logic.view == LINEPODS_VIEW_SETTINGS);
}

static void test_utf8_copy_keeps_codepoints_intact(void)
{
    char output[7];
    assert(linepods_utf8_copy(output, sizeof(output), "A\xE4\xB8\xAD\xE6\x96\x87") == 4);
    assert(strcmp(output, "A\xE4\xB8\xAD") == 0);

    char exact[7];
    assert(linepods_utf8_copy(exact, sizeof(exact), "\xE4\xB8\xAD\xE6\x96\x87") == 6);
    assert(strcmp(exact, "\xE4\xB8\xAD\xE6\x96\x87") == 0);
}

int main(void)
{
    test_first_run_starts_setup_only_after_confirmation();
    test_sync_success_waits_for_confirmation();
    test_card_navigation_wraps();
    test_detail_pages_then_moves_between_quotes();
    test_error_actions();
    test_browse_menu_and_book_picker();
    test_settings_navigation_and_confirmations();
    test_book_scope_wraps_and_random_avoids_current();
    test_card_long_press_actions();
    test_utf8_copy_keeps_codepoints_intact();
    return 0;
}
