#include "esp_log.h"
#include "esp_http_server.h"
#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_mac.h"

#include "web_server.h"
#include "app_config.h"

// Define a macro MIN se não estiver definida
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "WEB_SERVER";
httpd_handle_t webserver_handle = NULL;

// Manipulador para a rota raiz "/"
static esp_err_t root_get_handler(httpd_req_t *req) {
    // Obtém o endereço MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Get WiFi router credentials from NVS
    char router_ssid[MAX_SSID_LEN];
    char router_password[MAX_PASS_LEN];
    esp_err_t err_credentials = nvs_get_wifi_credentials(router_ssid, sizeof(router_ssid), router_password, sizeof(router_password));
    if(err_credentials != ESP_OK)
    {
        ESP_LOGE(TAG, "Fail to get Wifi Router Credentials from NVS");
    }
    router_ssid[MAX_SSID_LEN-1] = '\0';
    router_password[MAX_PASS_LEN-1] = '\0';

    char response[4000];
    
    snprintf(response, sizeof(response), "<!DOCTYPE html>"
       "<html>"
	       "<head>"
		       "<meta name='viewport' content='width=device-width, initial-scale=1'>"
		       "<style>"
			       "body { font-family: Arial, sans-serif; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; background-color: #f0f0f0; }"
			       ".container { text-align: center; background-color: #fff; padding: 20px; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1); border-radius: 8px; }"
			       "input[type='text'], input[type='password'] { width: 80%%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 4px; font-size: 16px; }"
			       "input[type='submit'] { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }"
			       "input[type='submit']:hover { background-color: #45a049; }"
		       "</style>"
	       "</head>"
	       "<body>"
		       "<div class='container'>"
                   "<h2>Device Info</h2>"
                   "<p>MAC Address: %02X%02X%02X%02X%02X%02X</p>"
                   "<br>"
			       "<h2>Configure WiFi Router</h2>"
			       "<form action='/set_wifi' method='post'>"
				       "<label for='ssid'>SSID:</label><br>"
				       "<input type='text' id='ssid' name='ssid' value='%s' required><br>"
				       "<label for='password'>Password:</label><br>"
				       "<input type='password' id='password' name='password' value='%s' required><br><br>"
				       "<input type='submit' value='Set WiFi'>"
			       "</form>"
		       "</div>"
	       "</body>"
       "</html>",
       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
       err_credentials == ESP_OK? router_ssid : "",
       err_credentials == ESP_OK? router_password : "");

    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
    
}

// Manipulador para a rota "/set_wifi"
static esp_err_t set_wifi_post_handler(httpd_req_t *req) {
    char buf[100];
    int ret, remaining = req->content_len;

    // Recebe o conteúdo da requisição
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }
    buf[req->content_len] = '\0';

    // Extrai SSID e senha do buffer recebido
    char *ssid = strtok(buf, "&");
    char *password = strtok(NULL, "&");
    if (ssid && password) {
        ssid = strtok(ssid, "=");
        ssid = strtok(NULL, "=");
        password = strtok(password, "=");
        password = strtok(NULL, "=");
        nvs_set_wifi_credentials(ssid, password);
        
	    const char *response = "<!DOCTYPE html>"
           "<html>"
	           "<head>"
		           "<meta name='viewport' content='width=device-width, initial-scale=1'>"
		           "<style>"
						"body { font-family: Arial, sans-serif; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; background-color: #f0f0f0; }"
						".container { text-align: center; background-color: #fff; padding: 20px; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1); border-radius: 8px; }"
		           "</style>"
	           "</head>"
	           "<body>"
		           "<div class='container'>"
			           "<h2>WiFi settings updated</h2>"
			           "<p>Restarting...</p>"
		           "</div>"
	           "</body>"
           "</html>";
        
        httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
        vTaskDelay(pdMS_TO_TICKS(1000)); //da tempo pra enviar o retorno http

        nvs_set_app_mode(APP_MODE_WIFI_MESH_NETWORK);
    } else {
        httpd_resp_send(req, "Invalid input", HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

// Definição da rota raiz
static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

// Definição da rota "/set_wifi"
static const httpd_uri_t set_wifi = {
    .uri       = "/set_wifi",
    .method    = HTTP_POST,
    .handler   = set_wifi_post_handler,
    .user_ctx  = NULL
};

// Inicialização do Wi-Fi em modo ponto de acesso (AP)
static void wifi_init_softap(void) {
    ESP_LOGI(TAG, "Iniciando WiFi como Ponto de Acesso");

    // Inicializa o armazenamento não volátil (NVS)
    nvs_flash_init();

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

// Inicia o servidor web
void start_webserver(bool init_wifi_ap) { 
    ESP_LOGI(TAG, "Iniciando WebServer");

    if(is_webserver_active()) {
        ESP_LOGW(TAG, "WebServer ja tinha sido iniciado");
        return;
    }

    if(init_wifi_ap) {
        wifi_init_softap();
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    //config.max_uri_handlers = 8;  // Ajusta o número máximo de manipuladores de URI
    //config.max_resp_headers = 16;  // Ajusta o número máximo de cabeçalhos de resposta
    config.stack_size = 8192;  // Aumenta o tamanho da pilha
    //config.recv_wait_timeout = 10;  // Ajusta o tempo de espera para receber (em segundos)
    //config.send_wait_timeout = 10;  // Ajusta o tempo de espera para enviar (em segundos)


    if (httpd_start(&webserver_handle, &config) == ESP_OK) {
        httpd_register_uri_handler(webserver_handle, &root);
        httpd_register_uri_handler(webserver_handle, &set_wifi);
        ESP_LOGI(TAG, "WebServer Iniciado.");
    } else {
        ESP_LOGE(TAG, "WebServer falhou.");
        webserver_handle = NULL;
    }
}

// Para o servidor web
void stop_webserver() {
    if (is_webserver_active()) {
        httpd_stop(webserver_handle);
        webserver_handle = NULL;
    }
}

bool is_webserver_active(void) {
    return webserver_handle != NULL;
}
