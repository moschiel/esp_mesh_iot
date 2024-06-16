#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "wifi.h"

// Pino do LED
#define LED_PIN 2

static const char *TAG = "main";

static void blink_led_task(void *arg);

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


// Tarefa para piscar o LED e imprimir mensagens no log
static void blink_led_task(void *arg) {
    bool led_state = false;
    while (true) {
        // Verifica se o Wi-Fi está conectado
        if (wifi_connected()) {
            // Pisca o LED a cada 1 segundo e imprime "Wifi Conectado" no log
            gpio_set_level(LED_PIN, led_state);
            led_state = !led_state;
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            // Pisca o LED 3 vezes por segundo e imprime "Nao foi possivel conectar" no log
            gpio_set_level(LED_PIN, led_state);
            led_state = !led_state;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}