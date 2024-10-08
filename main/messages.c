#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mesh.h"
#include "cJSON.h"
#include "esp_mac.h"

#include "web_server.h"
#include "web_socket.h"
#include "app_config.h"
#include "utils.h"
#include "mesh_network.h"
#include "mesh_tree.h"
#include "ota.h"
#include "messages.h"

static const char* TAG = "MESSAGES";
extern int32_t requested_retry_offset;
extern uint8_t STA_MAC_address[6];
static const mesh_addr_t mesh_broadcast_addr = {.addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};


bool mount_msg_node_status(char* buf, int buf_size, uint8_t node_sta_addr[6], uint8_t parent_sta_addr[6], int layer, char* fw_ver) {
    // Buffer para armazenar as strings dos endereços MAC
    char node_addr_str[18];
    char parent_addr_str[18];

    // Converte os endereços MAC para strings
    mac_bytes_to_string(node_sta_addr, node_addr_str);
    mac_bytes_to_string(parent_sta_addr, parent_addr_str);

    // Cria o objeto JSON
    cJSON *root = cJSON_CreateObject();

    // Adiciona os campos ao JSON
    cJSON_AddNumberToObject(root, "msg_id", JSON_MSG_NODE_STATUS);
    cJSON_AddStringToObject(root, "node_addr", node_addr_str);
    cJSON_AddStringToObject(root, "parent_addr", parent_addr_str);
    cJSON_AddNumberToObject(root, "layer", layer);
    cJSON_AddStringToObject(root, "fw_ver", fw_ver); 

    // Converte o objeto JSON para string e salva no buffer
    bool ret = cJSON_PrintPreallocated(root, buf, buf_size, false);

    // Libera a memória usada pelo objeto JSON
    cJSON_Delete(root);

    return ret;
}

bool process_msg_node_status(cJSON *root) {
    if (root == NULL) {
        return false;
    }

    uint8_t node_sta_addr[6];
    uint8_t parent_sta_addr[6];
    int layer;

    // Extrai os campos do JSON
    cJSON *node_addr_json = cJSON_GetObjectItem(root, "node_addr");
    cJSON *parent_addr_json = cJSON_GetObjectItem(root, "parent_addr");
    cJSON *layer_json = cJSON_GetObjectItem(root, "layer");

    if (node_addr_json && cJSON_IsString(node_addr_json) &&
        parent_addr_json && cJSON_IsString(parent_addr_json) &&
        layer_json && cJSON_IsNumber(layer_json))  {
        
        // Converte as strings de endereços MAC para arrays uint8_t
        mac_str_to_bytes(node_addr_json->valuestring, node_sta_addr);
        mac_str_to_bytes(parent_addr_json->valuestring, parent_sta_addr);
        layer = layer_json->valueint;

        //se nao ter fw_ver, reportamos como undefined
        cJSON *fw_version_json = cJSON_GetObjectItem(root, "fw_ver");
        char *fw_ver = "undefined";
        if(fw_version_json && cJSON_IsString(fw_version_json)) {
            fw_ver = fw_version_json->valuestring;
        }

        update_tree_with_node(node_sta_addr, parent_sta_addr, layer, fw_ver);

        return true;  // Retorna true se tudo estiver correto
    }

    return false;  // Retorna false se houver algum erro nos campos do JSON
}

