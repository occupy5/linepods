#include "linepods_client.h"

#include "linepods_logic.h"
#include "linepods_response_buffer.h"
#include "linepods_store.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINEPODS_GATEWAY_URL "https://i.weread.qq.com/api/agent/gateway"
#define LINEPODS_SKILL_VERSION "1.0.4"
#define NOTEBOOK_RESPONSE_MAX (24 * 1024)
#define HIGHLIGHT_RESPONSE_MAX (64 * 1024)
/* Keep one notebook page small enough that cJSON can build the reply tree
 * inside the ~60 KB heap left while Wi-Fi/TLS are active; cJSON needs roughly
 * four times the JSON text in heap, so a 20-book page exhausts the device. */
#define NOTEBOOK_PAGE_SIZE 6
#define NOTEBOOK_PAGE_LIMIT 256
#define RESPONSE_INITIAL_CAPACITY (4 * 1024)

static const char *TAG = "linepods_client";

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    size_t limit;
    bool overflow;
    bool no_memory;
} response_buffer_t;

typedef struct {
    esp_http_client_handle_t client;
    response_buffer_t response;
    char authorization[LINEPODS_API_KEY_MAX + 16];
    size_t request_count;
} gateway_session_t;

static void set_error(char *destination, size_t capacity, const char *message)
{
    if (!destination || capacity == 0) return;
    snprintf(destination, capacity, "%s", message ? message : "Unknown error");
}

static esp_err_t http_event(esp_http_client_event_t *event)
{
    response_buffer_t *buffer = event->user_data;
    if (!buffer || event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0) {
        return ESP_OK;
    }
    size_t incoming = (size_t)event->data_len;
    size_t required = buffer->length + incoming + 1;
    if (required < buffer->length || required > buffer->limit) {
        buffer->overflow = true;
        return ESP_OK;
    }
    if (required > buffer->capacity) {
        size_t next = linepods_response_next_capacity(buffer->capacity, required,
                                                    buffer->limit);
        char *grown = next > 0 ? realloc(buffer->data, next) : NULL;
        if (!grown) {
            buffer->no_memory = true;
            return ESP_FAIL;
        }
        buffer->data = grown;
        buffer->capacity = next;
    }
    memcpy(buffer->data + buffer->length, event->data, incoming);
    buffer->length += incoming;
    buffer->data[buffer->length] = '\0';
    return ESP_OK;
}

