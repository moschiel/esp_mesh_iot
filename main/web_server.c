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
#include "utils.h"
#include "mesh_network.h"

// Define a macro MIN se não estiver definida
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "WEB_SERVER";
static httpd_handle_t webserver_handle = NULL;

// Manipulador para a rota raiz "/"
static esp_err_t root_get_handler(httpd_req_t *req) {
    // Obtém o endereço MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // Get WiFi router credentials from NVS
    char router_ssid[MAX_SSID_LEN];
    char router_password[MAX_PASS_LEN];
    esp_err_t err_credentials = nvs_get_wifi_credentials(router_ssid, sizeof(router_ssid), router_password, sizeof(router_password));
    router_ssid[MAX_SSID_LEN-1] = '\0';
    router_password[MAX_PASS_LEN-1] = '\0';

    // Obtem as credenciais da rede mesh da NVS
    uint8_t mesh_id[6];
    char mesh_password[MAX_PASS_LEN];
    nvs_get_mesh_credentials(mesh_id, mesh_password, sizeof(mesh_password));
    
    char response[3000];
    
    strcpy(response, 
    "<!DOCTYPE html>"
       "<html>"
	       "<head>"
		       "<meta name='viewport' content='width=device-width, initial-scale=1'>"
		       "<style>"
			       "body { font-family: Arial, sans-serif; margin: auto; padding: 0; width: 400px; max-width: 90%; justify-content: center; align-items: center; height: 100vh; background-color: #f0f0f0; }"
			       ".container { text-align: center; background-color: #fff; padding: 20px; width: auto; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.1); border-radius: 8px; }"
			       "input[type='text'], input[type='password'] { width: 80%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 4px; font-size: 16px; }"
			       "input[type='submit'] { background-color: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; }"
			       "input[type='submit']:hover { background-color: #45a049; }"
		       "</style>"
                "<script>"
                    "function formatMeshID(e) {"
                    "  var r = e.target.value.replace(/[^a-fA-F0-9]/g, '');"
                    "  e.target.value = r.match(/.{1,2}/g).join('-');"
                    "}"
                    "function togglePassword(id) {"
                    "  var x = document.getElementById(id);"
                    "  if (x.type === 'password') {"
                    "    x.type = 'text';"
                    "  } else {"
                    "    x.type = 'password';"
                    "  }"
                    "}"
                "</script>"
	       "</head>"
	       "<body>");
    httpd_resp_send_chunk(req, response, HTTPD_RESP_USE_STRLEN);

    snprintf(response, sizeof(response),
               "<br/>"
		       "<div class='container'>"
                   "<h2>Device Info</h2>"
                   "<p>MAC address: "MACSTR"</p>"
		       "</div>"
               "<br/>"
		       "<div class='container'>"
			       "<form action='/set_wifi' method='post'>"
                        "<h2>Configure WiFi Router</h2>"
                        "<label for='router_ssid'>Router SSID:</label><br>"
                        "<input type='text' id='router_ssid' name='router_ssid' value='%s' required><br>"
                        "<label for='router_password'>Router Password:</label><br>"
                        "<input type='password' id='router_password' name='router_password' value='%s' required><br/>"
                        "<input type='checkbox' onclick='togglePassword(\"router_password\")'> Show Password<br/>"
                        "<br/>"
                        "<h2>Configure Mesh Network</h2>"
                        "<label for='mesh_id'>Mesh ID:</label><br>"
                        "<input type='text' id='mesh_id' name='mesh_id' value='%02X-%02X-%02X-%02X-%02X-%02X' required oninput='formatMeshID(event)' maxlength='17'><br>" //NAO PODE SEPARADOR ':', FORM CODIFICA DIFERENTE AO FAZER POST
                        "<label for='mesh_password'>Mesh Password:</label><br>"
                        "<input type='password' id='mesh_password' name='mesh_password' value='%s' required><br/>"
                        "<input type='checkbox' onclick='togglePassword(\"mesh_password\")'> Show Password<br/>"
                        "<br/>"
                        "<input type='submit' value='Update Config'>"
			       "</form>"
		       "</div>",
                MAC2STR(mac),
                err_credentials == ESP_OK? router_ssid : "",
                err_credentials == ESP_OK? router_password : "",
                mesh_id[0],mesh_id[1],mesh_id[2],mesh_id[3],mesh_id[4],mesh_id[5],
                mesh_password);
    httpd_resp_send_chunk(req, response, HTTPD_RESP_USE_STRLEN);

    if(nvs_get_app_mode() == APP_MODE_WIFI_MESH_NETWORK && is_mesh_parent_connected()) {
        strcpy(response, 
                "<br/>"
                "<div class='container'>"
                    "<h2>Nodes in Mesh Network</h2>"
                    "<ul>");
        httpd_resp_send_chunk(req, response, HTTPD_RESP_USE_STRLEN);

        // Obtém a tabela de roteamento da rede mesh
        mesh_addr_t routing_table[CONFIG_MESH_ROUTE_TABLE_SIZE];
        int routing_table_size = 0;
        esp_mesh_get_routing_table((mesh_addr_t *)&routing_table, CONFIG_MESH_ROUTE_TABLE_SIZE * 6, &routing_table_size);
        for (int i = 0; i < routing_table_size; i++) {
            snprintf(response, sizeof(response),
                        "<li>"MACSTR"</li>", 
                        MAC2STR(routing_table[i].addr));            
            httpd_resp_send_chunk(req, response, HTTPD_RESP_USE_STRLEN);
        }
        
        strcpy(response,
                    "</ul>"
                "</div>"
                "<br/>");
        httpd_resp_send_chunk(req, response, HTTPD_RESP_USE_STRLEN);
    }

    snprintf(response, sizeof(response), 
            "</body>"
        "</html>");
    httpd_resp_send_chunk(req, response, HTTPD_RESP_USE_STRLEN);

    //When you are finished sending all your chunks, you must call this function with buf_len as 0.
    httpd_resp_send_chunk(req, response, 0);

    return ESP_OK;
    
}

