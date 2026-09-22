#include "linepods_store.h"

#include "esp_log.h"
#include "esp_spiffs.h"
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef LINEPODS_STORE_BASE_PATH
#define LINEPODS_STORE_BASE_PATH "/linepods"
#endif

#define LINEPODS_STORE_PARTITION "linepods_cache"
#define LINEPODS_STORE_MAX_TEXT (64 * 1024)
#define LINEPODS_LIBRARY_MAGIC UINT32_C(0x574D4C42)
#define LINEPODS_LIBRARY_COMMITTED UINT32_C(0x434F4D54)
#define LINEPODS_LIBRARY_VERSION 2
#define LINEPODS_LIBRARY_VERSION_LEGACY 1
#define LINEPODS_RECORD_BOOK UINT32_C(0x424F4F4B)
#define LINEPODS_RECORD_QUOTE UINT32_C(0x51554F54)
#define LINEPODS_SLOT_COUNT 2
#define LINEPODS_PATH_SIZE (sizeof(LINEPODS_STORE_BASE_PATH) + 24)

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
} library_header_t;

typedef struct __attribute__((packed)) {
    uint32_t kind;
    uint32_t record_size;
    char book_id[LINEPODS_BOOK_ID_MAX + 1];
    char title[LINEPODS_TITLE_MAX + 1];
    char author[LINEPODS_AUTHOR_MAX + 1];
} library_book_v1_t;

typedef struct __attribute__((packed)) {
    uint32_t kind;
    uint32_t record_size;
    char book_id[LINEPODS_BOOK_ID_MAX + 1];
    char title[LINEPODS_TITLE_MAX + 1];
    char author[LINEPODS_AUTHOR_MAX + 1];
    int64_t sync_sort;
    uint32_t note_count;
} library_book_t;

typedef struct __attribute__((packed)) {
    uint32_t kind;
    uint32_t record_size;
} library_record_t;

typedef struct __attribute__((packed)) {
    uint32_t kind;
    uint32_t record_size;
    uint32_t book_offset;
    uint32_t text_length;
    int64_t created_at;
    char quote_id[LINEPODS_QUOTE_ID_MAX + 1];
    char chapter[LINEPODS_CHAPTER_MAX + 1];
} library_quote_t;

typedef struct {
    FILE *library;
    FILE *index;
    int target_slot;
    uint32_t generation;
    uint32_t current_book_offset;
    uint32_t item_count;
    uint32_t book_count;
    bool active;
} sync_state_t;

static const char *TAG = "linepods_store";
static bool s_mounted;
static int s_active_slot = -1;
static library_header_t s_active_header;
static sync_state_t s_sync = {.target_slot = -1};

static void storage_yield(void)
{
#ifdef ESP_PLATFORM
    /* SPIFFS metadata walks can keep a single-core C3 busy long enough to
     * starve the idle task.  One tick is enough to feed the task watchdog
     * without making ordinary card reads noticeably slower. */
    vTaskDelay(1);
#endif
}

static void slot_path(int slot, char *path, size_t path_size)
{
    snprintf(path, path_size, LINEPODS_STORE_BASE_PATH "/library%d.bin", slot);
}

static void index_path(char *path, size_t path_size)
{
    snprintf(path, path_size, LINEPODS_STORE_BASE_PATH "/index.tmp");
}

static esp_err_t write_exact(FILE *file, const void *data, size_t size)
{
    if (!file || (!data && size > 0)) return ESP_ERR_INVALID_ARG;
    if (size == 0) return ESP_OK;
    if (fwrite(data, 1, size, file) == size) return ESP_OK;
    return errno == ENOSPC ? ESP_ERR_NO_MEM : ESP_FAIL;
}

static bool read_exact(FILE *file, void *data, size_t size)
{
    return file && data && fread(data, 1, size, file) == size;
}