bool mount_msg_get_wifi_config(char* buf, int buf_size) {
// Get WiFi router credentials from NVS
    char router_ssid[MAX_SSID_LEN] = {0};
    char router_password[MAX_PASS_LEN] = {0};
    nvs_get_wifi_credentials(router_ssid, sizeof(router_ssid), router_password, sizeof(router_password));

    // Obtem as credenciais da rede mesh da NVS
    uint8_t mesh_id[6];
    char mesh_password[MAX_PASS_LEN] = {0};
    nvs_get_mesh_credentials(mesh_id, mesh_password, sizeof(mesh_password));

    // Get IP address config
    char router_ip[IP_ADDR_LEN] = {0};
    char root_node_ip[IP_ADDR_LEN] = {0};
    char netmask[IP_ADDR_LEN] = {0};
    #if ENABLE_CONFIG_STATIC_IP
    nvs_get_ip_config(router_ip, root_node_ip, netmask);
    #endif

    //Obtem ultimo fw url
    char fw_url[100] = {0};
    nvs_get_ota_fw_url(fw_url, sizeof(fw_url));

    int required_size = snprintf(buf, buf_size,
        "{"
            "\"device_mac_addr\":\""MACSTR"\","
            "\"router_ssid\":\"%s\","
            "\"router_password\":\"%s\","
            "\"mesh_id\":\""MESHSTR"\"," 
            "\"mesh_password\":\"%s\","
            "\"router_ip\":\"%s\","
            "\"root_ip\":\"%s\","
            "\"netmask\":\"%s\","
            "\"is_connected\":%s,"
            "\"fw_url\":\"%s\""
        "}",
        MAC2STR(STA_MAC_address),
        router_ssid,
        router_password,
        MESH2STR(mesh_id),
        mesh_password,
        router_ip,
        root_node_ip,
        netmask,
        is_mesh_parent_connected()? "true":"false",
        fw_url
    );

    if(required_size >= buf_size) {
        ESP_LOGE(TAG, "Fail to mount msg get_wifi_config");
        return false;
    }
    return true;
}

bool process_msg_set_wifi_config(cJSON *json)
{
    const cJSON *router_ssid = cJSON_GetObjectItem(json, "router_ssid");
    const cJSON *router_password = cJSON_GetObjectItem(json, "router_password");
    const cJSON *mesh_id_str = cJSON_GetObjectItem(json, "mesh_id");
    const cJSON *mesh_password = cJSON_GetObjectItem(json, "mesh_password");
    #if ENABLE_CONFIG_STATIC_IP
    const cJSON *router_ip = cJSON_GetObjectItem(json, "router_ip");
    const cJSON *root_node_ip = cJSON_GetObjectItem(json, "root_ip");
    const cJSON *netmask = cJSON_GetObjectItem(json, "netmask");
    #endif
    
    if(cJSON_IsString(router_ssid) && cJSON_IsString(router_password)) {
        nvs_set_wifi_credentials(router_ssid->valuestring, router_password->valuestring);
    }

    if(cJSON_IsString(mesh_id_str) && cJSON_IsString(mesh_password)) {
        uint8_t mesh_id[6] = {0};
        mac_str_to_bytes(mesh_id_str->valuestring, mesh_id);
        nvs_set_mesh_credentials(mesh_id, mesh_password->valuestring);
    }

    #if ENABLE_CONFIG_STATIC_IP
    if(cJSON_IsString(router_ip) && cJSON_IsString(root_node_ip) && cJSON_IsString(netmask)) {
        nvs_set_ip_config(router_ip->valuestring, root_node_ip->valuestring, netmask->valuestring);
    }
    #endif

    return true;
}

bool process_msg_fw_update_request(char* payload) {
    cJSON *root = cJSON_Parse(payload);
    if (root == NULL) {
        ESP_LOGE(TAG, "Fail to parse JSON");
        return false;
    }

    bool ret;
    cJSON *fw_url_json = cJSON_GetObjectItem(root, "fw_url");
    if(fw_url_json && cJSON_IsString(fw_url_json)) {
        nvs_set_ota_fw_url(fw_url_json->valuestring);
        start_ota(fw_url_json->valuestring);
        ret = true;
    } else {
        ESP_LOGE(TAG, "Fail to parse 'fw_url'");
        ret = false;
    }

    cJSON_Delete(root);

    return ret;
}

