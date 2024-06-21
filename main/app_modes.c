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

#include "app_modes.h"
#include "app_config.h"
#include "web_server.h"


static void sta_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static const char *TAG = "APP_MODES";

// Configurações do Wi-Fi em modo estação (STA)
wifi_config_t wifi_sta_config = {
    .sta = {
        .ssid = "",
        .password = "",
    }
};

// Inicialização do Wi-Fi em modo estação (STA / cliente)
bool wifi_init_sta(void) {
	ESP_LOGI(TAG, "Iniciando WiFi como estacao");
    char ssid[MAX_SSID_LEN];
    char password[MAX_PASS_LEN];

    // Inicializa a pilha de rede
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    // Configurações padrão do Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Registra manipuladores de eventos
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &sta_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &sta_event_handler, NULL, &instance_got_ip);

    // Get credentials from NVS
    esp_err_t err = nvs_get_wifi_credentials(ssid, sizeof(ssid), password, sizeof(password));
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "Fail to get Wifi Credentials from NVS, going back to AP mode");
        nvs_app_set_mode(APP_MODE_WIFI_AP_WEB_SERVER);
     	return false;
    }
    // Popula Configurações do Wi-Fi
    strncpy((char *)wifi_sta_config.sta.ssid, ssid, sizeof(wifi_sta_config.sta.ssid));
    strncpy((char *)wifi_sta_config.sta.password, password, sizeof(wifi_sta_config.sta.password));

    // Define o modo de operação do Wi-Fi e aplica as configurações
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config);
    // Inicia o Wi-Fi
    esp_wifi_start();
    
    return true;
}

// Manipulador de eventos para Wi-Fi em modo STA
static void sta_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Iniciando conexao a rede Wifi \"%s\"", wifi_sta_config.sta.ssid);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Tentando reconectar a rede WiFi \"%s\"", wifi_sta_config.sta.ssid);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Conectado a rede Wifi \"%s\"", wifi_sta_config.sta.ssid);
    }
}


// Inicialização do Wi-Fi em modo ponto de acesso (AP) + WebServer
void wifi_init_softap(void) {
    ESP_LOGI(TAG, "Iniciando WiFi como Ponto de Acesso");

    // Inicializa a pilha de rede
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    // Configurações padrão do Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Obtém o endereço MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Cria o SSID único
    char unique_ssid[32];
    snprintf(unique_ssid, sizeof(unique_ssid), "ESP32_Config_%02X%02X%02X", mac[3], mac[4], mac[5]);

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = "",
            .ssid_len = 0,
            .password = "esp32config",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    strncpy((char *)wifi_ap_config.ap.ssid, unique_ssid, sizeof(wifi_ap_config.ap.ssid));
    wifi_ap_config.ap.ssid_len = strlen(unique_ssid);

    // Define o modo de operação do Wi-Fi e aplica as configurações
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config);
    esp_wifi_start();
}

void init_app_mode(void) {
    // Inicializa o armazenamento não volátil (NVS)
    nvs_flash_init();

    APP_MODE_t app_mode = nvs_app_get_mode();

    switch(app_mode)
    {
        case APP_MODE_WIFI_AP_WEB_SERVER:
        {
            // Inicializa WiFi como Ponto de Acesso e sobe Webserver
            wifi_init_softap();
            start_webserver();
        }
        break;
        case APP_MODE_WIFI_STA:
        {
            // Tenta Inicializar o Wi-Fi em modo estação
            wifi_init_sta();
        }
        break;
        case APP_MODE_WIFI_MESH_NETWORK:
        {

        }
        break;
    }
}