#include "wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include "web_server.h"
#include "esp_system.h"
#include "esp_mac.h"

#define WIFI_NAMESPACE "wifi_config"  // Namespace para armazenar as configurações Wi-Fi (STA) na NVS
#define WIFI_SSID_KEY "ssid"  // Chave para o SSID na NVS
#define WIFI_PASS_KEY "password"  // Chave para a senha na NVS
#define WIFI_MODE_KEY "mode" // Chave para a modo na NVS
#define MAX_SSID_LEN 32  // Comprimento máximo do SSID
#define MAX_PASS_LEN 64  // Comprimento máximo da senha

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static const char *TAG = "WIFI";
static bool is_sta_mode = true;  // Flag para indicar o modo STA
static bool sta_connected = false;
static esp_netif_t *sta_netif = NULL;  // netif para o modo STA
static esp_netif_t *ap_netif = NULL; //netif para o modo AP

// Configurações do Wi-Fi STA
wifi_config_t wifi_sta_config = {
    .sta = {
        .ssid = "",
        .password = "",
    }
};

// Inicialização do Wi-Fi em modo estação (STA / cliente)
bool wifi_init_sta(void) {
    wifi_stop();

	is_sta_mode = true;
	
	ESP_LOGI(TAG, "Iniciando WiFi como estacao");
    char ssid[MAX_SSID_LEN];
    char password[MAX_PASS_LEN];
	
    // Inicializa a pilha de rede
    esp_netif_init();
    esp_event_loop_create_default();
    sta_netif = esp_netif_create_default_wifi_sta();

    // Configurações padrão do Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Registra manipuladores de eventos
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip);

    // Get credentials from NVS
    esp_err_t err = nvs_wifi_get_credentials(ssid, sizeof(ssid), password, sizeof(password));
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "Fail to get Wifi Credentials from NVS, going back to AP mode");
        nvs_wifi_set_mode(WIFI_MODE_AP, false);
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

// Inicialização do Wi-Fi em modo ponto de acesso (AP) e inicia WebServer
void wifi_init_softap(void) {
    wifi_stop();

    ESP_LOGI(TAG, "Iniciando WiFi como Ponto de Acesso");
    esp_netif_init();
    esp_event_loop_create_default();
    ap_netif = esp_netif_create_default_wifi_ap();

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

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config);
    esp_wifi_start();

    start_webserver();
}

// Para o Wi-Fi
void wifi_stop(void) {
    is_sta_mode = false;
    sta_connected = false;

    esp_wifi_stop();
    esp_wifi_deinit();
    
    if (sta_netif) {
        esp_netif_destroy(sta_netif);
        sta_netif = NULL;
    }

    if (ap_netif) {
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
    }

    esp_wifi_set_mode(WIFI_MODE_NULL);
}

//Salva na NVS o ssid e password do roteador WiFi
esp_err_t nvs_wifi_set_credentials(const char *ssid, const char *password) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(nvs_handle, WIFI_SSID_KEY, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(nvs_handle, WIFI_PASS_KEY, password);
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

//Obtem da NVS o ssid e password do roteador WiFi para se conectar
esp_err_t nvs_wifi_get_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_str(nvs_handle, WIFI_SSID_KEY, ssid, &ssid_len);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs_handle, WIFI_PASS_KEY, password, &password_len);
    }
    nvs_close(nvs_handle);
    return err;
}

void nvs_wifi_set_mode(int mode, bool restart_esp) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return;
    }
    err = nvs_set_i32(nvs_handle, WIFI_MODE_KEY, mode);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Solicitado troca do modo do WiFi para \"%s\"", mode == WIFI_MODE_STA ? "STA" : "AP");

    if(restart_esp) {
        ESP_LOGI(TAG, "Reiniciando ...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        //se não é pra reiniciar ESP, então ja altera o modo WiFi sem resetar a ESP
        if(mode == WIFI_MODE_STA) {
            //Inicializa WiFi como estacao
            wifi_init_sta();
        }
        else {
            // Inicializa WiFi como Ponto de Acesso e sobe Webserver
            wifi_init_softap();
        }
    }
}

int nvs_wifi_get_mode(void) {
    nvs_handle_t nvs_handle;
    int32_t mode = WIFI_MODE_AP;  // Default mode
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        nvs_get_i32(nvs_handle, WIFI_MODE_KEY, &mode);
        nvs_close(nvs_handle);
    }
    return mode;
}

// Manipulador de eventos para Wi-Fi
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	if (!is_sta_mode) return;  // Se não estamos no modo STA, sair
	
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // Inicia a tentativa de conexão ao Wi-Fi
        ESP_LOGI(TAG, "Iniciando conexao a rede Wifi \"%s\"", wifi_sta_config.sta.ssid);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Limpa a flag de conexão e tenta reconectar ao Wi-Fi
        sta_connected = false;
        esp_wifi_connect();
        ESP_LOGI(TAG, "Tentando reconectar a rede WiFi \"%s\"", wifi_sta_config.sta.ssid);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // Define a flag de conexão quando um IP é obtido
        sta_connected = true;
        ESP_LOGI(TAG, "Conectado a rede Wifi \"%s\"", wifi_sta_config.sta.ssid);
    }
}

//Indica se esta conectado a um roteador
bool wifi_sta_connected(void) {
	return sta_connected;
}

//Indica se esta como ponto de acesso
bool wifi_ap_mode_active(void) {
    wifi_mode_t mode;
	if(esp_wifi_get_mode(&mode) != ESP_OK) return false;

    return mode == WIFI_MODE_AP;
}

esp_err_t wifi_get_mac(uint8_t *mac) {
    return esp_read_mac(mac, ESP_MAC_WIFI_STA);
}