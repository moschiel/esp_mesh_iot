#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "esp_mesh_internal.h"
#include "esp_http_server.h"

#include "utils.h"

extern uint8_t STA_MAC_address[6];

//static const char *TAG = "UTILS";

void print_chip_info(void) {
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;

    esp_chip_info(&chip_info);

    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    printf("MAC address: "MACSTR"\n", MAC2STR(STA_MAC_address));
}

// Função para converter uma string no formato MAC para um array de bytes
void mac_str_to_bytes(const char *mac_str, uint8_t *mac_bytes) {
    for (int i = 0; i < 6; ++i) {
        sscanf(mac_str + 3 * i, "%2hhx", &mac_bytes[i]);
    }
}

void mac_bytes_to_string(uint8_t mac[6], char *mac_str) {
    sprintf(mac_str, MACSTR, MAC2STR(mac));
}

// Função para comparar endereços MAC
bool compare_mac(uint8_t mac1[6], uint8_t mac2[6]) {
    return memcmp(mac1, mac2, 6) == 0;
}

// Função auxiliar para converter o endereço MAC para a string com os três últimos bytes
void format_mac(char *str, const uint8_t *mac) {
    sprintf(str, ""MACSTREND"", MAC2STREND(mac));
}

// Envia resposta string a uma requisicao http de chunk em chunk
void httpd_resp_send_str_chunk(httpd_req_t *req, char *response) {
    const size_t total_size = strlen(response);
    const int chunk_size = 1000;
    char *pData = response;
    size_t offset = 0;
    size_t size_to_send = 0;

    while (offset < total_size) {
        size_to_send = total_size - offset < chunk_size ? total_size - offset : chunk_size;
        httpd_resp_send_chunk(req, pData, size_to_send);
        offset += chunk_size;
        pData += size_to_send;
    }
}

// Formata resposta string a uma requisicao http, e envia
void httpd_resp_send_format_str_chunk(httpd_req_t *req, char *buffer, size_t buffer_size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, buffer_size, format, args);
    va_end(args);

    httpd_resp_send_chunk(req, buffer, HTTPD_RESP_USE_STRLEN);
}
