#include <stdio.h>
#include <inttypes.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "esp_mesh_internal.h"

#include "utils.h"

static const char *TAG = "UTILS";

void print_chip_info(void) {
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    uint8_t mac[6];

    esp_chip_info(&chip_info);
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

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

    printf("MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

char* generate_nodes_list_html(void) {
    // Obtém a tabela de roteamento
    mesh_addr_t routing_table[CONFIG_MESH_ROUTE_TABLE_SIZE];
    int routing_table_size = 0;
    esp_mesh_get_routing_table((mesh_addr_t *)&routing_table, CONFIG_MESH_ROUTE_TABLE_SIZE * 6, &routing_table_size);

    // Cada nó contribui com 26 bytes:
    // - 17 bytes para o endereço MAC (ex: "AA:BB:CC:DD:EE:FF")
    // - 9 bytes para a tag <li></li>
    // A tag <ul></ul> contribui com 9 bytes
    // Adiciona 1 byte para o caractere nulo
    size_t max_len = routing_table_size * 26 + 9 + 1; 
    char *nodes_list = (char *)malloc(max_len);
    if (nodes_list == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for nodes list");
        return NULL;
    }

    // Constrói a lista de endereços MAC dos nós
    snprintf(nodes_list, max_len, "<ul>");
    for (int i = 0; i < routing_table_size; i++) {
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 routing_table[i].addr[0], routing_table[i].addr[1], routing_table[i].addr[2],
                 routing_table[i].addr[3], routing_table[i].addr[4], routing_table[i].addr[5]);
        snprintf(nodes_list + strlen(nodes_list), max_len - strlen(nodes_list), "<li>%s</li>", mac_str);
    }
    snprintf(nodes_list + strlen(nodes_list), max_len - strlen(nodes_list), "</ul>");
    
    return nodes_list;
}
