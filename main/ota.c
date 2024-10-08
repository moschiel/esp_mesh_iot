#include <stdio.h>

#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_https_ota.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mesh.h"

#include "ota.h"
#include "utils.h"
#include "web_socket.h"
#include "messages.h"
#include "mesh_tree.h"

#define VERSION_CHECK 0
#define DATE_TIME_CHECK 0 //por alguma razao, a data de compilacao apenas muda se der 'clean' antes do 'build', nao vale a pena pois demora demais o build
#define FW_PKGS_DELAY 100 //ms

// Certificado do servidor, se necessário
extern const uint8_t server_cert_pem_start[] asm("_binary_server_cert_pem_start");

static const char *TAG = "OTA_MESH";
static bool ota_running = false;
static char ota_url[128];

int32_t requested_retry_offset = -1;

typedef struct {
    esp_app_desc_t appDesc;
    unsigned int size;
    esp_partition_t *update_partition;
} ota_fw_info_t;

//Check if there is at least one child node outdated
static bool is_child_nodes_outdated(char *fwVersionToCompare)
{
    bool ret = false;
    int children_count = 0;
    MeshNode* nodes = get_mesh_tree_table(&children_count);
    if(children_count > 0) {
        if(nodes != NULL) {
            for(int i=0; i<children_count; i++) {
                if(strcmp(fwVersionToCompare, nodes[i].fw_ver) != 0) {
                    ret = true;
                    break;
                }
            }
            free(nodes);
        }
    }
    return ret;
}

//TODO: tem um bug, que se o socket fechar do lado do cliente durante a transmissao,
// o log da esp morre, e apenas volta depois de concluir a transferencia do firmware
static void distribute_firmware_to_children(ota_fw_info_t *ota_fw_info) {
    if(esp_mesh_get_routing_table_size() <= 1) {
        send_ws_msg_ota_status("No CHILD nodes present on mesh network.", true, false, 0);
        return;
    }

    if(is_child_nodes_outdated(ota_fw_info->appDesc.version)) {
        send_ws_msg_ota_status("Updating CHILD nodes", false, false, 0);
    } else {
        send_ws_msg_ota_status("CHILD nodes already has this firmware version installed.", true, false, 0);
        return;
    }
    
    if (ota_fw_info->update_partition == NULL) {
        ESP_LOGE(TAG, "partition not found");
        return;
    }

    firmware_packet_t packet;
    packet.id = BIN_MSG_FW_PACKET; // Identificador para pacotes de firmware
    strcpy(packet.version, ota_fw_info->appDesc.version);
    packet.offset = 0;
    packet.total_size = ota_fw_info->size;
    
    esp_err_t err = ESP_OK;

    while (packet.offset < packet.total_size) {
        packet.data_size = (packet.offset + sizeof(packet.data) > packet.total_size) ? (packet.total_size - packet.offset) : sizeof(packet.data);
        err = esp_partition_read(ota_fw_info->update_partition, packet.offset, packet.data, packet.data_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error reading OTA partition: %s", esp_err_to_name(err));
            break;
        }

        // Envia pacote para os nós filhos
        uint8_t retry = 0;
fw_packet_retry:
        err = mesh_broadcast_bin_msg((uint8_t *)&packet, sizeof(firmware_packet_t));
        
        if(err == ESP_ERR_MESH_QUEUE_FULL && retry++ < 5){
            ESP_LOGW(TAG, "mesh queue full, retry firmware transfer");
            vTaskDelay(pdMS_TO_TICKS(500));
            goto fw_packet_retry;
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send firmware to children: %s", esp_err_to_name(err));
            break;
        }

        uint32_t total_sent_size = packet.offset + packet.data_size;

        // Calcula e registra o progresso
        static uint8_t lastPercent = 0;
        uint8_t percent = (uint8_t)(((float)total_sent_size  / (float)packet.total_size)*100.0);
        if((percent - lastPercent) >= 1) { //atualiza a cada 1%
            lastPercent = percent;
            ESP_LOGI(TAG, "Updating CHILD nodes: total_sent: %lu bytes ( %u%% )", total_sent_size, percent);
            send_ws_msg_ota_status("Updating CHILD nodes", false, false, percent);
        }
        
        packet.offset = total_sent_size;

        vTaskDelay(pdMS_TO_TICKS(FW_PKGS_DELAY));

        if(requested_retry_offset != -1) {
            ESP_LOGW(TAG, "REQUESTED RETRY OFFSET: %li", requested_retry_offset);
            packet.offset = requested_retry_offset;
            requested_retry_offset = -1;
        }
    }

    if(err == ESP_OK) {
        ESP_LOGI(TAG, "Firmware distribution complete");
        send_ws_msg_ota_status("Update CHILD nodes COMPLETE", true, false, 100);
    } else {
        send_ws_msg_ota_status("Update CHILD nodes FAILED...", true, true, 0);
    }
}

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }

#if VERSION_CHECK
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0
#if DATE_TIME_CHECK
        // && memcmp(new_app_info->date, running_app_info.date, sizeof(new_app_info->date)) == 0
        // && memcmp(new_app_info->time, running_app_info.time, sizeof(new_app_info->time)) == 0