static size_t utf8_sequence_length(unsigned char lead)
{
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

static void copy_preview(char *destination, size_t destination_size,
                         const char *source)
{
    if (!destination || destination_size == 0) return;
    destination[0] = '\0';
    if (!source) return;
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
}

static bool seek_to(FILE *file, uint32_t offset)
{
    return file && fseek(file, (long)offset, SEEK_SET) == 0;
}

static bool file_offset(FILE *file, uint32_t *offset)
{
    long position = file ? ftell(file) : -1;
    if (position < 0 || (unsigned long)position > UINT32_MAX) return false;
    *offset = (uint32_t)position;
    return true;
}

static bool supported_version(uint16_t version)
{
    return version == LINEPODS_LIBRARY_VERSION
        || version == LINEPODS_LIBRARY_VERSION_LEGACY;
}

static bool read_book_at(FILE *file, const library_header_t *header,
                         uint32_t offset, library_book_t *book)
{
    if (!file || !header || !book || offset < sizeof(*header)
        || offset >= header->index_offset || !seek_to(file, offset)) {
        return false;
    }
    *book = (library_book_t) {0};
    if (header->version == LINEPODS_LIBRARY_VERSION) {
        return read_exact(file, book, sizeof(*book))
            && book->kind == LINEPODS_RECORD_BOOK
            && book->record_size == sizeof(*book);
    }
    if (header->version == LINEPODS_LIBRARY_VERSION_LEGACY) {
        library_book_v1_t legacy;
        if (!read_exact(file, &legacy, sizeof(legacy))
            || legacy.kind != LINEPODS_RECORD_BOOK
            || legacy.record_size != sizeof(legacy)) {
            return false;
        }
        book->kind = legacy.kind;
        book->record_size = legacy.record_size;
        memcpy(book->book_id, legacy.book_id, sizeof(book->book_id));
        memcpy(book->title, legacy.title, sizeof(book->title));
        memcpy(book->author, legacy.author, sizeof(book->author));
        return true;
    }
    return false;
}

static bool skip_forward(FILE *file, size_t count, unsigned char *buffer,
                         size_t buffer_size)
{
    while (count > 0) {
        size_t chunk = count < buffer_size ? count : buffer_size;
        if (!read_exact(file, buffer, chunk)) return false;
        count -= chunk;
    }
    return true;
}

/* Walks the record area strictly forward and matches every quote record
 * against the committed offset index.
 *
 * The writer appends each book, that book's quotes, and finally the index in a
 * single forward pass, so validation can follow the same order.  Revisiting
 * the records through the index instead measured at multiple seconds for a
 * 64-highlight library on SPIFFS, where every backwards jump costs a fresh
 * page lookup; two forward cursors read the same bytes once. */
static bool validate_records(FILE *file, FILE *index,
                             const library_header_t *header)
{
    if (!seek_to(file, sizeof(*header))
        || !seek_to(index, header->index_offset)) {
        return false;
    }

    unsigned char scratch[256];
    uint32_t position = sizeof(*header);
    uint32_t owner = UINT32_MAX;
    uint32_t books = 0;
    uint32_t quotes = 0;
    unsigned record_count = 0;

    while (position < header->index_offset) {
        if (header->index_offset - position < sizeof(library_record_t)) {
            return false;
        }
        library_record_t record;
        if (!read_exact(file, &record, sizeof(record))) return false;
        if (record.record_size < sizeof(record)
            || (uint64_t)position + record.record_size > header->index_offset) {
            return false;
        }
        size_t payload = record.record_size - sizeof(record);

        if (record.kind == LINEPODS_RECORD_BOOK) {
            size_t expected = (header->version == LINEPODS_LIBRARY_VERSION_LEGACY
                                   ? sizeof(library_book_v1_t)
                                   : sizeof(library_book_t))
                            - sizeof(record);
            if (payload != expected
                || !skip_forward(file, payload, scratch, sizeof(scratch))) {
                return false;
            }
            owner = position;
            books++;
        } else if (record.kind == LINEPODS_RECORD_QUOTE) {
            if (owner == UINT32_MAX || record.record_size < sizeof(library_quote_t)) {
                return false;
            }
            library_quote_t quote;
            memcpy(&quote, &record, sizeof(record));
            if (!read_exact(file, (unsigned char *)&quote + sizeof(record),
                            sizeof(quote) - sizeof(record))
                || quote.book_offset != owner
                || quote.record_size != sizeof(quote) + quote.text_length
                || quote.text_length > LINEPODS_STORE_MAX_TEXT) {
                return false;
            }
            uint32_t indexed = 0;
            if (!read_exact(index, &indexed, sizeof(indexed))
                || indexed != position) {
                return false;
            }
            if (!skip_forward(file, quote.text_length, scratch,
                              sizeof(scratch))) {
                return false;
            }
            quotes++;
        } else {
            return false;
        }

        position += record.record_size;
        if ((++record_count & 7U) == 0U) storage_yield();
    }

    if (position != header->index_offset
        || books != header->book_count
        || quotes != header->item_count) {
        return false;
    }
    /* Nothing may follow the committed index. */
    uint32_t extra = 0;
    return !read_exact(index, &extra, sizeof(extra));
}

static bool validate_slot(int slot, library_header_t *header_out)
{
    char path[LINEPODS_PATH_SIZE];
    slot_path(slot, path, sizeof(path));
    FILE *file = fopen(path, "rb");
    FILE *index = fopen(path, "rb");
    if (!file || !index) {
        if (file) fclose(file);
        if (index) fclose(index);
        return false;
    }

    library_header_t header;
    bool valid = read_exact(file, &header, sizeof(header))
              && header.magic == LINEPODS_LIBRARY_MAGIC
              && supported_version(header.version)
              && header.header_size == sizeof(header)
              && header.committed == LINEPODS_LIBRARY_COMMITTED
              && header.item_count > 0
              && header.book_count > 0
              && header.index_offset >= sizeof(header);
    if (valid) {
        struct stat info;
        uint64_t expected = (uint64_t)header.index_offset
                          + (uint64_t)header.item_count * sizeof(uint32_t);
        valid = stat(path, &info) == 0 && info.st_size >= 0
             && (uint64_t)info.st_size == header.file_size
             && expected == header.file_size;
    }
    if (valid) valid = validate_records(file, index, &header);

    fclose(file);
    fclose(index);
    if (valid && header_out) *header_out = header;
    return valid;
}

static bool generation_newer(uint32_t candidate, uint32_t current)
{
    return (int32_t)(candidate - current) > 0;
}

static void refresh_active_slot(void)
{
    library_header_t headers[LINEPODS_SLOT_COUNT];
    bool valid[LINEPODS_SLOT_COUNT];
    for (int i = 0; i < LINEPODS_SLOT_COUNT; i++) {
        valid[i] = validate_slot(i, &headers[i]);
        storage_yield();
    }
    int selected = -1;
    if (valid[0]) selected = 0;
    if (valid[1]
        && (selected < 0
            || generation_newer(headers[1].generation,
                                headers[selected].generation))) {
        selected = 1;
    }
    s_active_slot = selected;
    s_active_header = selected >= 0 ? headers[selected] : (library_header_t) {0};
}

static void cleanup_legacy_cache(void)
{
    DIR *directory = opendir(LINEPODS_STORE_BASE_PATH);
    if (!directory) return;

    struct dirent *entry = NULL;
    while ((entry = readdir(directory)) != NULL) {
        const char *name = strrchr(entry->d_name, '/');
        name = name ? name + 1 : entry->d_name;
        size_t length = strlen(name);
        bool legacy = length == 7
                   && name[0] == 'q'
                   && name[1] >= '0' && name[1] <= '9'
                   && name[2] >= '0' && name[2] <= '9'
                   && (strcmp(name + 3, ".txt") == 0
                       || strcmp(name + 3, ".tmp") == 0);
        if (!legacy) continue;

        char path[LINEPODS_PATH_SIZE];
        snprintf(path, sizeof(path), LINEPODS_STORE_BASE_PATH "/%.7s", name);
        storage_yield();
        remove(path);
        storage_yield();
    }
    closedir(directory);
}

esp_err_t linepods_store_init(void)
{
    if (s_mounted) return ESP_OK;
    esp_vfs_spiffs_conf_t config = {
        .base_path = LINEPODS_STORE_BASE_PATH,
        .partition_label = LINEPODS_STORE_PARTITION,
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    esp_err_t error = esp_vfs_spiffs_register(&config);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount %s: %s", LINEPODS_STORE_PARTITION,
                 esp_err_to_name(error));
        return error;
    }
    size_t total = 0;
    size_t used = 0;
    error = esp_spiffs_info(LINEPODS_STORE_PARTITION, &total, &used);
    if (error != ESP_OK) {
        esp_vfs_spiffs_unregister(LINEPODS_STORE_PARTITION);
        return error;
    }
    s_mounted = true;
    refresh_active_slot();
    char temporary[LINEPODS_PATH_SIZE];
    index_path(temporary, sizeof(temporary));
    remove(temporary);
    ESP_LOGI(TAG, "Mounted %s: total=%u used=%u active_slot=%d items=%u",
             LINEPODS_STORE_PARTITION, (unsigned)total, (unsigned)used,
             s_active_slot, (unsigned)s_active_header.item_count);
    return ESP_OK;
}

