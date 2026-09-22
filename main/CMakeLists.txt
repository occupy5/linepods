idf_component_register(
    SRCS "main.c"
         "linepods_client.c"
         "linepods_config.c"
         "linepods_form.c"
         "linepods_logic.c"
         "linepods_network.c"
         "linepods_pager.c"
         "linepods_power_policy.c"
         "linepods_response_buffer.c"
         "linepods_store.c"
         "linepods_status.c"
         "linepods_sync_policy.c"
         "linepods_ui.c"
         "linepods_wifi_reason.c"
         "../assets/fonts/linepods_font_body_20.c"
         "../assets/fonts/linepods_font_compare_noto_20_1.c"
         "../assets/fonts/linepods_font_compare_pixel_24_1.c"
         "../assets/fonts/linepods_font_title_20.c"
         "../assets/fonts/linepods_font_ui_18.c"
    INCLUDE_DIRS "."
    REQUIRES bsp driver esp_event esp_http_client esp_http_server esp_hw_support
             esp_netif esp_timer esp_wifi json mbedtls nvs_flash spiffs
)
