#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

//custom libs
#include "wifi_config.h"
#include "web_server.h"
#include "utils.h"

// Pino do LED
#define LED_PIN 2
// Pino do Botao para trocar o modo do wifi entre ponto de acesso do wifi (AP) e estação (STA)
#define BUTTON_WIFI_MODE_PIN 5
#define HOLD_TIME_MS 5000

static const char *TAG = "MAIN_APP";
bool press_hold_timeout = false;

// Tarefa para piscar o LED
static void blink_led_task(void *arg) {
    bool led_state = false;
    while (true) {
        gpio_set_level(LED_PIN, led_state);

        if (press_hold_timeout){
			led_state = !led_state;
            // Pisca o LED 5 vezes por segundo indicando ao usuario que ja pode soltar o botao
            vTaskDelay(pdMS_TO_TICKS(200));
        } else if(wifi_ap_mode_active()) {
			led_state = !led_state;
            // Pisca o LED a cada 1 segundo se estiver no modo AP
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else if (is_wifi_sta_connected()) {
            // Mantem o LED ligado se estiver no modo STA
            led_state = true;
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
			led_state = !led_state;
            // Pisca o LED 10 vezes por segundo indicando que esta desconectado o wifi
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// Tarefa para verificar botoes
static void check_button_task(void *arg) {
    static uint32_t press_start_time = 0;

    while (true) {		

        //Verifica botao para trocar o modo do wifi entre ponto de acesso (AP) e estação (STA)
        if (gpio_get_level(BUTTON_WIFI_MODE_PIN) == 0) { //se pressionado
            if(press_hold_timeout == false) {
                if (press_start_time == 0) {
                    press_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                } else if ((xTaskGetTickCount() * portTICK_PERIOD_MS - press_start_time) >= HOLD_TIME_MS) {
                    //Ja segurou o botao por tempo suficiente,
                    //a task blink_led_task vai fazer o led piscar 10x por segundo, indicando que ja podemos soltar o botao.
                    press_hold_timeout = true;
                }
            }
        } else {
            press_start_time = 0;

            if(press_hold_timeout) {
                //Solicitado troca do modo WiFi
                press_hold_timeout = false;
                if(nvs_wifi_get_mode() == WIFI_MODE_AP)
                    nvs_wifi_set_mode(WIFI_MODE_STA);
                else 
                    nvs_wifi_set_mode(WIFI_MODE_AP);
                
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void init_IOs() {
    // Configura o pino do LED como saída
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    // Configura o pino do botão como entrada
    gpio_set_direction(BUTTON_WIFI_MODE_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_WIFI_MODE_PIN, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(BUTTON_WIFI_MODE_PIN, GPIO_INTR_ANYEDGE);
}

// Função principal do aplicativo
void app_main(void) {
	ESP_LOGI(TAG, "App Version: 3");
	
	print_chip_info();
	
    //nvs_flash_erase();

    init_NVS();

    init_IOs();

    if(nvs_wifi_get_mode() == WIFI_MODE_STA) {
        // Tenta Inicializar o Wi-Fi em modo estação
        wifi_init_sta();
    }
    else { 
        // Inicializa WiFi como Ponto de Acesso e sobe Webserver
        wifi_init_softap();
        start_webserver();
    }

    // Cria a tarefa para piscar o LED
    xTaskCreate(blink_led_task, "blink_led_task", 2048, NULL, 5, NULL);
    // Cria a tarefa para checar botoes
    xTaskCreate(check_button_task, "check_button_task", 4096, NULL, 5, NULL);
}