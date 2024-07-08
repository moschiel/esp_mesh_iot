#include "esp_log.h"
#include "esp_http_server.h"
#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "cJSON.h"
#include "esp_spiffs.h"

#include "web_server.h"
#include "app_config.h"
#include "utils.h"
#include "mesh_network.h"
#include "mesh_tree.h"
#include "ota.h"
#include "messages.h"

// Define a macro MIN se não estiver definida
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif


static const char *TAG = "WEB_SERVER";
static httpd_handle_t webserver_handle = NULL;

extern uint8_t STA_MAC_address[6];

static void wifi_init_softap(void);
static void init_spiffs(void);

// Manipulador para a rota raiz "/"
static esp_err_t root_get_handler(httpd_req_t *req) {

    FILE* f = fopen("/spiffs/index.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char chunk[1024];
    while (fgets(chunk, sizeof(chunk), f) != NULL) {
        httpd_resp_send_str_chunk(req, chunk);
    }
    fclose(f);

    httpd_resp_send_chunk(req, NULL, 0); // Terminate the response
    return ESP_OK;
}

static esp_err_t root_get_configs_handler(httpd_req_t *req) {
    // Get WiFi router credentials from NVS
    char router_ssid[MAX_SSID_LEN];
    char router_password[MAX_PASS_LEN];
    nvs_get_wifi_credentials(router_ssid, sizeof(router_ssid), router_password, sizeof(router_password));
    router_ssid[MAX_SSID_LEN-1] = '\0';
    router_password[MAX_PASS_LEN-1] = '\0';

    // Obtem as credenciais da rede mesh da NVS
    uint8_t mesh_id[6];
    char mesh_password[MAX_PASS_LEN];
    nvs_get_mesh_credentials(mesh_id, mesh_password, sizeof(mesh_password));

    //Obtem ultimo fw url
    char fw_url[100] = {0};
    nvs_get_ota_fw_url(fw_url, sizeof(fw_url));

    char response[500];
    sprintf(response, 
        "{"
            "\"device_mac_addr\":\""MACSTR"\","
            "\"router_ssid\":\"%s\","
            "\"router_password\":\"%s\","
            "\"mesh_id\":\""MESHSTR"\"," 
            "\"mesh_password\":\"%s\","
            "\"is_connected\":%s,"
            "\"fw_url\":\"%s\""
        "}",
        MAC2STR(STA_MAC_address),
        router_ssid,
        router_password,
        MESH2STR(mesh_id),
        mesh_password,
        is_mesh_parent_connected()? "true":"false",
        fw_url
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send_str_chunk(req, response);
    httpd_resp_send_chunk(req, NULL, 0); // Terminate the response
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

    ESP_LOGI(TAG, "Received /set_wifi: %s", buf);

    // Extrai SSID e senha do buffer recebido usando sscanf
    char router_ssid[MAX_SSID_LEN] = {0};
    char router_password[MAX_PASS_LEN] = {0};
    char mesh_id_str[6*3] = {0};  // Buffer para a string do mesh_id no formato MAC
    char mesh_password[MAX_PASS_LEN] = {0};
    sscanf(buf, "router_ssid=%[^&]&router_password=%[^&]&mesh_id=%[^&]&mesh_password=%s", router_ssid, router_password, mesh_id_str, mesh_password);

    if (strlen(router_ssid) > 0 && strlen(router_password) > 0 && strlen(mesh_id_str) > 0 && strlen(mesh_password) > 0) {
        uint8_t mesh_id[6] = {0};
        mac_str_to_bytes(mesh_id_str, mesh_id);

        nvs_set_wifi_credentials(router_ssid, router_password);
        nvs_set_mesh_credentials(mesh_id, mesh_password);

        // Send HTTP response with 200 OK
        httpd_resp_set_status(req, "200 OK");
        httpd_resp_send(req, NULL, 0);
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Da tempo pra enviar o retorno HTTP antes de mudar para o modo MESH e resetar a ESP
        nvs_set_app_mode(APP_MODE_WIFI_MESH_NETWORK);
    } else {
        //400 Bad Request
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, NULL, 0);
    }

    free(buf);
    return ESP_OK;
}

// Manipulador para a rota "/mesh_list_data" que retorna a lista de nodes
static esp_err_t mesh_list_data_handler(httpd_req_t *req)
{
    // Obtém JSON da arvore de nós da da rede mesh
    char* mesh_json_str = build_mesh_list_json();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, mesh_json_str, strlen(mesh_json_str));
    httpd_resp_send_chunk(req, NULL, 0); // Terminate the response

    if(mesh_json_str != NULL) free(mesh_json_str);

    return ESP_OK;
}