void linepods_store_sync_abort(void)
{
    if (s_sync.library) fclose(s_sync.library);
    if (s_sync.index) fclose(s_sync.index);
    if (s_sync.target_slot >= 0) {
        char path[LINEPODS_PATH_SIZE];
        slot_path(s_sync.target_slot, path, sizeof(path));
        remove(path);
    }
    char temporary[LINEPODS_PATH_SIZE];
    index_path(temporary, sizeof(temporary));
    remove(temporary);
    s_sync = (sync_state_t) {.target_slot = -1};
}

esp_err_t linepods_store_sync_begin(void)
{
    if (!s_mounted) return ESP_ERR_INVALID_STATE;
    if (s_sync.active) return ESP_ERR_INVALID_STATE;
    if (s_active_slot < 0) refresh_active_slot();

    int target = s_active_slot == 0 ? 1 : 0;
    char library_path[LINEPODS_PATH_SIZE];
    char temporary[LINEPODS_PATH_SIZE];
    slot_path(target, library_path, sizeof(library_path));
    index_path(temporary, sizeof(temporary));
    remove(library_path);
    remove(temporary);

    s_sync = (sync_state_t) {
        .target_slot = target,
        .generation = s_active_slot >= 0 ? s_active_header.generation + 1 : 1,
        .current_book_offset = UINT32_MAX,
        .active = true,
    };
    s_sync.library = fopen(library_path, "wb+");
    s_sync.index = fopen(temporary, "wb+");
    if (!s_sync.library || !s_sync.index) {
        esp_err_t open_error = errno == ENOSPC ? ESP_ERR_NO_MEM : ESP_FAIL;
        linepods_store_sync_abort();
        return open_error;
    }

    library_header_t placeholder = {
        .magic = LINEPODS_LIBRARY_MAGIC,
        .version = LINEPODS_LIBRARY_VERSION,
        .header_size = sizeof(library_header_t),
        .generation = s_sync.generation,
    };
    esp_err_t error = write_exact(s_sync.library, &placeholder, sizeof(placeholder));
    if (error != ESP_OK) linepods_store_sync_abort();
    return error;
}

