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

#define APP_NAMESPACE "app_config"  // Namespace para armazenar as configurações da aplicacao na NVS
#define WIFI_ROUTER_SSID_KEY "wifi_ssid"  // Chave para o SSID do roteador WiFi na NVS
#define WIFI_ROUTER_PASS_KEY "wifi_password"  // Chave para a senha do roteador WiFi na NVS
#define APP_MODE_KEY "app_mode" // Chave para a modo da aplicacao na NVS


static const char *TAG = "APP_CONFIG";

//Salva na NVS o ssid e password do roteador WiFi
esp_err_t nvs_set_wifi_credentials(const char *ssid, const char *password) {
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
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_str(nvs_handle, WIFI_ROUTER_SSID_KEY, ssid, &ssid_len);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs_handle, WIFI_ROUTER_PASS_KEY, password, &password_len);
    }
    nvs_close(nvs_handle);
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
    
    ESP_LOGI(TAG, "Solicitado troca do modo da aplicacao para \"%s\"", 
        mode == APP_MODE_WIFI_MESH_NETWORK ? "MESH_NETWORK" : 
        mode == APP_MODE_WIFI_AP_WEBSERVER ? "AP+WEBSERVER" : "UNKNOWN");

    ESP_LOGI(TAG, "Reiniciando ...");
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