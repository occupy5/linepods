#pragma once

#include "esp_err.h"
#include "linepods_client.h"
#include <stddef.h>

esp_err_t linepods_store_init(void);
esp_err_t linepods_store_sync_begin(void);
esp_err_t linepods_store_sync_add_book(const linepods_book_t *book);
esp_err_t linepods_store_sync_copy_book(const linepods_book_t *book,
                                      size_t *copied_quote_count);
esp_err_t linepods_store_sync_add_quote(const linepods_quote_t *quote,
                                      const char *text, size_t length);
esp_err_t linepods_store_sync_commit(size_t source_book_count,
                                   size_t source_quote_count);
void linepods_store_sync_abort(void);
esp_err_t linepods_store_load_content(linepods_content_t *content);
esp_err_t linepods_store_read_item(size_t item_index, linepods_item_t *item);
esp_err_t linepods_store_read_text(size_t item_index, char **text,
                                 size_t *length);
