#include "esp_log.h"
#include "esp_http_server.h"
#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "cJSON.h"

#include "web_server.h"
#include "app_config.h"
#include "utils.h"
#include "mesh_network.h"
#include "html_builder.h"
#include "mesh_tree.h"
#include "ota.h"

// Define a macro MIN se não estiver definida
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "WEB_SERVER";
static httpd_handle_t webserver_handle = NULL;

static void send_ws_frame_str(httpd_req_t *req, char *payload);
static esp_err_t receive_ws_frame_str(httpd_req_t *req, char *payload);

extern uint8_t STA_MAC_address[6];

// Manipulador para a rota raiz "/"
static esp_err_t root_get_handler(httpd_req_t *req) {
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

    //Obtem ip info
    esp_netif_ip_info_t ip_info;
    get_mesh_root_ip(&ip_info);

    send_root_html(
        req, 
        STA_MAC_address,
        err_credentials == ESP_OK? router_ssid : "",
        err_credentials == ESP_OK? router_password : "",
        mesh_id,
        mesh_password,
        is_mesh_parent_connected(),
        ip4addr_ntoa((const ip4_addr_t*)&(ip_info.ip)));

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

        send_set_wifi_html(req, true);
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Da tempo pra enviar o retorno HTTP antes de mudar para o modo MESH e resetar a ESP
        nvs_set_app_mode(APP_MODE_WIFI_MESH_NETWORK);
    } else {
        send_set_wifi_html(req, false);
    }

    free(buf);
    return ESP_OK;
}

// Manipulador para a rota "/get_mesh_data"
static esp_err_t mesh_data_get_handler(httpd_req_t *req)
{
    // Obtém JSON da arvore de nós da da rede mesh
    char* mesh_json_str = build_mesh_tree_json();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, mesh_json_str, strlen(mesh_json_str));

    if(mesh_json_str != NULL) free(mesh_json_str);

    return ESP_OK;
}

// Manipulador para a rota "/mesh_tree_table"
static esp_err_t mesh_tree_get_handler(httpd_req_t *req) {
    // Obtém JSON da arvore de nós da da rede mesh
    char* mesh_json_str = build_mesh_tree_json();
    
    // Enviar a árvore de rede mesh em HTML
    send_mesh_tree_html(req, mesh_json_str);

    if(mesh_json_str != NULL) free(mesh_json_str);

    return ESP_OK;
}

// Manipulador para rota "ws:/update" para OTA
static const esp_err_t websocket_update_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "ENTER WEBSOCKET");
    if (req->method == HTTP_GET) {
        return ESP_OK;
    }

    char buf[256];
    esp_err_t ret = receive_ws_frame_str(req, buf);
    if(ret != ESP_OK) {
        return ret;
    }

    // Parse the received JSON
    cJSON *root = cJSON_Parse(buf);
    cJSON *url = cJSON_GetObjectItem(root, "url");

    if (url) {
        // Send websocket message to indicate the update has started
        send_ws_frame_str(req, "{\"status\": \"Update Started\"}");

        if (execute_ota(url->valuestring)) {
            ESP_LOGI(TAG, "Rebooting...");
            // Send websocket message to indicate the update is completed
            send_ws_frame_str(req, "{\"status\": \"Update Complete, ESP restarted\"}");
            // Da tempo pra enviar o retorno antes de resetar a ESP
            vTaskDelay(pdMS_TO_TICKS(1000)); 
            esp_restart();
        } else {
            send_ws_frame_str(req, "{\"status\": \"Update Failed\"}");
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}


// Definição da rota raiz
static const httpd_uri_t root_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

// Definição da rota "/set_wifi"
static const httpd_uri_t set_wifi_uri = {
    .uri       = "/set_wifi",
    .method    = HTTP_POST,
    .handler   = set_wifi_post_handler,
    .user_ctx  = NULL
};

// Definição da rota "/get_mesh_data"
static const httpd_uri_t mesh_data_uri = {
    .uri       = "/get_mesh_data",
    .method    = HTTP_GET,
    .handler   = mesh_data_get_handler,
    .user_ctx  = NULL
};

// Definição da rota "/mesh_tree" para plotar a árvore da rede mesh
static const httpd_uri_t mesh_tree_uri = {
    .uri       = "/mesh_tree",
    .method    = HTTP_GET,
    .handler   = mesh_tree_get_handler,
    .user_ctx  = NULL
};

// Definição da rota WebSocket "/update" para acompanhar status do OTA
static const httpd_uri_t ws_update_status_uri = {
    .uri       = "/update",
    .method    = HTTP_GET,
    .handler   = websocket_update_handler
};

static void send_ws_frame_str(httpd_req_t *req, char *payload)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t*)payload;
    ws_pkt.len = strlen(payload);
    httpd_ws_send_frame(req, &ws_pkt);
}

static esp_err_t receive_ws_frame_str(httpd_req_t *req, char *payload)
{
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t *)payload;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(payload));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        return ret;
    }
    payload[ws_pkt.len] = '\0';
    return ESP_OK;
}

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

    // Cria o SSID único
    char unique_ssid[32];
    snprintf(unique_ssid, sizeof(unique_ssid), "ESP32_Config_%02X%02X%02X", STA_MAC_address[3], STA_MAC_address[4], STA_MAC_address[5]);

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
        httpd_register_uri_handler(webserver_handle, &root_uri);
        httpd_register_uri_handler(webserver_handle, &set_wifi_uri);
        httpd_register_uri_handler(webserver_handle, &mesh_data_uri);
        httpd_register_uri_handler(webserver_handle, &mesh_tree_uri);
        httpd_register_uri_handler(webserver_handle, &ws_update_status_uri);
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
