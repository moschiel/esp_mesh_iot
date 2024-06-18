#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "wifi.h"

//for printing chip info
#include <stdio.h>
#include <inttypes.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

// Pino do LED
#define LED_PIN 2
// Pino do Botao para trocar o modo do wifi entre ponto de acesso do wifi (AP) e estação (STA)
#define BUTTON_WIFI_MODE_PIN 5
#define HOLD_TIME_MS 5000

static const char *TAG = "MAIN";

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

        if(wifi_ap_mode_active()) {
			led_state = !led_state;
            // Pisca o LED a cada 1 segundo se estiver no modo AP
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else if (wifi_sta_connected()) {
            // Mantem o LED ligado se estiver no modo STA
            led_state = true;
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
			led_state = !led_state;
            // Pisca o LED 10 vezes por segundo se estiver desconectado
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// Tarefa para verificar botoes
static void check_button_task(void *arg) {
    static uint32_t press_start_time = 0;
    static bool reach_hold_time = false;

    while (true) {		

        //Verifica botao para trocar o modo do wifi entre ponto de acesso (AP) e estação (STA)
        if (gpio_get_level(BUTTON_WIFI_MODE_PIN) == 0) { //se pressionado
            if(reach_hold_time == false) {
                if (press_start_time == 0) {
                    press_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                } else if ((xTaskGetTickCount() * portTICK_PERIOD_MS - press_start_time) >= HOLD_TIME_MS) {
                    //Ja segurou o botao por tempo suficiente,
                    //para o wifi, assim a task blink_led_task vai fazer o led piscar rapido, indicando que ja podemos soltar o botao.
                    wifi_stop(); 
                    reach_hold_time = true;
                }
            }
        } else {
            press_start_time = 0;

            if(reach_hold_time) {
                //Solicitado troca do modo WiFi
                reach_hold_time = false;
                if(nvs_wifi_get_mode() == WIFI_MODE_AP)
                    nsv_wifi_set_mode(WIFI_MODE_STA, false);
                else 
                    nsv_wifi_set_mode(WIFI_MODE_AP, false);
                
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Função principal do aplicativo
void app_main(void) {
	ESP_LOGI(TAG, "app version: 2");
	
	print_chip_info();
	
    //nvs_flash_erase();
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
    gpio_set_direction(BUTTON_WIFI_MODE_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_WIFI_MODE_PIN, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(BUTTON_WIFI_MODE_PIN, GPIO_INTR_ANYEDGE);

    if(nvs_wifi_get_mode() == WIFI_MODE_STA) {
        // Tenta Inicializar o Wi-Fi em modo estação
        wifi_init_sta();
    }
    else { 
        // Inicializa WiFi como Ponto de Acesso e sobe Webserver
        wifi_init_softap();
    }

    // Cria a tarefa para piscar o LED
    xTaskCreate(blink_led_task, "blink_led_task", 2048, NULL, 5, NULL);
    // Cria a tarefa para checar botoes
    xTaskCreate(check_button_task, "check_button_task", 4096, NULL, 5, NULL);
}