esp_err_t linepods_store_sync_add_book(const linepods_book_t *book)
{
    if (!s_sync.active || !book || !book->book_id[0]) return ESP_ERR_INVALID_ARG;
    uint32_t offset = 0;
    if (!file_offset(s_sync.library, &offset)) return ESP_FAIL;
    library_book_t stored = {
        .kind = LINEPODS_RECORD_BOOK,
        .record_size = sizeof(library_book_t),
    };
    memcpy(stored.book_id, book->book_id, sizeof(stored.book_id));
    memcpy(stored.title, book->title, sizeof(stored.title));
    memcpy(stored.author, book->author, sizeof(stored.author));
    stored.sync_sort = book->sync_sort;
    stored.note_count = book->note_count;
    esp_err_t error = write_exact(s_sync.library, &stored, sizeof(stored));
    if (error == ESP_OK) {
        s_sync.current_book_offset = offset;
        s_sync.book_count++;
    }
    return error;
}

static esp_err_t copy_quote_record(FILE *source, const library_quote_t *source_quote)
{
    uint32_t target_offset = 0;
    if (!source || !source_quote || !file_offset(s_sync.library, &target_offset)) {
        return ESP_FAIL;
    }
    library_quote_t target_quote = *source_quote;
    target_quote.book_offset = s_sync.current_book_offset;
    esp_err_t error = write_exact(s_sync.library, &target_quote,
                                  sizeof(target_quote));
    unsigned char buffer[256];
    size_t remaining = source_quote->text_length;
    unsigned chunk_count = 0;
    while (error == ESP_OK && remaining > 0) {
        size_t count = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        if (!read_exact(source, buffer, count)) return ESP_FAIL;
        error = write_exact(s_sync.library, buffer, count);
        remaining -= count;
        if ((++chunk_count & 7U) == 0U) storage_yield();
    }
    if (error == ESP_OK) {
        error = write_exact(s_sync.index, &target_offset, sizeof(target_offset));
    }
    if (error == ESP_OK) s_sync.item_count++;
    return error;
}

