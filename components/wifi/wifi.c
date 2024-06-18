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

#define WIFI_NAMESPACE "wifi_config"  // Namespace para armazenar as configurações Wi-Fi (STA) na NVS
#define WIFI_SSID_KEY "ssid"  // Chave para o SSID na NVS
#define WIFI_PASS_KEY "password"  // Chave para a senha na NVS
#define MAX_SSID_LEN 32  // Comprimento máximo do SSID
#define MAX_PASS_LEN 64  // Comprimento máximo da senha

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// Criação de um grupo de eventos para gerenciar o estado do Wi-Fi
static EventGroupHandle_t wifi_event_group;
// Bit para indicar que o Wi-Fi está conectado
const int CONNECTED_BIT = BIT0; 
static const char *TAG = "wifi";
static bool is_sta_mode = true;  // Flag para indicar o modo STA
static esp_netif_t *sta_netif = NULL;  // netif para o modo STA

// Configurações do Wi-Fi STA
wifi_config_t wifi_sta_config = {
    .sta = {
        .ssid = "",
        .password = "",
    }
};

// Inicialização do Wi-Fi em modo estação (STA / cliente)
bool wifi_init_sta(void) {
	is_sta_mode = true;
	
	ESP_LOGI(TAG, "Iniciando WiFi como estacao");
    char ssid[MAX_SSID_LEN];
    char password[MAX_PASS_LEN];

    // Criação do grupo de eventos
    if (wifi_event_group == NULL) {
    	wifi_event_group = xEventGroupCreate();
	}
	
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
    esp_err_t err = wifi_get_credentials(ssid, sizeof(ssid), password, sizeof(password));
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "Fail to get Wifi Credentials from NVS");
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

// Inicialização do Wi-Fi em modo ponto de acesso (AP)
void wifi_init_softap(void) {
	wifi_stop_sta();
	
	ESP_LOGI(TAG, "Iniciando WiFi como Ponto de Acesso");
    //wifi_event_group = xEventGroupCreate(); //nao queremos monitrar eventos em AP, porque? porque nao.
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = "ESP32_Config",
            .ssid_len = strlen("ESP32_Config"),
            .password = "esp32config",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config);
    esp_wifi_start();
}

// Para o Wi-Fi em modo estação (cliente)
void wifi_stop_sta(void) {
    is_sta_mode = false;
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_netif_destroy(sta_netif);  // Desfaz a inicialização do netif STA
}

//Salva na NVS o ssid e password do roteador WiFi
esp_err_t wifi_set_credentials(const char *ssid, const char *password) {
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
esp_err_t wifi_get_credentials(char *ssid, size_t ssid_len, char *password, size_t password_len) {
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

// Manipulador de eventos para Wi-Fi
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	if (!is_sta_mode) return;  // Se não estamos no modo STA, sair
	
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // Inicia a tentativa de conexão ao Wi-Fi
        ESP_LOGI(TAG, "Iniciando conexão a rede Wifi \"%s\"", wifi_sta_config.sta.ssid);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Limpa o bit de conexão e tenta reconectar ao Wi-Fi
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        esp_wifi_connect();
        ESP_LOGI(TAG, "Tentando reconectar a rede WiFi \"%s\"", wifi_sta_config.sta.ssid);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // Define o bit de conexão quando um IP é obtido
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        ESP_LOGI(TAG, "Conectado a rede Wifi \"%s\"", wifi_sta_config.sta.ssid);
    }
}

//Indica se esta conectado a um roteador
bool wifi_sta_connected(void) {
	return xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT;
}
