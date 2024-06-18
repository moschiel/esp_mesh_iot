#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "wifi.h"
#include "web_server.h"

//for printing chip info
#include <stdio.h>
#include <inttypes.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

// Pino do LED
#define LED_PIN 2
// Pino do Botao para iniciar o modo ponto de acesso do wifi (AP)
#define BUTTON_AP_MODE_PIN 5
#define HOLD_TIME_MS 5000

static const char *TAG = "main";
static bool ap_mode = false;

void print_chip_info(void) {
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());
}

// Tarefa para piscar o LED
static void blink_led_task(void *arg) {
    bool led_state = false;
    while (true) {
        gpio_set_level(LED_PIN, led_state);

        if(ap_mode) {
			led_state = !led_state;
            // Pisca o LED a cada 1 segundo
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else if (wifi_sta_connected()) {
            led_state = true;
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
			led_state = !led_state;
            // Pisca o LED 10 vezes por segundo
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// Tarefa para verificar se o botão que habilita o WiFi como ponto de acesso foi pressionado por 5 segundos
static void check_button_ap_mode_task(void *arg) {
    uint32_t press_start_time = 0;

    while (true) {
		if(ap_mode == false) {			
	        if (gpio_get_level(BUTTON_AP_MODE_PIN) == 0) { //se pressionado
	            if (press_start_time == 0) {
	                press_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
	            } else if ((xTaskGetTickCount() * portTICK_PERIOD_MS - press_start_time) >= HOLD_TIME_MS) {
				    // Inicia o modo AP e WebServer
	                ap_mode = true;
				    wifi_init_softap();
				    start_webserver();
	                break;
	            }
	        } else {
	            press_start_time = 0;
	        }
		}

        vTaskDelay(pdMS_TO_TICKS(100));
    }

}

// Função principal do aplicativo
void app_main(void) {
	ESP_LOGI(TAG, "firmware version: 1");
	
	print_chip_info();
	
    // Inicializa o armazenamento não volátil (NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Configura o pino do LED como saída
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    // Configura o pino do botão como entrada
    gpio_set_direction(BUTTON_AP_MODE_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_AP_MODE_PIN, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(BUTTON_AP_MODE_PIN, GPIO_INTR_ANYEDGE);

    // Tenta Inicializar o Wi-Fi em modo estação
    wifi_init_sta();

    // Cria a tarefa para piscar o LED
    xTaskCreate(blink_led_task, "blink_led_task", 512, NULL, 5, NULL);
    // Cria a tarefa para checar se o botao que ativa o modo AP
    xTaskCreate(check_button_ap_mode_task, "check_button_ap_mode_task", 4096, NULL, 5, NULL);
}