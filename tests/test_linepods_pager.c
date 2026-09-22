#include "linepods_pager.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint16_t fixed_width(uint32_t codepoint, uint32_t next_codepoint,
                            void *context)
{
    (void)next_codepoint;
    (void)context;
    return codepoint < 0x80 ? 1 : 2;
}

static void test_wraps_without_splitting_utf8(void)
{
    const char *text = "AB\xE4\xB8\xAD\xE6\x96\x87" "CD";
    linepods_pager_t pager;
    assert(linepods_pager_build(&pager, text, 4, 1, fixed_width, NULL));
    assert(pager.page_count == 2);

    size_t offset = 0;
    size_t length = 0;
    assert(linepods_pager_slice(&pager, 0, &offset, &length));
    assert(offset == 0 && length == 5);
    assert(strncmp(text + offset, "AB\xE4\xB8\xAD", length) == 0);
    assert(linepods_pager_slice(&pager, 1, &offset, &length));
    assert(length == 5);
    assert(strncmp(text + offset, "\xE6\x96\x87" "CD", length) == 0);
}

static void test_newline_starts_next_page_cleanly(void)
{
    linepods_pager_t pager;
    const char *text = "one\ntwo\nthree";
    assert(linepods_pager_build(&pager, text, 20, 2, fixed_width, NULL));
    assert(pager.page_count == 2);
    size_t offset = 0;
    size_t length = 0;
    assert(linepods_pager_slice(&pager, 1, &offset, &length));
    assert(strcmp(text + offset, "three") == 0);
    assert(length == strlen("three"));
}

static void test_empty_text_has_one_page(void)
{
    linepods_pager_t pager;
    assert(linepods_pager_build(&pager, "", 10, 2, fixed_width, NULL));
    assert(pager.page_count == 1);
    size_t offset = 99;
    size_t length = 99;
    assert(linepods_pager_slice(&pager, 0, &offset, &length));
    assert(offset == 0 && length == 0);
}

int main(void)
{
    test_wraps_without_splitting_utf8();
    test_newline_starts_next_page_cleanly();
    test_empty_text_has_one_page();
    return 0;
}