// Manipulador para a rota "/mesh_tree_view" para carregar a vizualizacao da arvore
static esp_err_t mesh_tree_view_handler(httpd_req_t *req) {
     FILE* f = fopen("/spiffs/mesh-graph.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char chunk[1024];
    while (fgets(chunk, sizeof(chunk), f) != NULL) {
        httpd_resp_send_str_chunk(req, chunk);
    }
    fclose(f);

    httpd_resp_send_chunk(req, NULL, 0); // Terminate the response
    return ESP_OK;
}

// Manipulador para a rota "/mesh_tree_data" que retorna o json da arvore
static esp_err_t mesh_tree_data_handler(httpd_req_t *req)
{
    // Obtém JSON da arvore de nós da da rede mesh
    char* mesh_json_str = build_mesh_tree_json();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, mesh_json_str, strlen(mesh_json_str));
    httpd_resp_send_chunk(req, NULL, 0); // Terminate the response

    if(mesh_json_str != NULL) free(mesh_json_str);

    return ESP_OK;
}

// Manipulador para rota "ws_update" para OTA
#if BLA
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
#endif
static esp_err_t ws_update_fw_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Websocket handshake");
        // send_ws_frame_str(req, "{\"ota_msg\": \"Hello from ESP32\"}");
    } else {
        //se nao chegar GET, é mensagem
        char payload[500] = {0};
        if(receive_ws_frame_str(req, payload, sizeof(payload)) == ESP_OK) {
            ESP_LOGI(TAG, "Rcv Socket Method: %i, Data: %s", req->method, payload);

            char fw_url[100];
            if(rx_msg_fw_update_request(payload, fw_url)) {
                nvs_set_ota_fw_url(fw_url);
                start_ota(fw_url, req);
            } else {
                send_ws_ota_status(req, "Fail to parse JSON", true);
            }
        }
    }
    /*else if (req->method == HTTP_POST) {
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
        ws_pkt.payload = malloc(128);
        ws_pkt.len = httpd_req_recv(req, (char*)ws_pkt.payload, ws_pkt.len);
        if (ws_pkt.len <= 0) {
            if (ws_pkt.len == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_ws_send_frame(req, &ws_pkt);
            }
            free(ws_pkt.payload);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Received packet with message: %s", ws_pkt.payload);
        free(ws_pkt.payload);
    }*/
    return ESP_OK;
}

// Definição da rota raiz
static const httpd_uri_t root_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

// Definição da rota "/get_configs" para requisitar configs
static const httpd_uri_t get_configs_uri = {
    .uri       = "/get_configs",
    .method    = HTTP_GET,
    .handler   = root_get_configs_handler,
    .user_ctx  = NULL
};

// Definição da rota "/mesh_list_data"
static const httpd_uri_t mesh_list_data_uri = {
    .uri       = "/mesh_list_data",
    .method    = HTTP_GET,
    .handler   = mesh_list_data_handler,
    .user_ctx  = NULL
};

// Definição da rota "/set_wifi"
static const httpd_uri_t set_wifi_uri = {
    .uri       = "/set_wifi",
    .method    = HTTP_POST,
    .handler   = set_wifi_post_handler,
    .user_ctx  = NULL
};

// Definição da rota "/mesh_tree" para plotar a árvore da rede mesh
static const httpd_uri_t mesh_tree_view_uri = {
    .uri       = "/mesh_tree_view",
    .method    = HTTP_GET,
    .handler   = mesh_tree_view_handler,
    .user_ctx  = NULL
};

// Definição da rota "/mesh_tree_data"
static const httpd_uri_t mesh_tree_data_uri = {
    .uri       = "/mesh_tree_data",
    .method    = HTTP_GET,
    .handler   = mesh_tree_data_handler,
    .user_ctx  = NULL
};

// Definição da rota WebSocket "/ws_update" para acompanhar status do OTA
static httpd_uri_t ws_update_fw_uri = {
    .uri        = "/ws_update",
    .method     = HTTP_GET,
    .handler    = ws_update_fw_handler,
    .is_websocket = true,
    .user_ctx   = NULL
};

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
        httpd_register_uri_handler(webserver_handle, &get_configs_uri);
        httpd_register_uri_handler(webserver_handle, &mesh_list_data_uri);
        httpd_register_uri_handler(webserver_handle, &set_wifi_uri);
        httpd_register_uri_handler(webserver_handle, &ws_update_fw_uri);
        httpd_register_uri_handler(webserver_handle, &mesh_tree_view_uri);
        httpd_register_uri_handler(webserver_handle, &mesh_tree_data_uri);
        ESP_LOGI(TAG, "WebServer Iniciado com sucesso.");
        
        // Inicialize o SPIFFS, onde fica armazenado os arquivos HTML
        init_spiffs();
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


// Função para inicializar o SPIFFS, onde fica armazenado os arquivos HTML
static void init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS partition size total: %d, used: %d", total, used);
    }
}
