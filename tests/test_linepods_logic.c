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

static void test_font_compare_navigation(void)
{
    linepods_logic_t logic;
    linepods_logic_init(&logic);
    linepods_logic_set_quotes(&logic, 3);

    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_LONG)
           == LINEPODS_ACTION_SHOW_FONT_COMPARE);
    assert(logic.view == LINEPODS_VIEW_FONT_COMPARE);
    assert(logic.font_compare_index == 0);

    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_UP_CLICK)
           == LINEPODS_ACTION_SHOW_FONT_COMPARE);
    assert(logic.font_compare_index == 3);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_DOWN_CLICK)
           == LINEPODS_ACTION_SHOW_FONT_COMPARE);
    assert(logic.font_compare_index == 0);

    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_LONG)
           == LINEPODS_ACTION_SHOW_CARD);
    assert(logic.view == LINEPODS_VIEW_CARD);
}

static void test_card_long_press_actions(void)
{
    linepods_logic_t logic;
    linepods_logic_init(&logic);
    linepods_logic_set_quotes(&logic, 3);

    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_UP_LONG)
           == LINEPODS_ACTION_SYNC);
    assert(linepods_logic_handle(&logic, LINEPODS_INPUT_OK_LONG)
           == LINEPODS_ACTION_START_SETUP);
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
    test_font_compare_navigation();
    test_card_long_press_actions();
    test_utf8_copy_keeps_codepoints_intact();
    return 0;
}
