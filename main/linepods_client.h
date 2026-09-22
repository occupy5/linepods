#pragma once

#include "esp_err.h"
#include "linepods_config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LINEPODS_BOOK_ID_MAX 63
#define LINEPODS_TITLE_MAX 127
#define LINEPODS_AUTHOR_MAX 95
#define LINEPODS_QUOTE_ID_MAX 63
#define LINEPODS_QUOTE_PREVIEW_MAX 383
#define LINEPODS_CHAPTER_MAX 127

typedef struct {
    char book_id[LINEPODS_BOOK_ID_MAX + 1];
    char title[LINEPODS_TITLE_MAX + 1];
    char author[LINEPODS_AUTHOR_MAX + 1];
    int64_t sync_sort;
    uint32_t note_count;
} linepods_book_t;

typedef struct {
    char quote_id[LINEPODS_QUOTE_ID_MAX + 1];
    char preview[LINEPODS_QUOTE_PREVIEW_MAX + 1];
    char chapter[LINEPODS_CHAPTER_MAX + 1];
    size_t text_length;
    int64_t created_at;
    bool cached;
} linepods_quote_t;

typedef struct {
    linepods_book_t book;
    linepods_quote_t quote;
} linepods_item_t;

typedef struct {
    linepods_item_t item;
    size_t item_index;
    size_t item_count;
    size_t loaded_book_count;
    size_t source_book_count;
    size_t source_quote_count;
} linepods_content_t;

/* Rebuilds the inactive on-device library slot from every notebook and
 * highlight returned by the official Weixin Read Agent Gateway. Only one notebook
 * page and one book response are held in RAM at a time; the previous committed
 * library remains readable until the new generation is complete. */
esp_err_t linepods_client_sync(const linepods_config_t *config,
                             char *error_message,
                             size_t error_message_size);
