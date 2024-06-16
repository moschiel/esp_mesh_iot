#include "wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "sdkconfig.h"


static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// Criação de um grupo de eventos para gerenciar o estado do Wi-Fi
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static const char *TAG = "wifi";

// Inicialização do Wi-Fi em modo estação
void wifi_init_sta(void) {
    // Criação do grupo de eventos
    wifi_event_group = xEventGroupCreate();

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
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip);

    // Configurações do Wi-Fi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ROUTER_WIFI_SSID,
            .password = CONFIG_ROUTER_WIFI_PASS,
        },
    };
    // Define o modo de operação do Wi-Fi e aplica as configurações
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    // Inicia o Wi-Fi
    esp_wifi_start();
}

// Manipulador de eventos para Wi-Fi
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // Inicia a tentativa de conexão ao Wi-Fi
        ESP_LOGI(TAG, "Iniciando conexão a rede Wifi %s", CONFIG_ROUTER_WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Limpa o bit de conexão e tenta reconectar ao Wi-Fi
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        esp_wifi_connect();
        ESP_LOGI(TAG, "Tentando reconectar ao WiFi %s", CONFIG_ROUTER_WIFI_SSID);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // Define o bit de conexão quando um IP é obtido
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        ESP_LOGI(TAG, "Conectado a rede Wifi %s", CONFIG_ROUTER_WIFI_SSID);
    }
}

bool wifi_connected(void){
	return xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT;
}