static esp_err_t gateway_session_open(gateway_session_t *session,
                                      const char *api_key)
{
    if (!session || !api_key) return ESP_ERR_INVALID_ARG;
    *session = (gateway_session_t) {0};
    snprintf(session->authorization, sizeof(session->authorization),
             "Bearer %s", api_key);
    esp_http_client_config_t http_config = {
        .url = LINEPODS_GATEWAY_URL,
        .event_handler = http_event,
        .user_data = &session->response,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
        .keep_alive_enable = true,
        .keep_alive_idle = 5,
        .keep_alive_interval = 5,
        .keep_alive_count = 3,
    };
    session->client = esp_http_client_init(&http_config);
    if (!session->client) {
        memset(session->authorization, 0, sizeof(session->authorization));
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_set_method(session->client, HTTP_METHOD_POST);
    esp_http_client_set_header(session->client, "Authorization",
                               session->authorization);
    esp_http_client_set_header(session->client, "Content-Type",
                               "application/json");
    esp_http_client_set_header(session->client, "Accept", "application/json");
    return ESP_OK;
}

static void gateway_session_close(gateway_session_t *session)
{
    if (!session) return;
    if (session->client) esp_http_client_cleanup(session->client);
    free(session->response.data);
    memset(session, 0, sizeof(*session));
}

static esp_err_t gateway_request(gateway_session_t *session, const char *body,
                                 size_t response_limit, cJSON **document,
                                 char *error_message, size_t error_message_size)
{
    if (!session || !session->client) return ESP_ERR_INVALID_STATE;
    *document = NULL;
    size_t maximum_capacity = response_limit + 1;
    size_t initial_capacity = maximum_capacity < RESPONSE_INITIAL_CAPACITY + 1
                            ? maximum_capacity : RESPONSE_INITIAL_CAPACITY + 1;
    free(session->response.data);
    session->response = (response_buffer_t) {
        .data = calloc(1, initial_capacity),
        .capacity = initial_capacity,
        .limit = maximum_capacity,
    };
    response_buffer_t *response = &session->response;
    if (!response->data) {
        ESP_LOGE(TAG, "Response allocation failed: requested=%u free=%u largest=%u",
                 (unsigned)initial_capacity,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        set_error(error_message, error_message_size, "内存不足，无法接收微信读书数据");
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_post_field(session->client, body, strlen(body));

    esp_err_t error = esp_http_client_perform(session->client);
    int status = esp_http_client_get_status_code(session->client);
    session->request_count++;

    ESP_LOGI(TAG, "Gateway response: bytes=%u capacity=%u free=%u largest=%u",
             (unsigned)response->length, (unsigned)response->capacity,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    if (response->no_memory) {
        error = ESP_ERR_NO_MEM;
        set_error(error_message, error_message_size,
                  "单本书划线数据较多，设备内存不足；旧缓存已保留");
    } else if (error != ESP_OK) {
        snprintf(error_message, error_message_size, "网络请求失败（%s）",
                 esp_err_to_name(error));
    } else if (response->overflow) {
        error = ESP_ERR_INVALID_SIZE;
        set_error(error_message, error_message_size, "微信读书返回内容过大，设备无法处理");
    } else if (status != 200) {
        error = ESP_ERR_INVALID_RESPONSE;
        if (status == 401 || status == 403) {
            set_error(error_message, error_message_size,
                      "API Key 无效或已过期，请重新获取后提交");
        } else if (status == 429) {
            set_error(error_message, error_message_size,
                      "请求有些频繁，请稍等几分钟再试");
        } else if (status >= 500) {
            set_error(error_message, error_message_size,
                      "微信读书服务暂时不可用，请稍后再试");
        } else {
            snprintf(error_message, error_message_size,
                     "微信读书暂时拒绝了请求（HTTP %d）", status);
        }
    } else {
        *document = cJSON_ParseWithLength(response->data, response->length);
        if (!*document) {
            const char *error_pointer = cJSON_GetErrorPtr();
            size_t error_offset = response->length;
            if (error_pointer) {
                uintptr_t base = (uintptr_t)response->data;
                uintptr_t position = (uintptr_t)error_pointer;
                if (position >= base && position - base <= response->length) {
                    error_offset = (size_t)(position - base);
                }
            }
            /* cJSON reports both malformed payloads and allocation failures here, so
             * log where parsing stopped next to the remaining heap. */
            ESP_LOGE(TAG, "JSON parse rejected: bytes=%u offset=%u free=%u largest=%u",
                     (unsigned)response->length, (unsigned)error_offset,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            error = ESP_ERR_INVALID_RESPONSE;
            set_error(error_message, error_message_size, "微信读书返回了无法解析的数据");
        }
    }
    free(response->data);
    response->data = NULL;
    response->length = 0;
    response->capacity = 0;

    if (error != ESP_OK) return error;

    cJSON *upgrade = cJSON_GetObjectItemCaseSensitive(*document, "upgrade_info");
    if (cJSON_IsObject(upgrade)) {
        cJSON_Delete(*document);
        *document = NULL;
        set_error(error_message, error_message_size, "微信读书接口版本已更新，请升级固件");
        return ESP_ERR_INVALID_VERSION;
    }

    cJSON *errcode = cJSON_GetObjectItemCaseSensitive(*document, "errcode");
    if (cJSON_IsNumber(errcode) && errcode->valuedouble != 0) {
        cJSON *errmsg = cJSON_GetObjectItemCaseSensitive(*document, "errmsg");
        set_error(error_message, error_message_size,
                  cJSON_IsString(errmsg) ? errmsg->valuestring : "微信读书拒绝了请求");
        cJSON_Delete(*document);
        *document = NULL;
        return ESP_FAIL;
    }
    return ESP_OK;
}

static cJSON *payload(cJSON *document)
{
    cJSON *data = cJSON_GetObjectItemCaseSensitive(document, "data");
    return cJSON_IsObject(data) ? data : document;
}

static const char *json_string(cJSON *object, const char *key)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(value) ? value->valuestring : "";
}

static int64_t json_integer(cJSON *object, const char *key, int64_t fallback)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsNumber(value) ? (int64_t)value->valuedouble : fallback;
}

static void copy_book(linepods_book_t *destination, cJSON *notebook)
{
    cJSON *book = cJSON_GetObjectItemCaseSensitive(notebook, "book");
    if (!cJSON_IsObject(book)) book = notebook;
    linepods_utf8_copy(destination->book_id, sizeof(destination->book_id),
                     json_string(notebook, "bookId"));
    if (!destination->book_id[0]) {
        linepods_utf8_copy(destination->book_id, sizeof(destination->book_id),
                         json_string(book, "bookId"));
    }
    linepods_utf8_copy(destination->title, sizeof(destination->title),
                     json_string(book, "title"));
    linepods_utf8_copy(destination->author, sizeof(destination->author),
                     json_string(book, "author"));
}

static const char *chapter_title(cJSON *chapters, int64_t chapter_uid)
{
    if (!cJSON_IsArray(chapters)) return "";
    cJSON *chapter = NULL;
    cJSON_ArrayForEach(chapter, chapters) {
        if (json_integer(chapter, "chapterUid", -1) == chapter_uid) {
            return json_string(chapter, "title");
        }
    }
    return "";
}

static void copy_quote(linepods_quote_t *destination, cJSON *item,
                       cJSON *chapters)
{
    const char *text = json_string(item, "markText");
    size_t source_length = strlen(text);
    linepods_utf8_copy(destination->preview, sizeof(destination->preview), text);
    destination->text_length = source_length;
    linepods_utf8_copy(destination->quote_id, sizeof(destination->quote_id),
                     json_string(item, "bookmarkId"));
    int64_t chapter_uid = json_integer(item, "chapterUid", -1);
    linepods_utf8_copy(destination->chapter, sizeof(destination->chapter),
                     chapter_title(chapters, chapter_uid));
    destination->created_at = json_integer(item, "createTime", 0);
    destination->cached = true;
}

static esp_err_t fetch_highlights(gateway_session_t *session,
                                  const linepods_book_t *requested_book,
                                  size_t *added_count,
                                  size_t *source_quote_count,
                                  char *error_message,
                                  size_t error_message_size)
{
    if (!requested_book || !requested_book->book_id[0]
        || !added_count || !source_quote_count) {
        return ESP_ERR_INVALID_ARG;
    }
    *added_count = 0;
    char body[256];
    snprintf(body, sizeof(body),
             "{\"api_name\":\"/book/bookmarklist\",\"bookId\":\"%s\","
             "\"skill_version\":\"%s\"}",
             requested_book->book_id, LINEPODS_SKILL_VERSION);

    cJSON *document = NULL;
    esp_err_t error = gateway_request(session, body, HIGHLIGHT_RESPONSE_MAX,
                                      &document, error_message, error_message_size);
    if (error != ESP_OK) return error;

    cJSON *response = payload(document);
    cJSON *updated = cJSON_GetObjectItemCaseSensitive(response, "updated");
    cJSON *chapters = cJSON_GetObjectItemCaseSensitive(response, "chapters");
    cJSON *book = cJSON_GetObjectItemCaseSensitive(response, "book");
    if (!cJSON_IsArray(updated)) {
        cJSON_Delete(document);
        set_error(error_message, error_message_size, "微信读书没有返回划线列表");
        return ESP_ERR_INVALID_RESPONSE;
    }

    linepods_book_t resolved_book = *requested_book;
    if (cJSON_IsObject(book)) {
        const char *title = json_string(book, "title");
        const char *author = json_string(book, "author");
        if (title[0]) {
            linepods_utf8_copy(resolved_book.title, sizeof(resolved_book.title), title);
        }
        if (author[0]) {
            linepods_utf8_copy(resolved_book.author, sizeof(resolved_book.author), author);
        }
    }

    bool book_started = false;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, updated) {
        const char *text = json_string(item, "markText");
        if (!text[0]) continue;
        (*source_quote_count)++;
        if (!book_started) {
            error = linepods_store_sync_add_book(&resolved_book);
            if (error != ESP_OK) break;
            book_started = true;
        }
        linepods_quote_t quote = {0};
        copy_quote(&quote, item, cJSON_IsArray(chapters) ? chapters : NULL);
        error = linepods_store_sync_add_quote(&quote, text, strlen(text));
        if (error != ESP_OK) break;
        (*added_count)++;
    }
    cJSON_Delete(document);

    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Cache append rejected: book=%s error=%s",
                 requested_book->book_id, esp_err_to_name(error));
        set_error(error_message, error_message_size,
                  error == ESP_ERR_INVALID_SIZE
                      ? "单条划线超过 64 KB，设备无法保存"
                      : error == ESP_ERR_NO_MEM
                          ? "设备存储空间不足，无法保存全部划线"
                          : "无法把完整划线保存到设备存储");
        return error;
    }
    if (*added_count == 0) {
        set_error(error_message, error_message_size, "这本书没有可显示的划线内容");
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

static esp_err_t sync_notebooks(gateway_session_t *session,
                                size_t *loaded_book_count,
                                size_t *reused_book_count,
                                size_t *source_book_count,
                                size_t *source_quote_count,
                                char *error_message,
                                size_t error_message_size)
{
    linepods_book_t *books = calloc(NOTEBOOK_PAGE_SIZE, sizeof(*books));
    if (!books) {
        set_error(error_message, error_message_size, "内存不足，无法准备书籍列表");
        return ESP_ERR_NO_MEM;
    }
    int64_t last_sort = 0;
    bool has_last_sort = false;
    bool completed = false;
    esp_err_t error = ESP_OK;

    for (size_t page_number = 0; page_number < NOTEBOOK_PAGE_LIMIT; page_number++) {
        char body[224];
        if (has_last_sort) {
            snprintf(body, sizeof(body),
                     "{\"api_name\":\"/user/notebooks\",\"count\":%d,"
                     "\"lastSort\":%" PRId64 ",\"skill_version\":\"%s\"}",
                     NOTEBOOK_PAGE_SIZE, last_sort, LINEPODS_SKILL_VERSION);
        } else {
            snprintf(body, sizeof(body),
                     "{\"api_name\":\"/user/notebooks\",\"count\":%d,"
                     "\"skill_version\":\"%s\"}",
                     NOTEBOOK_PAGE_SIZE, LINEPODS_SKILL_VERSION);
        }

        cJSON *document = NULL;
        error = gateway_request(session, body, NOTEBOOK_RESPONSE_MAX,
                                &document, error_message, error_message_size);
        if (error != ESP_OK) break;
        cJSON *page = payload(document);
        cJSON *array = cJSON_GetObjectItemCaseSensitive(page, "books");
        if (!cJSON_IsArray(array)) {
            cJSON_Delete(document);
            set_error(error_message, error_message_size,
                      "微信读书没有返回笔记本列表");
            error = ESP_ERR_INVALID_RESPONSE;
            break;
        }

        size_t book_count = 0;
        int64_t next_sort = last_sort;
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, array) {
            next_sort = json_integer(item, "sort", next_sort);
            if (json_integer(item, "noteCount", 0) <= 0) continue;
            if (book_count >= NOTEBOOK_PAGE_SIZE) {
                error = ESP_ERR_INVALID_SIZE;
                set_error(error_message, error_message_size,
                          "微信读书单页书籍数量超出设备处理范围");
                break;
            }
            linepods_book_t *book = &books[book_count++];
            copy_book(book, item);
            book->sync_sort = json_integer(item, "sort", 0);
            int64_t note_count = json_integer(item, "noteCount", 0);
            book->note_count = note_count > UINT32_MAX
                             ? UINT32_MAX : (uint32_t)note_count;
            (*source_book_count)++;
        }
        bool has_more = json_integer(page, "hasMore", 0) == 1;
        bool progressed = !has_last_sort || next_sort != last_sort;
        cJSON_Delete(document);
        if (error != ESP_OK) break;

        for (size_t i = 0; i < book_count; i++) {
            size_t added = 0;
            error = linepods_store_sync_copy_book(&books[i], &added);
            if (error == ESP_OK) {
                *source_quote_count += added;
                (*loaded_book_count)++;
                (*reused_book_count)++;
                continue;
            }
            if (error != ESP_ERR_NOT_FOUND) break;
            error = fetch_highlights(session, &books[i], &added,
                                     source_quote_count,
                                     error_message, error_message_size);
            if (error == ESP_ERR_NOT_FOUND) {
                error = ESP_OK;
                continue;
            }
            if (error != ESP_OK) break;
            if (added > 0) (*loaded_book_count)++;
        }
        if (error != ESP_OK) break;
        if (!has_more) {
            completed = true;
            break;
        }
        if (!progressed) {
            set_error(error_message, error_message_size, "微信读书分页数据异常");
            error = ESP_ERR_INVALID_RESPONSE;
            break;
        }
        last_sort = next_sort;
        has_last_sort = true;
    }
    free(books);

    if (error != ESP_OK) return error;
    if (!completed) {
        set_error(error_message, error_message_size, "微信读书书籍数量超出同步页数上限");
        return ESP_ERR_INVALID_SIZE;
    }
    if (*source_book_count == 0) {
        set_error(error_message, error_message_size, "没有找到带划线内容的书");
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

esp_err_t linepods_client_sync(const linepods_config_t *config,
                             char *error_message,
                             size_t error_message_size)
{
    if (!linepods_config_valid(config)) return ESP_ERR_INVALID_ARG;
    set_error(error_message, error_message_size, "");
    gateway_session_t session;
    esp_err_t error = gateway_session_open(&session, config->api_key);
    if (error != ESP_OK) {
        set_error(error_message, error_message_size, "无法创建安全网络连接");
        return error;
    }
    error = linepods_store_sync_begin();
    if (error != ESP_OK) {
        gateway_session_close(&session);
        set_error(error_message, error_message_size,
                  "无法创建新的划线缓存，请检查设备存储空间");
        return error;
    }

    size_t loaded_books = 0;
    size_t reused_books = 0;
    size_t source_books = 0;
    size_t source_quotes = 0;
    ESP_LOGI(TAG, "Synchronizing the complete Weixin Read notebook library");
    error = sync_notebooks(&session, &loaded_books, &reused_books,
                           &source_books, &source_quotes,
                           error_message, error_message_size);
    if (error == ESP_OK) {
        error = linepods_store_sync_commit(source_books, source_quotes);
        if (error != ESP_OK) {
            set_error(error_message, error_message_size,
                      error == ESP_ERR_NO_MEM
                          ? "设备存储空间不足，无法保存全部划线"
                          : "无法提交完整划线缓存");
        }
    }
    if (error != ESP_OK) {
        linepods_store_sync_abort();
        gateway_session_close(&session);
        return error;
    }

    set_error(error_message, error_message_size, "");
    ESP_LOGI(TAG, "Synchronized books=%u reused=%u candidate_books=%u quotes=%u requests=%u",
             (unsigned)loaded_books, (unsigned)reused_books,
             (unsigned)source_books, (unsigned)source_quotes,
             (unsigned)session.request_count);
    gateway_session_close(&session);
    return ESP_OK;
}
