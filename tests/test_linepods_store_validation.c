/* Exercises the committed-library validator in linepods_store.c.
 *
 * The validator walks the record area and the offset index with two strictly
 * forward cursors.  Nothing in the public store API can resurrect that path
 * after a successful mount, so this test forges a committed library on disk
 * and lets linepods_store_init() accept or reject it.  Every variant runs in a
 * forked child because the mount flag inside linepods_store.c is process-wide.
 */

#include "linepods_store.h"

#include "esp_spiffs.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
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

/* Mirrors of the private on-disk layout.  The validator only makes sense
 * against this exact byte format, so the test pins it down explicitly. */
#define DISK_MAGIC UINT32_C(0x574D4C42)
#define DISK_COMMITTED UINT32_C(0x434F4D54)
#define DISK_VERSION 2
#define DISK_RECORD_BOOK UINT32_C(0x424F4F4B)
#define DISK_RECORD_QUOTE UINT32_C(0x51554F54)

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t generation;
    uint32_t item_count;
    uint32_t book_count;
    uint32_t source_book_count;
    uint32_t source_quote_count;
    uint32_t index_offset;
    uint32_t file_size;
    uint32_t committed;
} disk_header_t;

typedef struct __attribute__((packed)) {
    uint32_t kind;
    uint32_t record_size;
    char book_id[LINEPODS_BOOK_ID_MAX + 1];
    char title[LINEPODS_TITLE_MAX + 1];
    char author[LINEPODS_AUTHOR_MAX + 1];
    int64_t sync_sort;
    uint32_t note_count;
} disk_book_t;

typedef struct __attribute__((packed)) {
    uint32_t kind;
    uint32_t record_size;
    uint32_t book_offset;
    uint32_t text_length;
    int64_t created_at;
    char quote_id[LINEPODS_QUOTE_ID_MAX + 1];
    char chapter[LINEPODS_CHAPTER_MAX + 1];
} disk_quote_t;

enum {
    VARIANT_VALID = 0,
    VARIANT_BAD_INDEX,
    VARIANT_BAD_BOOK_OFFSET,
    VARIANT_BAD_RECORD_SIZE,
    VARIANT_BAD_BOOK_COUNT,
    VARIANT_TRAILING_BYTES,
    VARIANT_COUNT,
};

static const char *variant_name(unsigned variant)
{
    switch (variant) {
    case VARIANT_VALID: return "valid";
    case VARIANT_BAD_INDEX: return "mismatched index entry";
    case VARIANT_BAD_BOOK_OFFSET: return "quote without its owner";
    case VARIANT_BAD_RECORD_SIZE: return "inconsistent record size";
    case VARIANT_BAD_BOOK_COUNT: return "book count mismatch";
    case VARIANT_TRAILING_BYTES: return "bytes after the index";
    default: return "unknown";
    }
}

