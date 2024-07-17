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

#define VERSION_CHECK 0
#define DATE_TIME_CHECK 0 //por alguma razao, a data de compilacao apenas muda se der 'clean' antes do 'build', nao vale a pena pois demora demais o build
#define FW_PKGS_DELAY 50 //ms

// Certificado do servidor, se necessário
extern const uint8_t server_cert_pem_start[] asm("_binary_server_cert_pem_start");

static const char *TAG = "OTA_MESH";
static bool ota_running = false;
static char ota_url[128];
static const mesh_addr_t mesh_broadcast_addr = {.addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

typedef struct {
    esp_app_desc_t appDesc;
    unsigned int size;
    esp_partition_t *update_partition;
} ota_fw_info_t;

void send_ws_ota_status(char* msg, bool done, bool isError, uint8_t percent) {
    char payload[1000];
    if(mount_msg_ota_status(payload, sizeof(payload), msg, done, isError, percent)) {
        add_ws_msg_to_tx_queue(payload);
    }
}

//TODO: tem um bug, que se o socket fechar do lado do cliente durante a transmissao,
// o log da esp morre, e apenas volta depois de concluir a transferencia do firmware
void distribute_firmware_to_children(ota_fw_info_t *ota_fw_info) {
    send_ws_ota_status("Updating CHILD nodes", false, false, 0);
    
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

        // Enviar pacote para os nós filhos
        mesh_data_t data;
        data.data = (uint8_t *)&packet;
        data.size = sizeof(firmware_packet_t);
        data.proto = MESH_PROTO_BIN;
        data.tos = MESH_TOS_P2P;
        
        uint8_t retry = 0;
fw_packet_retry:
        err = esp_mesh_send(&mesh_broadcast_addr, &data, MESH_DATA_P2P, NULL, 0);
        if(err == ESP_ERR_MESH_QUEUE_FULL && retry++ < 5){
            ESP_LOGW(TAG, "mesh queue full, retry firmware transfer");
            vTaskDelay(pdMS_TO_TICKS(500));
            goto fw_packet_retry;
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send firmware to children: %s", esp_err_to_name(err));
            break;
        }

        packet.offset += packet.data_size;

        // Calcula e registra o progresso
        static uint8_t lastPercent = 0;
        uint8_t percent = (uint8_t)(((float)packet.offset  / (float)packet.total_size)*100.0);
        if(percent - lastPercent >= 1) { //atualiza a cada 1%
            lastPercent = percent;
            ESP_LOGD(TAG, "Updating CHILD nodes: %u bytes ( %u%% )", (unsigned int)packet.offset, percent);
            send_ws_ota_status("Updating CHILD nodes", false, false, percent);
        }
        

        vTaskDelay(pdMS_TO_TICKS(FW_PKGS_DELAY));
    }

    if(err == ESP_OK) {
        ESP_LOGI(TAG, "Firmware distribution complete");
        send_ws_ota_status("Update complete", true, false, 100);
    } else {
        send_ws_ota_status("Update CHILD nodes FAILED...", true, true, 100);
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
static esp_err_t esp_https_ota_custom(const esp_https_ota_config_t *ota_config, ota_fw_info_t * ota_fw_info)
{
    esp_err_t err = ESP_OK;
    send_ws_ota_status("Updating ROOT node", false, false, 0);

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
            send_ws_ota_status("Updating ROOT node", false, false, percent);
        }
    }

ota_end:
    if (err != ESP_OK) {
        esp_https_ota_abort(https_ota_handle);
    } else {
        err = esp_https_ota_finish(https_ota_handle);
    }

    if(err != ESP_OK) {
        ESP_LOGI(TAG, "Updating ROOT node Failed...");
        send_ws_ota_status("Updating ROOT node Failed...", true, true, 0);
    } else {
        ESP_LOGI(TAG, "ROOT node updated");
        send_ws_ota_status("ROOT node updated", false, false, 100);
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

    if (esp_https_ota_custom(&ota_config, &ota_fw_info) == ESP_OK) 
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
        distribute_firmware_to_children(&ota_fw_info);
        ESP_LOGI(TAG, "Restarting...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
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

