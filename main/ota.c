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
#include "esp_https_ota.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mesh.h"

#include "ota.h"
#include "utils.h"
#include "web_socket.h"
#include "messages.h"

#define SKIP_VERSION_CHECK 0

// Certificado do servidor, se necessário
extern const uint8_t server_cert_pem_start[] asm("_binary_server_cert_pem_start");

static const char *TAG = "OTA_MESH";
static bool ota_running = false;
static char ota_url[128];

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

#if SKIP_VERSION_CHECK
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0 &&
        memcmp(new_app_info->date, running_app_info.date, sizeof(new_app_info->date)) == 0 &&
        memcmp(new_app_info->time, running_app_info.time, sizeof(new_app_info->time)) == 0) {
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
    if (ota_config == NULL || ota_config->http_config == NULL) {
        ESP_LOGE(TAG, "esp_https_ota: Invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(ota_config, &https_ota_handle);
    if (https_ota_handle == NULL) {
        return ESP_FAIL;
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
    
    ESP_LOGI(TAG, "Updating root node, Version: %s, Date: %s %s, Size: %u", 
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
        if(percent != lastPercent) {
            lastPercent = percent;
            ESP_LOGD(TAG, "Updating root node: %d bytes ( %u%% )", len_read, percent);
            send_ws_ota_status("Updating root node", false, false, percent);
        }
    }

ota_end:
    if (err != ESP_OK) {
        esp_https_ota_abort(https_ota_handle);
        return err;
    }

    esp_err_t ota_finish_err = esp_https_ota_finish(https_ota_handle);
    if (ota_finish_err != ESP_OK) {
        return ota_finish_err;
    }
    return ESP_OK;
}

static void ota_task(void *arg) {
    ESP_LOGI(TAG, "Started OTA Task, url: %s", ota_url);

    esp_http_client_config_t http_config = {
        .url = ota_url,
        .cert_pem = NULL  // Desabilita a verificação do servidor (somente para testes)
        //.cert_pem = (char *)server_cert_pem_start, // Inclua o certificado do servidor se necessário
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config
    };

    ota_fw_info_t ota_fw_info;

    send_ws_ota_status("Updating root node", false, false, 0);

    if (esp_https_ota_custom(&ota_config, &ota_fw_info) == ESP_OK) 
    {
        ESP_LOGI(TAG, "Root node updated, restarting...");
        send_ws_ota_status("Root node updated, restarting...", true, false, 0);
        //da tempo suficiente de enviar a resposta via ws, e entao reseta
        vTaskDelay(pdMS_TO_TICKS(1000));

        // distribute_firmware_to_children();

        esp_restart();
    }
    else 
    {
        ESP_LOGI(TAG, "OTA Failed...");
        send_ws_ota_status("OTA Failed", true, true, 0);
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

typedef struct {
    uint16_t id;        // Identificador de 2 bytes
    uint32_t offset;    // Offset de 4 bytes
    uint32_t data_size;
    uint32_t total_size;
    uint8_t data[1024];
} firmware_packet_t;


void distribute_firmware_to_children(ota_fw_info_t *ota_fw_info) {
    if (ota_fw_info->update_partition == NULL) {
        ESP_LOGE(TAG, "Running partition not found");
        return;
    }

    firmware_packet_t packet;
    packet.offset = 0;
    packet.total_size = ota_fw_info->size; //ota_fw_info->update_partition->size;
    esp_err_t err;

    // Identificador para pacotes de firmware
    packet.id = 0x0102;

    while (packet.offset < packet.total_size) {
        packet.data_size = (packet.offset + sizeof(packet.data) > packet.total_size) ? (packet.total_size - packet.offset) : sizeof(packet.data);
        err = esp_partition_read(ota_fw_info->update_partition, packet.offset, packet.data, packet.data_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error reading OTA partition: %s", esp_err_to_name(err));
            return;
        }

        // Enviar pacote para os nós filhos
        mesh_data_t data;
        data.data = (uint8_t *)&packet;
        data.size = sizeof(packet.id) + sizeof(packet.offset) + packet.data_size;
        data.proto = MESH_PROTO_BIN;
        data.tos = MESH_TOS_P2P;
        
        err = esp_mesh_send(NULL, &data, MESH_DATA_TODS, NULL, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send firmware bhildren: %s", esp_err_to_name(err));
        }

        packet.offset += packet.data_size;

        // Calcula e registra o progresso
        int progress = (int)(((float)packet.offset / packet.total_size) * 100);
        ESP_LOGI(TAG, "Firmware distribution progress: %d%%", progress);
    }
    ESP_LOGI(TAG, "Firmware distribution complete");
}