esp_err_t linepods_store_sync_copy_book(const linepods_book_t *book,
                                      size_t *copied_quote_count)
{
    if (!s_sync.active || !book || !book->book_id[0] || !copied_quote_count) {
        return ESP_ERR_INVALID_ARG;
    }
    *copied_quote_count = 0;
    if (s_active_slot < 0
        || s_active_header.version != LINEPODS_LIBRARY_VERSION) {
        return ESP_ERR_NOT_FOUND;
    }

    char path[LINEPODS_PATH_SIZE];
    slot_path(s_active_slot, path, sizeof(path));
    FILE *source = fopen(path, "rb");
    if (!source) return ESP_FAIL;

    uint32_t position = sizeof(library_header_t);
    unsigned record_count = 0;
    bool matched = false;
    esp_err_t error = ESP_ERR_NOT_FOUND;
    while (position < s_active_header.index_offset) {
        library_record_t record;
        if (!seek_to(source, position) || !read_exact(source, &record, sizeof(record))
            || record.record_size < sizeof(record)
            || (uint64_t)position + record.record_size
               > s_active_header.index_offset) {
            error = ESP_FAIL;
            break;
        }
        if (record.kind == LINEPODS_RECORD_BOOK) {
            library_book_t stored;
            if (!read_book_at(source, &s_active_header, position, &stored)) {
                error = ESP_FAIL;
                break;
            }
            if (matched) break;
            matched = strcmp(stored.book_id, book->book_id) == 0
                   && stored.sync_sort == book->sync_sort
                   && stored.note_count == book->note_count;
            if (matched) {
                error = linepods_store_sync_add_book(book);
                if (error != ESP_OK) break;
            }
        } else if (record.kind == LINEPODS_RECORD_QUOTE && matched) {
            library_quote_t quote;
            if (!seek_to(source, position)
                || !read_exact(source, &quote, sizeof(quote))
                || quote.record_size != sizeof(quote) + quote.text_length
                || quote.text_length > LINEPODS_STORE_MAX_TEXT) {
                error = ESP_FAIL;
                break;
            }
            error = copy_quote_record(source, &quote);
            if (error != ESP_OK) break;
            (*copied_quote_count)++;
        }
        position += record.record_size;
        if ((++record_count & 7U) == 0U) storage_yield();
    }
    fclose(source);
    if (error == ESP_OK && *copied_quote_count == 0) return ESP_ERR_NOT_FOUND;
    return error;
}