static size_t build_library(unsigned variant, unsigned char *buffer,
                            size_t capacity)
{
    static const char text[] = "完整划线一";
    size_t text_length = strlen(text);
    size_t book_offset = sizeof(disk_header_t);
    size_t quote_offset = book_offset + sizeof(disk_book_t);
    size_t index_offset = quote_offset + sizeof(disk_quote_t) + text_length;
    size_t file_size = index_offset + sizeof(uint32_t);
    assert(sizeof(disk_header_t) + sizeof(disk_book_t) + sizeof(disk_quote_t)
               + text_length + sizeof(uint32_t)
           < capacity);
    memset(buffer, 0, capacity);
    if (variant == VARIANT_TRAILING_BYTES) file_size += sizeof(uint32_t);

    disk_book_t book;
    memset(&book, 0, sizeof(book));
    book.kind = DISK_RECORD_BOOK;
    book.record_size = sizeof(book);
    snprintf(book.book_id, sizeof(book.book_id), "%s", "book-1");
    snprintf(book.title, sizeof(book.title), "%s", "第一本书");
    snprintf(book.author, sizeof(book.author), "%s", "作者甲");
    book.sync_sort = 1001;
    book.note_count = 1;
    memcpy(buffer + book_offset, &book, sizeof(book));

    disk_quote_t quote;
    memset(&quote, 0, sizeof(quote));
    quote.kind = DISK_RECORD_QUOTE;
    quote.record_size = sizeof(quote) + text_length;
    quote.book_offset = (uint32_t)book_offset;
    quote.text_length = (uint32_t)text_length;
    quote.created_at = 200;
    snprintf(quote.quote_id, sizeof(quote.quote_id), "%s", "quote-1");
    snprintf(quote.chapter, sizeof(quote.chapter), "%s", "第一章");
    if (variant == VARIANT_BAD_BOOK_OFFSET) quote.book_offset = 0;
    if (variant == VARIANT_BAD_RECORD_SIZE) quote.record_size++;
    memcpy(buffer + quote_offset, &quote, sizeof(quote));
    memcpy(buffer + quote_offset + sizeof(quote), text, text_length);

    uint32_t entry = (uint32_t)quote_offset;
    if (variant == VARIANT_BAD_INDEX) entry += 4;
    memcpy(buffer + index_offset, &entry, sizeof(entry));

    disk_header_t header;
    memset(&header, 0, sizeof(header));
    header.magic = DISK_MAGIC;
    header.version = DISK_VERSION;
    header.header_size = sizeof(disk_header_t);
    header.generation = 7;
    header.item_count = 1;
    header.book_count = 1;
    header.source_book_count = 1;
    header.source_quote_count = 1;
    header.index_offset = (uint32_t)index_offset;
    header.file_size = (uint32_t)file_size;
    header.committed = DISK_COMMITTED;
    if (variant == VARIANT_BAD_BOOK_COUNT) header.book_count = 2;
    memcpy(buffer, &header, sizeof(header));
    return file_size;
}

static void slot_file_path(int slot, char *path, size_t path_size)
{
    snprintf(path, path_size, "%s/library%d.bin", LINEPODS_STORE_BASE_PATH, slot);
}

static void remove_store_files(void)
{
    char path[512];
    for (int slot = 0; slot < 2; slot++) {
        slot_file_path(slot, path, sizeof(path));
        remove(path);
    }
    snprintf(path, sizeof(path), "%s/index.tmp", LINEPODS_STORE_BASE_PATH);
    remove(path);
}

static void write_library(unsigned variant)
{
    unsigned char buffer[8192];
    size_t size = build_library(variant, buffer, sizeof(buffer));
    char path[512];
    slot_file_path(0, path, sizeof(path));
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(buffer, 1, size, file) == size);
    assert(fclose(file) == 0);
}

static void run_variant_in_child(unsigned variant)
{
    remove_store_files();
    write_library(variant);
    if (linepods_store_init() != ESP_OK) {
        fprintf(stderr, "%s: mount failed\n", variant_name(variant));
        _exit(1);
    }
    linepods_content_t content;
    esp_err_t error = linepods_store_load_content(&content);
    if (variant == VARIANT_VALID) {
        if (error != ESP_OK || content.item_count != 1
            || content.loaded_book_count != 1
            || content.source_quote_count != 1
            || strcmp(content.item.book.book_id, "book-1") != 0
            || strcmp(content.item.quote.chapter, "第一章") != 0
            || content.item.quote.preview[0] == '\0') {
            fprintf(stderr, "%s: intact library was rejected (%s)\n",
                    variant_name(variant), esp_err_to_name(error));
            _exit(1);
        }
        _exit(0);
    }
    if (error != ESP_ERR_NOT_FOUND) {
        fprintf(stderr, "%s: corrupt library was accepted (%s)\n",
                variant_name(variant), esp_err_to_name(error));
        _exit(1);
    }
    _exit(0);
}

int main(void)
{
    assert(mkdir(LINEPODS_STORE_BASE_PATH, 0700) == 0 || errno == EEXIST);
    for (unsigned variant = 0; variant < VARIANT_COUNT; variant++) {
        pid_t child = fork();
        assert(child >= 0);
        if (child == 0) run_variant_in_child(variant);
        int status = 0;
        assert(waitpid(child, &status, 0) == child);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "linepods_store validation test failed\n");
            return 1;
        }
    }
    remove_store_files();
    assert(rmdir(LINEPODS_STORE_BASE_PATH) == 0);
    puts("linepods_store committed-library validation tests passed");
    return 0;
}