#endif
        ) {
        ESP_LOGW(TAG, "Current running version is the same as a new. Aborting update.");
        return ESP_FAIL;
    }
#endif

#ifdef CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
    /**
     * Secure version check from firmware image header prevents subsequent download and flash write of
     * entire firmware image. However this is optional because it is also taken care in API
     * esp_https_ota_finish at the end of OTA update procedure.
     */
    const uint32_t hw_sec_version = esp_efuse_read_secure_version();
    if (new_app_info->secure_version < hw_sec_version) {
        ESP_LOGW(TAG, "New firmware security version is less than eFuse programmed, %"PRIu32" < %"PRIu32, new_app_info->secure_version, hw_sec_version);
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

//baseado na funcao esp_https_ota
static esp_err_t esp_https_ota_custom(const esp_https_ota_config_t *ota_config, ota_fw_info_t * ota_fw_info, bool* isSameFile)
{
    esp_err_t err = ESP_OK;
    send_ws_msg_ota_status("Updating ROOT node", false, false, 0);

    if (ota_config == NULL || ota_config->http_config == NULL) {
        ESP_LOGE(TAG, "esp_https_ota: Invalid argument");
        err = ESP_ERR_INVALID_ARG;
        goto ota_end;
    }

    esp_https_ota_handle_t https_ota_handle = NULL;
    err = esp_https_ota_begin(ota_config, &https_ota_handle);
    if (https_ota_handle == NULL) {
        err = ESP_FAIL;
        goto ota_end;
    } 
    
    err = esp_https_ota_get_img_desc(https_ota_handle, &ota_fw_info->appDesc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
        goto ota_end;
    }

    err = validate_image_header(&ota_fw_info->appDesc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "image header verification failed");
        goto ota_end;
    }

    ota_fw_info->size = esp_https_ota_get_image_size(https_ota_handle);
  
    if(strcmp(ota_fw_info->appDesc.version, CONFIG_APP_PROJECT_VER) == 0) {
        ota_fw_info->update_partition = (esp_partition_t *)esp_ota_get_running_partition();
        *isSameFile = true;
        goto ota_end;
    }

    *isSameFile = false;
    ota_fw_info->update_partition = (esp_partition_t *)esp_ota_get_next_update_partition(NULL);
    
    ESP_LOGI(TAG, "Updating ROOT node, Version: %s, Date: %s %s, Size: %u", 
        ota_fw_info->appDesc.version, ota_fw_info->appDesc.date, ota_fw_info->appDesc.time, ota_fw_info->size);

    
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        //Acompanha progresso
        int len_read = esp_https_ota_get_image_len_read(https_ota_handle);
        static uint8_t lastPercent = 0;
        uint8_t percent = (uint8_t)(((float)len_read / (float)ota_fw_info->size)*100.0);
        if(percent - lastPercent >= 1) {
            lastPercent = percent;
            ESP_LOGD(TAG, "Updating ROOT node: %d bytes ( %u%% )", len_read, percent);
            send_ws_msg_ota_status("Updating ROOT node", false, false, percent);
        }
    }

ota_end:
    if (err != ESP_OK || *isSameFile) {
        esp_https_ota_abort(https_ota_handle);
    } else {
        err = esp_https_ota_finish(https_ota_handle);
    }

    if(err != ESP_OK) {
        ESP_LOGI(TAG, "Updating ROOT node Failed...");
        send_ws_msg_ota_status("Updating ROOT node Failed...", true, true, 0);
    } else if(*isSameFile) {
        ESP_LOGI(TAG, "ROOT node already updated, Version: %s, Date: %s %s, Size: %u", 
            ota_fw_info->appDesc.version, ota_fw_info->appDesc.date, ota_fw_info->appDesc.time, ota_fw_info->size);
        send_ws_msg_ota_status("ROOT node already has this firmware version installed.", false, false, 0);
    } else {
        ESP_LOGI(TAG, "ROOT node update COMPLETE");
        send_ws_msg_ota_status("ROOT node update COMPLETE", false, false, 100);
    }

    return err;
}


static void ota_task(void *arg) {
    ESP_LOGI(TAG, "Started OTA Task, url: %s", ota_url);

    esp_http_client_config_t http_config = {
        .url = ota_url,
        .cert_pem = NULL
        //.cert_pem = (char *)server_cert_pem_start, // Inclua o certificado do servidor se necessário
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config
    };

    ota_fw_info_t ota_fw_info;
    bool isSameFile = false;

    if (esp_https_ota_custom(&ota_config, &ota_fw_info, &isSameFile) == ESP_OK) 
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
        distribute_firmware_to_children(&ota_fw_info);

        if(isSameFile == false) {
            ESP_LOGI(TAG, "Restarting...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
        }
    }

    ota_running = false;
    ESP_LOGW(TAG, "Deleted OTA Task");
    vTaskDelete(NULL);
}

void start_ota(char* url) {
    if (!ota_running) {
        ota_running = true;

        strcpy(ota_url, url);
        xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);
    }
}