esp_err_t linepods_store_sync_add_quote(const linepods_quote_t *quote,
                                      const char *text, size_t length)
{
    if (!s_sync.active || s_sync.current_book_offset == UINT32_MAX
        || !quote || !text) {
        return ESP_ERR_INVALID_ARG;
    }
    if (length > LINEPODS_STORE_MAX_TEXT
        || length > UINT32_MAX - sizeof(library_quote_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint32_t offset = 0;
    if (!file_offset(s_sync.library, &offset)) return ESP_FAIL;
    library_quote_t stored = {
        .kind = LINEPODS_RECORD_QUOTE,
        .record_size = (uint32_t)(sizeof(library_quote_t) + length),
        .book_offset = s_sync.current_book_offset,
        .text_length = (uint32_t)length,
        .created_at = quote->created_at,
    };
    memcpy(stored.quote_id, quote->quote_id, sizeof(stored.quote_id));
    memcpy(stored.chapter, quote->chapter, sizeof(stored.chapter));

    esp_err_t error = write_exact(s_sync.library, &stored, sizeof(stored));
    if (error == ESP_OK) error = write_exact(s_sync.library, text, length);
    if (error == ESP_OK) error = write_exact(s_sync.index, &offset, sizeof(offset));
    if (error == ESP_OK) s_sync.item_count++;
    return error;
}

esp_err_t linepods_store_sync_commit(size_t source_book_count,
                                   size_t source_quote_count)
{
    if (!s_sync.active) return ESP_ERR_INVALID_STATE;
    if (s_sync.item_count == 0 || s_sync.book_count == 0) {
        linepods_store_sync_abort();
        return ESP_ERR_NOT_FOUND;
    }
    if (source_book_count > UINT32_MAX || source_quote_count > UINT32_MAX) {
        linepods_store_sync_abort();
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t error = ESP_OK;
    if (fflush(s_sync.library) != 0 || fflush(s_sync.index) != 0
        || fseek(s_sync.index, 0, SEEK_SET) != 0) {
        error = ESP_FAIL;
    }
    uint32_t index_offset_value = 0;
    if (error == ESP_OK && !file_offset(s_sync.library, &index_offset_value)) {
        error = ESP_FAIL;
    }

    unsigned char buffer[256];
    while (error == ESP_OK) {
        size_t count = fread(buffer, 1, sizeof(buffer), s_sync.index);
        if (count > 0) error = write_exact(s_sync.library, buffer, count);
        if (count < sizeof(buffer)) {
            if (ferror(s_sync.index)) error = ESP_FAIL;
            break;
        }
    }

    uint32_t file_size_value = 0;
    if (error == ESP_OK && !file_offset(s_sync.library, &file_size_value)) {
        error = ESP_FAIL;
    }
    library_header_t header = {
        .magic = LINEPODS_LIBRARY_MAGIC,
        .version = LINEPODS_LIBRARY_VERSION,
        .header_size = sizeof(library_header_t),
        .generation = s_sync.generation,
        .item_count = s_sync.item_count,
        .book_count = s_sync.book_count,
        .source_book_count = (uint32_t)source_book_count,
        .source_quote_count = (uint32_t)source_quote_count,
        .index_offset = index_offset_value,
        .file_size = file_size_value,
        .committed = LINEPODS_LIBRARY_COMMITTED,
    };
    if (error == ESP_OK) {
        if (fseek(s_sync.library, 0, SEEK_SET) != 0) {
            error = ESP_FAIL;
        } else {
            error = write_exact(s_sync.library, &header, sizeof(header));
        }
        if (error == ESP_OK && fflush(s_sync.library) != 0) error = ESP_FAIL;
    }

    int target = s_sync.target_slot;
    if (s_sync.library && fclose(s_sync.library) != 0 && error == ESP_OK) {
        error = ESP_FAIL;
    }
    if (s_sync.index && fclose(s_sync.index) != 0 && error == ESP_OK) {
        error = ESP_FAIL;
    }
    s_sync.library = NULL;
    s_sync.index = NULL;

    char temporary[LINEPODS_PATH_SIZE];
    index_path(temporary, sizeof(temporary));
    remove(temporary);
    if (error == ESP_OK && !validate_slot(target, &header)) error = ESP_FAIL;
    if (error != ESP_OK) {
        char path[LINEPODS_PATH_SIZE];
        slot_path(target, path, sizeof(path));
        remove(path);
        s_sync = (sync_state_t) {.target_slot = -1};
        return error;
    }

    s_active_slot = target;
    s_active_header = header;
    s_sync = (sync_state_t) {.target_slot = -1};
    cleanup_legacy_cache();
    ESP_LOGI(TAG, "Committed slot=%d generation=%u items=%u books=%u bytes=%u",
             target, (unsigned)header.generation, (unsigned)header.item_count,
             (unsigned)header.book_count, (unsigned)header.file_size);
    return ESP_OK;
}

static esp_err_t open_active(FILE **file, library_header_t *header)
{
    if (!s_mounted || !file || !header) return ESP_ERR_INVALID_ARG;
    if (s_active_slot < 0) refresh_active_slot();
    if (s_active_slot < 0) return ESP_ERR_NOT_FOUND;

    /* Full index validation is intentionally limited to mount and commit.  A
     * card change must remain O(1), even when the library contains thousands
     * of highlights.  Re-read the small committed header here and validate
     * the selected record below. */
    int slot = s_active_slot;
    char path[LINEPODS_PATH_SIZE];
    slot_path(slot, path, sizeof(path));
    *file = fopen(path, "rb");
    if (!*file) return ESP_FAIL;
    if (!read_exact(*file, header, sizeof(*header))
        || header->magic != LINEPODS_LIBRARY_MAGIC
        || !supported_version(header->version)
        || header->header_size != sizeof(*header)
        || header->committed != LINEPODS_LIBRARY_COMMITTED
        || header->item_count == 0
        || header->index_offset < sizeof(*header)) {
        fclose(*file);
        *file = NULL;
        return ESP_FAIL;
    }
    return ESP_OK;
}

static bool read_quote_at(FILE *file, const library_header_t *header,
                          size_t item_index, library_quote_t *quote,
                          uint32_t *quote_offset)
{
    if (item_index >= header->item_count) return false;
    uint64_t position = (uint64_t)header->index_offset
                      + item_index * sizeof(uint32_t);
    uint64_t index_end = position + sizeof(uint32_t);
    if (position > UINT32_MAX || index_end > header->file_size
        || !seek_to(file, (uint32_t)position)) {
        return false;
    }
    uint32_t offset = 0;
    bool valid = read_exact(file, &offset, sizeof(offset))
        && offset >= sizeof(*header)
        && (uint64_t)offset + sizeof(*quote) <= header->index_offset
        && seek_to(file, offset)
        && read_exact(file, quote, sizeof(*quote))
        && quote->kind == LINEPODS_RECORD_QUOTE
        && quote->record_size == sizeof(*quote) + quote->text_length
        && quote->text_length <= LINEPODS_STORE_MAX_TEXT
        && (uint64_t)offset + quote->record_size <= header->index_offset;
    if (valid && quote_offset) *quote_offset = offset;
    return valid;
}

esp_err_t linepods_store_read_item(size_t item_index, linepods_item_t *item)
{
    if (!item) return ESP_ERR_INVALID_ARG;
    FILE *file = NULL;
    library_header_t header;
    esp_err_t error = open_active(&file, &header);
    if (error != ESP_OK) return error;

    library_quote_t quote;
    uint32_t quote_offset = 0;
    if (!read_quote_at(file, &header, item_index, &quote, &quote_offset)) {
        fclose(file);
        return ESP_ERR_NOT_FOUND;
    }
    library_book_t book;
    if (!read_book_at(file, &header, quote.book_offset, &book)) {
        fclose(file);
        return ESP_FAIL;
    }

    *item = (linepods_item_t) {0};
    memcpy(item->book.book_id, book.book_id, sizeof(item->book.book_id));
    memcpy(item->book.title, book.title, sizeof(item->book.title));
    memcpy(item->book.author, book.author, sizeof(item->book.author));
    item->book.sync_sort = book.sync_sort;
    item->book.note_count = book.note_count;
    memcpy(item->quote.quote_id, quote.quote_id, sizeof(item->quote.quote_id));
    memcpy(item->quote.chapter, quote.chapter, sizeof(item->quote.chapter));
    item->quote.created_at = quote.created_at;
    item->quote.text_length = quote.text_length;
    item->quote.cached = true;

    char preview[LINEPODS_QUOTE_PREVIEW_MAX + 1];
    size_t preview_length = quote.text_length < LINEPODS_QUOTE_PREVIEW_MAX
                          ? quote.text_length : LINEPODS_QUOTE_PREVIEW_MAX;
    if (!seek_to(file, quote_offset + sizeof(quote))
        || !read_exact(file, preview, preview_length)) {
        fclose(file);
        return ESP_FAIL;
    }
    preview[preview_length] = '\0';
    copy_preview(item->quote.preview, sizeof(item->quote.preview), preview);
    fclose(file);
    return ESP_OK;
}

esp_err_t linepods_store_load_content(linepods_content_t *content)
{
    if (!content) return ESP_ERR_INVALID_ARG;
    /* Initialization and a successful commit already validate and select the
     * active slot.  Revalidating the whole library here made the application
     * task synchronously scan every quote immediately after a full sync. */
    if (s_active_slot < 0) refresh_active_slot();
    if (s_active_slot < 0) return ESP_ERR_NOT_FOUND;
    *content = (linepods_content_t) {
        .item_index = SIZE_MAX,
        .item_count = s_active_header.item_count,
        .loaded_book_count = s_active_header.book_count,
        .source_book_count = s_active_header.source_book_count,
        .source_quote_count = s_active_header.source_quote_count,
    };
    esp_err_t error = linepods_store_read_item(0, &content->item);
    if (error == ESP_OK) content->item_index = 0;
    return error;
}

esp_err_t linepods_store_read_text(size_t item_index, char **text,
                                 size_t *length)
{
    if (!text || !length) return ESP_ERR_INVALID_ARG;
    *text = NULL;
    *length = 0;
    FILE *file = NULL;
    library_header_t header;
    esp_err_t error = open_active(&file, &header);
    if (error != ESP_OK) return error;
    library_quote_t quote;
    if (!read_quote_at(file, &header, item_index, &quote, NULL)
        || quote.text_length > LINEPODS_STORE_MAX_TEXT) {
        fclose(file);
        return ESP_ERR_NOT_FOUND;
    }

    char *buffer = malloc((size_t)quote.text_length + 1);
    if (!buffer) {
        fclose(file);
        return ESP_ERR_NO_MEM;
    }
    if (!read_exact(file, buffer, quote.text_length)) {
        free(buffer);
        fclose(file);
        return ESP_FAIL;
    }
    buffer[quote.text_length] = '\0';
    fclose(file);
    *text = buffer;
    *length = quote.text_length;
    return ESP_OK;
}
