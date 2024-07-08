#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include "cJSON.h"
#include "esp_log.h"

#include "messages.h"
#include "utils.h"

static const char* TAG = "MESSAGES";


void tx_msg_node_connected(char* buf, int buf_size, uint8_t node_sta_addr[6], uint8_t parent_sta_addr[6], int layer) {
    // Buffer para armazenar as strings dos endereços MAC
    char node_addr_str[18];
    char parent_addr_str[18];

    // Converte os endereços MAC para strings
    mac_bytes_to_string(node_sta_addr, node_addr_str);
    mac_bytes_to_string(parent_sta_addr, parent_addr_str);

    // Cria o objeto JSON
    cJSON *root = cJSON_CreateObject();

    // Adiciona os campos ao JSON
    cJSON_AddNumberToObject(root, "msg_id", MSG_NODE_CONNECTED);
    cJSON_AddStringToObject(root, "node_addr", node_addr_str);
    cJSON_AddStringToObject(root, "parent_addr", parent_addr_str);
    cJSON_AddNumberToObject(root, "layer", layer);

    // Converte o objeto JSON para string e salva no buffer
    cJSON_PrintPreallocated(root, buf, buf_size, false);

    // Libera a memória usada pelo objeto JSON
    cJSON_Delete(root);
}

bool rx_msg_node_connected(cJSON *root, uint8_t node_sta_addr[6], uint8_t parent_sta_addr[6], int *layer) {
    if (root == NULL) {
        return false;
    }

    // Extrai os campos do JSON
    cJSON *node_addr_json = cJSON_GetObjectItem(root, "node_addr");
    cJSON *parent_addr_json = cJSON_GetObjectItem(root, "parent_addr");
    cJSON *layer_json = cJSON_GetObjectItem(root, "layer");

    if (node_addr_json && cJSON_IsString(node_addr_json) &&
        parent_addr_json && cJSON_IsString(parent_addr_json) &&
        layer_json && cJSON_IsNumber(layer_json)) {
        
        // Converte as strings de endereços MAC para arrays uint8_t
        mac_str_to_bytes(node_addr_json->valuestring, node_sta_addr);
        mac_str_to_bytes(parent_addr_json->valuestring, parent_sta_addr);
        *layer = layer_json->valueint;

        return true;  // Retorna true se tudo estiver correto
    }

    return false;  // Retorna false se houver algum erro nos campos do JSON
}

bool rx_msg_fw_update_request(char* payload, char* fw_url) {
    cJSON *root = cJSON_Parse(payload);
    if (root == NULL) {
        ESP_LOGE(TAG, "Fail to parse JSON");
        return false;
    }

    bool ret;
    cJSON *fw_url_json = cJSON_GetObjectItem(root, "fw_url");
    if(fw_url_json && cJSON_IsString(fw_url_json)) {
        strcpy(fw_url, fw_url_json->valuestring);
        ret = true;
    } else {
        ESP_LOGE(TAG, "Fail to parse 'fw_url'");
        ret = false;
    }

    cJSON_Delete(root);

    return ret;
}


