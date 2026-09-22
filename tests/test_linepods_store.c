#include "linepods_store.h"

#include "esp_spiffs.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef LINEPODS_STORE_BASE_PATH
#error "LINEPODS_STORE_BASE_PATH must be supplied by the host test build"
#endif

esp_err_t esp_vfs_spiffs_register(const esp_vfs_spiffs_conf_t *config)
{
    assert(config);
    assert(strcmp(config->base_path, LINEPODS_STORE_BASE_PATH) == 0);
    assert(config->max_files >= 3);
    return ESP_OK;
}

void esp_vfs_spiffs_unregister(const char *partition_label)
{
    (void)partition_label;
}

esp_err_t esp_spiffs_info(const char *partition_label, size_t *total,
                          size_t *used)
{
    (void)partition_label;
    *total = 1024 * 1024;
    *used = 0;
    return ESP_OK;
}

int linepods_test_spiffs_rename(const char *source, const char *destination)
{
    return rename(source, destination);
}

static linepods_book_t book(const char *id, const char *title, const char *author)
{
    linepods_book_t value = {0};
    snprintf(value.book_id, sizeof(value.book_id), "%s", id);
    snprintf(value.title, sizeof(value.title), "%s", title);
    snprintf(value.author, sizeof(value.author), "%s", author);
    return value;
}

static linepods_quote_t quote(const char *id, const char *chapter, int64_t created)
{
    linepods_quote_t value = {.created_at = created};
    snprintf(value.quote_id, sizeof(value.quote_id), "%s", id);
    snprintf(value.chapter, sizeof(value.chapter), "%s", chapter);
    return value;
}

static void assert_text(size_t index, const char *expected)
{
    char *text = NULL;
    size_t length = 0;
    assert(linepods_store_read_text(index, &text, &length) == ESP_OK);
    assert(length == strlen(expected));
    assert(strcmp(text, expected) == 0);
    free(text);
}

static void remove_store_files(void)
{
    char path[512];
    for (int slot = 0; slot < 2; slot++) {
        snprintf(path, sizeof(path), "%s/library%d.bin",
                 LINEPODS_STORE_BASE_PATH, slot);
        remove(path);
    }
    snprintf(path, sizeof(path), "%s/index.tmp", LINEPODS_STORE_BASE_PATH);
    remove(path);
}

static void create_file(const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", LINEPODS_STORE_BASE_PATH, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fputs("legacy", file) >= 0);
    assert(fclose(file) == 0);
}

static bool store_file_exists(const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", LINEPODS_STORE_BASE_PATH, name);
    return access(path, F_OK) == 0;
}

int main(void)
{
    assert(mkdir(LINEPODS_STORE_BASE_PATH, 0700) == 0 || errno == EEXIST);
    remove_store_files();
    assert(linepods_store_init() == ESP_OK);

    linepods_content_t content;
    assert(linepods_store_load_content(&content) == ESP_ERR_NOT_FOUND);

    linepods_book_t first_book = book("book-1", "第一本书", "作者甲");
    linepods_book_t second_book = book("book-2", "第二本书", "作者乙");
    first_book.sync_sort = 1001;
    first_book.note_count = 2;
    second_book.sync_sort = 1002;
    second_book.note_count = 1;
    linepods_quote_t first_quote = quote("quote-1", "第一章", 100);
    linepods_quote_t second_quote = quote("quote-2", "第二章", 200);
    linepods_quote_t third_quote = quote("quote-3", "第三章", 300);

    create_file("q00.txt");
    create_file("q99.tmp");
    create_file("keep.txt");

    assert(linepods_store_sync_begin() == ESP_OK);
    assert(linepods_store_sync_add_book(&first_book) == ESP_OK);
    assert(linepods_store_sync_add_quote(&first_quote, "完整划线一",
                                       strlen("完整划线一")) == ESP_OK);
    assert(linepods_store_sync_add_quote(&second_quote, "complete quote two",
                                       strlen("complete quote two")) == ESP_OK);
    assert(linepods_store_sync_add_book(&second_book) == ESP_OK);
    assert(linepods_store_sync_add_quote(&third_quote, "完整划线三",
                                       strlen("完整划线三")) == ESP_OK);
    assert(linepods_store_sync_commit(2, 3) == ESP_OK);
    assert(!store_file_exists("q00.txt"));
    assert(!store_file_exists("q99.tmp"));
    assert(store_file_exists("keep.txt"));

    assert(linepods_store_load_content(&content) == ESP_OK);
    assert(content.item_count == 3);
    assert(content.loaded_book_count == 2);
    assert(content.source_book_count == 2);
    assert(content.source_quote_count == 3);
    assert(content.item_index == 0);
    assert(strcmp(content.item.book.title, "第一本书") == 0);
    assert(content.item.book.sync_sort == 1001);
    assert(content.item.book.note_count == 2);
    assert(strcmp(content.item.quote.preview, "完整划线一") == 0);
    assert_text(1, "complete quote two");

    linepods_item_t item;
    assert(linepods_store_read_item(2, &item) == ESP_OK);
    assert(strcmp(item.book.title, "第二本书") == 0);
    assert(strcmp(item.quote.chapter, "第三章") == 0);
    assert(item.quote.created_at == 300);

    assert(linepods_store_sync_begin() == ESP_OK);
    size_t copied = 0;
    assert(linepods_store_sync_copy_book(&first_book, &copied) == ESP_OK);
    assert(copied == 2);
    assert(linepods_store_sync_copy_book(&second_book, &copied) == ESP_OK);
    assert(copied == 1);
    assert(linepods_store_sync_commit(2, 3) == ESP_OK);
    assert(linepods_store_load_content(&content) == ESP_OK);
    assert(content.item_count == 3);
    assert_text(0, "完整划线一");
    assert_text(2, "完整划线三");

    linepods_book_t changed_book = first_book;
    changed_book.sync_sort++;
    assert(linepods_store_sync_begin() == ESP_OK);
    assert(linepods_store_sync_copy_book(&changed_book, &copied)
           == ESP_ERR_NOT_FOUND);
    assert(copied == 0);
    linepods_store_sync_abort();

    assert(linepods_store_sync_begin() == ESP_OK);
    assert(linepods_store_sync_add_book(&first_book) == ESP_OK);
    assert(linepods_store_sync_add_quote(&first_quote, "incomplete",
                                       strlen("incomplete")) == ESP_OK);
    linepods_store_sync_abort();
    assert(linepods_store_load_content(&content) == ESP_OK);
    assert(content.item_count == 3);
    assert_text(2, "完整划线三");

    assert(linepods_store_sync_begin() == ESP_OK);
    assert(linepods_store_sync_add_book(&second_book) == ESP_OK);
    assert(linepods_store_sync_add_quote(&third_quote, "replacement generation",
                                       strlen("replacement generation")) == ESP_OK);
    assert(linepods_store_sync_commit(1, 1) == ESP_OK);
    assert(linepods_store_load_content(&content) == ESP_OK);
    assert(content.item_count == 1);
    assert(content.loaded_book_count == 1);
    assert(strcmp(content.item.book.book_id, "book-2") == 0);
    assert_text(0, "replacement generation");

    remove_store_files();
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/keep.txt", LINEPODS_STORE_BASE_PATH);
        assert(remove(path) == 0);
    }
    assert(rmdir(LINEPODS_STORE_BASE_PATH) == 0);
    puts("linepods_store indexed-library tests passed");
    return 0;
}
