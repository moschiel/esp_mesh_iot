#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include "esp_system.h"
#include "esp_mac.h"

#include "app_config.h"
#include "utils.h"

#define APP_NAMESPACE "app_config"  // Namespace para armazenar as configurações da aplicacao na NVS
#define WIFI_ROUTER_SSID_KEY "wifi_ssid"  // Chave para o SSID do roteador WiFi na NVS
#define WIFI_ROUTER_PASS_KEY "wifi_password"  // Chave para a senha do roteador WiFi na NVS
#define MESH_ID_KEY "mesh_id"  // Chave para o ID da rede mesh na NVS
#define MESH_PASSWORD_KEY "mesh_password"  // Chave para a senha da rede mesh na NVS
#define APP_MODE_KEY "app_mode" // Chave para a modo da aplicacao na NVS
#define OTA_FW_URL_KEY "ota_fw_url"
#define GATEWAY_IP_KEY "gateway_ip"
#define ROOT_NODE_STATIC_IP_KEY "root_static_ip"
#define NETMASK_KEY "netmask"

static const char *TAG = "APP_CONFIG";

//Salva na NVS o ssid e password do roteador WiFi
esp_err_t nvs_set_wifi_credentials(const char *ssid, const char *password) {
    ESP_LOGI(TAG, "Setting WiFi Router, ssid: %s, password: %s", ssid, password);

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(APP_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(nvs_handle, WIFI_ROUTER_SSID_KEY, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs_handle, WIFI_ROUTER_PASS_KEY, password);
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

//Obtem da NVS o ssid e password do roteador WiFi para se conectar
esp_err_t nvs_get_wifi_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(APP_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs_handle, WIFI_ROUTER_SSID_KEY, ssid, &ssid_len);
        if (err == ESP_OK) {
            err = nvs_get_str(nvs_handle, WIFI_ROUTER_PASS_KEY, password, &password_len);
        }
        nvs_close(nvs_handle);
    }
    #if ALLOW_DEFAULT_WIFI_CREDENTIALS
    if (err != ESP_OK) {
        // Define valores padrão se a leitura da NVS falhar
        strcpy(ssid, DEFAULT_WIFI_SSID);
        strcpy(password, DEFAULT_WIFI_PASSWORD);
        ESP_LOGE(TAG, "Failed to get WiFi router credentials, using default values");
        return ESP_OK;
    }
    #endif

    return err;
}

// Salva na NVS o ID e senha da rede Mesh
esp_err_t nvs_set_mesh_credentials(const uint8_t *mesh_id, const char *mesh_password) {
    ESP_LOGI(TAG, "Setting Mesh, ID: "MACSTR", password: %s", MAC2STR(mesh_id), mesh_password);

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(APP_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(nvs_handle, MESH_ID_KEY, mesh_id, 6);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs_handle, MESH_PASSWORD_KEY, mesh_password);
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if(err != ESP_OK) {
        ESP_LOGI(TAG, "Fail Setting Mesh Config");
    }

    return err;
}

// Obtém da NVS o ID e senha da rede Mesh para se conectar
esp_err_t nvs_get_mesh_credentials(uint8_t *mesh_id, char *mesh_password, size_t mesh_password_len) {
    nvs_handle_t nvs_handle;
    size_t mesh_id_len = 6;
    esp_err_t err = nvs_open(APP_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        err = nvs_get_blob(nvs_handle, MESH_ID_KEY, mesh_id, &mesh_id_len);
        if (err == ESP_OK) {
            err = nvs_get_str(nvs_handle, MESH_PASSWORD_KEY, mesh_password, &mesh_password_len);
        }
        nvs_close(nvs_handle);
    }

    if (err != ESP_OK) {
        // Define valores padrão se a leitura da NVS falhar
        uint8_t default_mesh_id[6] = DEFAULT_MESH_ID;
        memcpy(mesh_id, default_mesh_id, 6);
        strncpy(mesh_password, DEFAULT_MESH_PASSWORD, mesh_password_len);
        mesh_password[mesh_password_len - 1] = '\0'; // Garante terminação nula

        ESP_LOGE(TAG, "Failed to get Mesh credentials, using default values");
    }

    return err;
}

void nvs_set_app_mode(APP_MODE_t mode) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(APP_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return;
    }
    err = nvs_set_i32(nvs_handle, APP_MODE_KEY, (int32_t)mode);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Requested change of app mode to \"%s\"", 
        mode == APP_MODE_WIFI_MESH_NETWORK ? "MESH_NETWORK" : 
        mode == APP_MODE_WIFI_AP_WEBSERVER ? "AP+WEBSERVER" : "UNKNOWN");

    ESP_LOGI(TAG, "Restarting ...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
}

APP_MODE_t nvs_get_app_mode(void) {
    nvs_handle_t nvs_handle;
    int32_t mode = APP_MODE_WIFI_AP_WEBSERVER;  // Default mode
    esp_err_t err = nvs_open(APP_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        nvs_get_i32(nvs_handle, APP_MODE_KEY, &mode);
        nvs_close(nvs_handle);
    }
    return (APP_MODE_t)mode;
}

//Salva na NVS a ultimo url para OTA
esp_err_t nvs_set_ota_fw_url(const char *fw_url) {
    ESP_LOGI(TAG, "Setting fw url: %s", fw_url);

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(APP_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(nvs_handle, OTA_FW_URL_KEY, fw_url);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_get_ota_fw_url(char *fw_url, size_t max_url_len) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(APP_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs_handle, OTA_FW_URL_KEY, fw_url, &max_url_len);
    }
    nvs_close(nvs_handle);

    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get fw url");
    }
    return err;
}

esp_err_t nvs_set_ip_config(const char *gateway_ip, const char *root_node_ip, const char *netmask) {
    ESP_LOGI(TAG, "Setting IP, gateway: %s, root node: %s, netmask: %s", gateway_ip, root_node_ip, netmask);

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(APP_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(nvs_handle, GATEWAY_IP_KEY, gateway_ip);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs_handle, ROOT_NODE_STATIC_IP_KEY, root_node_ip);
        if (err == ESP_OK) {
            err = nvs_set_str(nvs_handle, NETMASK_KEY, netmask);
        }   
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

esp_err_t nvs_get_ip_config(char *gateway_ip, char *root_node_ip, char *netmask) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(APP_NAMESPACE, NVS_READONLY, &nvs_handle);
    size_t len = IP_ADDR_LEN;
    if (err == ESP_OK) {
        len = IP_ADDR_LEN;
        err = nvs_get_str(nvs_handle, GATEWAY_IP_KEY, gateway_ip, &len);
        if (err == ESP_OK) {
            len = IP_ADDR_LEN;
            err = nvs_get_str(nvs_handle, ROOT_NODE_STATIC_IP_KEY, root_node_ip, &len);
            if (err == ESP_OK) {
                len = IP_ADDR_LEN;
                err = nvs_get_str(nvs_handle, NETMASK_KEY, netmask, &len);
            }
        }
    }
    nvs_close(nvs_handle);

    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get static IP configurations");
    }
    return err;
}