// Manipulador para a rota "/set_wifi"
static esp_err_t set_wifi_post_handler(httpd_req_t *req) {
    // Calcula o tamanho necessário do buffer
    size_t buf_len = req->content_len + 1;
    char *buf = (char *)malloc(buf_len);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Falha ao alocar memória para o buffer");
        return ESP_FAIL;
    }

    int ret, remaining = req->content_len;
    size_t received = 0;

    // Recebe o conteúdo da requisição
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf + received, MIN(remaining, 64))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            free(buf);
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    buf[received] = '\0';


    // Extrai SSID e senha do buffer recebido usando sscanf
    char router_ssid[MAX_SSID_LEN] = {0};
    char router_password[MAX_PASS_LEN] = {0};
    char mesh_id_str[6 + 5*3] = {0};  // Buffer para a string do mesh_id no formato MAC, na url o caracter ':' eh convertido para string de 3 caracters "%3A"
    char mesh_password[MAX_PASS_LEN] = {0};
    sscanf(buf, "router_ssid=%[^&]&router_password=%[^&]&mesh_id=%[^&]&mesh_password=%s", router_ssid, router_password, mesh_id_str, mesh_password);

    if (strlen(router_ssid) > 0 && strlen(router_password) > 0 && strlen(mesh_id_str) > 0 && strlen(mesh_password) > 0) {
        uint8_t mesh_id[6] = {0};
        mac_str_to_bytes(mesh_id_str, mesh_id);

        nvs_set_wifi_credentials(router_ssid, router_password);
        nvs_set_mesh_credentials(mesh_id, mesh_password);

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
        vTaskDelay(pdMS_TO_TICKS(1000)); // Da tempo pra enviar o retorno HTTP

        nvs_set_app_mode(APP_MODE_WIFI_MESH_NETWORK);
    } else {
        httpd_resp_send(req, "Invalid input", HTTPD_RESP_USE_STRLEN);
    }

    free(buf);
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
        ESP_LOGI(TAG, "WebServer Iniciado com sucesso.");
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
        ESP_LOGW(TAG, "WebServer foi parado.");
    }
}

bool is_webserver_active(void) {
    return webserver_handle != NULL;
}