bool mount_msg_ota_status(char* buf, int buf_size, char* msg, bool done, bool isError, uint8_t percent) {
    int ret = snprintf(buf, buf_size, "{"
            "\"id\": \"ota_status\","
            "\"msg\": \"%s\","
            "\"isError\": %s,"
            "\"done\": %s,"
            "\"percent\": %u"
        "}", 
        msg, 
        isError? "true" : "false",
        done? "true" : "false",
        percent
    );

    if(ret >= buf_size) {
        ESP_LOGE(TAG, "Fail to mount ota_status msg");
        return false;
    }
    return true;
}

bool send_ws_msg_ota_status(char* msg, bool done, bool isError, uint8_t percent)
{
    char payload[1000];
    if(mount_msg_ota_status(payload, sizeof(payload), msg, done, isError, percent)) {
        add_ws_msg_to_tx_queue(payload);
        return true;
    }
    return false;
}

void send_msg_ota_offset_err(uint32_t expected, uint32_t received) {

    // Cria o objeto JSON
    cJSON *json = cJSON_CreateObject();

    // Adiciona os campos ao JSON
    cJSON_AddNumberToObject(json, "msg_id", JSON_MSG_OTA_OFFSET_ERR);
    cJSON_AddNumberToObject(json, "expected", expected);
    cJSON_AddNumberToObject(json, "received", received);

    // Converte o objeto JSON para string e salva no buffer
    char *jsonString = cJSON_PrintUnformatted(json);
    mesh_root_json_msg(jsonString);
    free(jsonString);

    // Libera a memória usada pelo objeto JSON
    cJSON_Delete(json);

}

bool process_msg_ota_offset_err(cJSON *root) {
    if (root == NULL) {
        return false;
    }

    // Extrai os campos do JSON
    cJSON *expected_json = cJSON_GetObjectItem(root, "expected");
    cJSON *received_json = cJSON_GetObjectItem(root, "received");

    if (expected_json && cJSON_IsNumber(expected_json) &&
        received_json && cJSON_IsNumber(received_json))  {
        
        requested_retry_offset = expected_json->valueint;
        return true;  // Retorna true se tudo estiver correto
    }

    return false;  // Retorna false se houver algum erro nos campos do JSON
}

