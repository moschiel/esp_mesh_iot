#include <stdio.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_mesh.h"

#include "ota.h"
#include "utils.h"

// Certificado do servidor, se necessário
extern const uint8_t server_cert_pem_start[] asm("_binary_server_cert_pem_start");

static const char *TAG = "OTA_Mesh_Root";
static bool ota_running = false;
static bool is_ws_req = false;
//referencia ao socket que solicitou OTA
static httpd_req_t ws_ota_req_ref;
static char ota_url[128];


void send_ws_ota_status(httpd_req_t* req, char* msg, bool done) {
    char buf[500];
    sprintf(buf, "{"
            "\"msg\": \"%s\","
            "\"done\": %s"
        "}", 
        msg, 
        done? "true" : "false"
    );

    send_ws_frame_str(req, buf);
}

static void ota_task(void *arg) {
    ESP_LOGI(TAG, "Started OTA Task, url: %s", ota_url);
    if(is_ws_req)
        send_ws_ota_status(&ws_ota_req_ref, "Started OTA", false);

    esp_http_client_config_t http_config = {
        .url = ota_url,
        //.event_handler = _http_event_handler,
        //.cert_pem = (char *)server_cert_pem_start, // Inclua o certificado do servidor se necessário
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    if (esp_https_ota(&ota_config) == ESP_OK) 
    {
        ESP_LOGI(TAG, "OTA Succeed, Rebooting...");
        if(is_ws_req) 
        {
            send_ws_ota_status(&ws_ota_req_ref, "OTA Succeed, Rebooting...", true);
            //da tempo suficiente de enviar a resposta http, e entao reseta
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        esp_restart();
    } 
    else 
    {
        ESP_LOGI(TAG, "OTA Failed...");
        if(is_ws_req) {
            send_ws_ota_status(&ws_ota_req_ref, "OTA Failed", true);
        }
    }

    ota_running = false;
    is_ws_req = false;

    ESP_LOGW(TAG, "Deleted OTA Task");

    vTaskDelete(NULL);
}

void start_ota(char* url, httpd_req_t *req) {
    if (!ota_running) {
        ota_running = true;

        if(req != NULL) {
            memcpy(&ws_ota_req_ref, req, sizeof(httpd_req_t));
            is_ws_req = true;
        } else {
            is_ws_req = false;
        }

        strcpy(ota_url, url);
        xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);
    }
}

/*
void distribute_firmware_to_children() {
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    if (running_partition == NULL) {
        ESP_LOGE(TAG, "Running partition not found");
        return;
    }

    // Lê o firmware da partição OTA em pacotes de 1024 bytes
    uint8_t buffer[1024];
    size_t offset = 0;
    size_t bytes_read;
    esp_err_t err;
    while (offset < running_partition->size) {
        bytes_read = (offset + sizeof(buffer) > running_partition->size) ? (running_partition->size - offset) : sizeof(buffer);
        err = esp_partition_read(running_partition, offset, buffer, bytes_read);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error reading OTA partition: %s", esp_err_to_name(err));
            return;
        }

        // Enviar pacote para os nós filhos
        mesh_data_t data;
        data.data = buffer;
        data.size = bytes_read;
        data.proto = MESH_PROTO_BIN;
        data.tos = MESH_TOS_P2P;

        // Enviar dados para os filhos
        for (int i = 0; i < tree_node_count; i++) {
            err = esp_mesh_send(NULL, &data, 0, NULL, 0);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send firmware to child node: %d", err);
            }
        }

        offset += bytes_read;
    }
    ESP_LOGI(TAG, "Firmware distributed to all child nodes");
}
*/
