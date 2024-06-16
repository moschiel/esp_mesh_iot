#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"


// Pino do LED
#define LED_PIN 2

// Criação de um grupo de eventos para gerenciar o estado do Wi-Fi
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static const char *TAG = "wifi_station";

// Tarefa para piscar o LED e imprimir mensagens no log
static void blink_led_task(void *arg) {
    bool led_state = false;
    while (true) {
        // Verifica se o Wi-Fi está conectado
        if (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) {
            // Pisca o LED a cada 1 segundo e imprime "Wifi Conectado" no log
            gpio_set_level(LED_PIN, led_state);
            led_state = !led_state;
            ESP_LOGI(TAG, "Wifi Conectado");
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            // Pisca o LED 3 vezes por segundo e imprime "Nao foi possivel conectar" no log
            gpio_set_level(LED_PIN, led_state);
            led_state = !led_state;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// Manipulador de eventos para Wi-Fi
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // Inicia a tentativa de conexão ao Wi-Fi
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Limpa o bit de conexão e tenta reconectar ao Wi-Fi
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        esp_wifi_connect();
        ESP_LOGI(TAG, "Tentando reconectar ao WiFi %s", CONFIG_ROUTER_WIFI_SSID);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // Define o bit de conexão quando um IP é obtido
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}

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

// Função principal do aplicativo
void app_main(void) {
    // Inicializa o armazenamento não volátil (NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Configura o pino do LED como saída
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    // Inicializa o Wi-Fi em modo estação
    wifi_init_sta();

    // Cria a tarefa para piscar o LED e imprimir mensagens no log
    xTaskCreate(blink_led_task, "blink_led_task", 2048, NULL, 5, NULL);
}
