#include "web_server.h"
#include "wifi_config.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <string.h>

// Define a macro MIN se não estiver definida
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "WEB_SERVER";
httpd_handle_t webserver_handle = NULL;

// Manipulador para a rota raiz "/"
static esp_err_t root_get_handler(httpd_req_t *req) {
    const char *response = "<!DOCTYPE html>"
       "<html>"
	       "<head>"
		       "<meta name='viewport' content='width=device-width, initial-scale=1'>"
		       "<style>"
			       "body { font-family: Arial, sans-serif; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; background-color: #f0f0f0; }"
			       ".container { text-align: center; background-color: #fff; padding: 20px; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1); border-radius: 8px; }"
			       "input[type='text'], input[type='password'] { width: 80%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 4px; font-size: 16px; }"
			       "input[type='submit'] { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }"
			       "input[type='submit']:hover { background-color: #45a049; }"
		       "</style>"
	       "</head>"
	       "<body>"
		       "<div class='container'>"
			       "<h2>Configure WiFi</h2>"
			       "<form action='/set_wifi' method='post'>"
				       "<label for='ssid'>SSID:</label><br>"
				       "<input type='text' id='ssid' name='ssid' required><br>"
				       "<label for='password'>Password:</label><br>"
				       "<input type='password' id='password' name='password' required><br><br>"
				       "<input type='submit' value='Set WiFi'>"
			       "</form>"
		       "</div>"
	       "</body>"
       "</html>";
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
        nvs_wifi_set_credentials(ssid, password);
        
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

        nvs_wifi_set_mode(WIFI_MODE_STA);
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

// Inicia o servidor web
void start_webserver(void) {
    stop_webserver();
    
    ESP_LOGI(TAG, "Iniciando WebServer");
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    //config.max_uri_handlers = 8;  // Ajusta o número máximo de manipuladores de URI
    //config.max_resp_headers = 16;  // Ajusta o número máximo de cabeçalhos de resposta
    //config.stack_size = 8192;  // Aumenta o tamanho da pilha
    //config.recv_wait_timeout = 10;  // Ajusta o tempo de espera para receber (em segundos)
    //config.send_wait_timeout = 10;  // Ajusta o tempo de espera para enviar (em segundos)


    if (httpd_start(&webserver_handle, &config) == ESP_OK) {
        httpd_register_uri_handler(webserver_handle, &root);
        httpd_register_uri_handler(webserver_handle, &set_wifi);
    }
}

// Para o servidor web
void stop_webserver() {
    if (webserver_handle) {
        httpd_stop(webserver_handle);
        webserver_handle = NULL;
    }
}