void process_msg_firmware_packet(firmware_packet_t *packet) {
    static esp_partition_t *ota_partition = NULL; 
    static esp_ota_handle_t ota_handle;
    static uint32_t expectedOffset = 0;
    esp_err_t err;

    // Se receber pacote de uma versao que ja esta instalada, ignora
    bool isSameFile = (strcmp(packet->version, CONFIG_APP_PROJECT_VER) == 0);
    if(isSameFile) {
        return;
    }

    // Se é o primeiro pacote, iniciar OTA
    if (packet->offset == 0) {
        expectedOffset = 0;
        
        ota_partition = (esp_partition_t*)esp_ota_get_next_update_partition(NULL);
        if (ota_partition == NULL) {
            ESP_LOGE(TAG, "No OTA partition found");
            err = ESP_FAIL;
            goto process_fw_packet_end;
        }

        err = esp_ota_begin((const esp_partition_t*)ota_partition, packet->total_size, &ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
            goto process_fw_packet_end;
        }
    } else if(ota_partition == NULL) {
        // Offset diferente de zero, mas nao foi inicializado OTA anteriomente
        //ESP_LOGE(TAG, "rcv offset != 0, but OTA is not initialized");
        return;
    }

    
    // Se recebeu um offset menor, então deve ser retransmissao para algum nó que não tinha recebido, fazemos nada.
    if(packet->offset < expectedOffset)
    {
        return;
    }
    else if(packet->offset > expectedOffset) // Se for maior que o esperado, perdemos dados, solicitamos o reenvio
    {
        err = ESP_FAIL;
        ESP_LOGE(TAG, "Received offset %lu, expected offset %lu", packet->offset, expectedOffset);
        // goto process_fw_packet_end; 

        send_msg_ota_offset_err(expectedOffset, packet->offset); 
        return;       
    }

    expectedOffset = packet->offset + packet->data_size;

    // Gravar os dados do pacote
    err = esp_ota_write(ota_handle, packet->data, packet->data_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
        goto process_fw_packet_end;
    }

    // Calcula e registra o progresso
    static uint8_t lastPercent = 0;
    uint8_t percent = (uint8_t)(((float)(packet->offset + packet->data_size)  / (float)packet->total_size)*100.0);
    if((percent - lastPercent) >= 1) { //atualiza a cada 1%
        lastPercent = percent;
        ESP_LOGI(TAG, "OTA progress: %lu bytes ( %u%% )", (packet->offset + packet->data_size), percent);
    }

    // Se é o último pacote, finalizar a OTA
    if (packet->offset + packet->data_size == packet->total_size) {
        err = esp_ota_end(ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
            goto process_fw_packet_end;
        }

        err = esp_ota_set_boot_partition(ota_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
            goto process_fw_packet_end;
        }

        ESP_LOGI(TAG, "OTA update completed successfully, Restarting...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }

process_fw_packet_end:
    if(err != ESP_OK) {
        ota_partition = NULL;
        expectedOffset = 0;

        ESP_LOGE(TAG, "OTA failed, update aborted...");
        ESP_LOGE(TAG, "fw packet, version: %s, offset: %lu, size: %lu, totalSize: %lu",
            packet->version, packet->offset, packet->data_size, packet->total_size);
    }
}

// broadcast json msg to all nodes
esp_err_t mesh_broadcast_json_msg(char* jsonString)
{
    if(jsonString == NULL) return ESP_FAIL;

    mesh_data_t mesh_data;
    mesh_data.data = (uint8_t*)jsonString;
    mesh_data.tos = MESH_TOS_P2P;
    mesh_data.proto = MESH_PROTO_JSON;
    mesh_data.size = strlen(jsonString) + 1;

    esp_err_t err = esp_mesh_send(
        &mesh_broadcast_addr,       // mesh_addr_t *to, (use NULL to send to root node)
        &mesh_data, // mesh_data_t *data           
        MESH_DATA_P2P,          // int flag, If the packet is to the root and “to” parameter is NULL, set this parameter to 0.
        NULL,       // mesh_opt_t opt[]
        0           // int opt_count
    );

    return err;
}

// broadcast bin msg to all nodes
esp_err_t mesh_broadcast_bin_msg(uint8_t* buf, uint16_t size)
{
    if(buf == NULL) return ESP_FAIL;

    mesh_data_t mesh_data;
    mesh_data.data = buf;
    mesh_data.tos = MESH_TOS_P2P;
    mesh_data.proto = MESH_PROTO_BIN;
    mesh_data.size = size;

    esp_err_t err = esp_mesh_send(
        &mesh_broadcast_addr,       // mesh_addr_t *to, (use NULL to send to root node)
        &mesh_data, // mesh_data_t *data           
        MESH_DATA_P2P,          // int flag, If the packet is to the root and “to” parameter is NULL, set this parameter to 0.
        NULL,       // mesh_opt_t opt[]
        0           // int opt_count
    );

    return err;
}

// json msg to root node
esp_err_t mesh_root_json_msg(char* jsonString)
{
    if(jsonString == NULL) return ESP_FAIL;

    mesh_data_t mesh_data;
    mesh_data.data = (uint8_t*)jsonString;
    mesh_data.tos = MESH_TOS_P2P;
    mesh_data.proto = MESH_PROTO_JSON;
    mesh_data.size = strlen(jsonString) + 1;

    esp_err_t err = esp_mesh_send(
        NULL,       // mesh_addr_t *to, (use NULL to send to root node)
        &mesh_data, // mesh_data_t *data           
        MESH_DATA_P2P,          // int flag, If the packet is to the root and “to” parameter is NULL, set this parameter to 0.
        NULL,       // mesh_opt_t opt[]
        0           // int opt_count
    );

    return err;
}