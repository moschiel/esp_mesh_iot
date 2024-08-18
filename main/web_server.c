#include "esp_log.h"
#include "esp_http_server.h"
#include <string.h>
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "cJSON.h"
#include "esp_spiffs.h"

#include "web_server.h"
#include "web_socket.h"
#include "app_config.h"
#include "utils.h"
#include "mesh_network.h"
#include "mesh_tree.h"
#include "ota.h"
#include "messages.h"
#include "html_macros.h"
#include "config_ip_addr.h"

#if USE_HTML_FROM_EMBED_TXTFILES
#if USE_MIN_HTML
extern const uint8_t index_html[] asm("_binary_min_index_html_start");
extern const uint8_t mesh_graph_html[] asm("_binary_min_mesh_graph_html_start");
#elif USE_FULL_HTML
extern const uint8_t index_html[] asm("_binary_index_html_start");
extern const uint8_t mesh_graph_html[] asm("_binary_mesh_graph_html_start");
#endif
#elif USE_HTML_FROM_MACROS
const char index_html[] = INDEX_HTML;
const char mesh_graph_html[] = MESH_GRAPH_HTML;
#elif USE_HTML_FROM_SPIFFS
#if USE_MIN_HTML
const char index_html[] = "/spiffs/min_index.html";
const char mesh_graph_html[] = "/spiffs/min_mesh_graph.html";
#elif USE_FULL_HTML
const char index_html[] = "/spiffs/index.html";
const char mesh_graph_html[] = "/spiffs/mesh_graph.html";
#endif
#endif

// Define a macro MIN se não estiver definida
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif  

static const char *TAG = "WEB_SERVER";
httpd_handle_t webserver_handle = NULL;

extern uint8_t STA_MAC_address[6];

static void wifi_init_softap(void);
#if USE_HTML_FROM_SPIFFS
static void init_spiffs(void);
#endif

// Manipulador para a rota raiz "/"
static esp_err_t root_get_handler(httpd_req_t *req) {
#if USE_HTML_FROM_EMBED_TXTFILES || USE_HTML_FROM_MACROS
    httpd_resp_send_str_chunk(req, (const char*)index_html);
    httpd_resp_send_chunk(req, NULL, 0); // Terminate the response
    return ESP_OK;
#elif USE_HTML_FROM_SPIFFS
    FILE* f = fopen(index_html, "r");
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
#endif
}

static esp_err_t root_get_configs_handler(httpd_req_t *req) {
    // Get WiFi router credentials from NVS
    char router_ssid[MAX_SSID_LEN] = {0};
    char router_password[MAX_PASS_LEN] = {0};
    nvs_get_wifi_credentials(router_ssid, sizeof(router_ssid), router_password, sizeof(router_password));

    // Obtem as credenciais da rede mesh da NVS
    uint8_t mesh_id[6];
    char mesh_password[MAX_PASS_LEN] = {0};
    nvs_get_mesh_credentials(mesh_id, mesh_password, sizeof(mesh_password));

    // Get IP address config
    char router_ip[IP_ADDR_LEN] = {0};
    char root_node_ip[IP_ADDR_LEN] = {0};
    char netmask[IP_ADDR_LEN] = {0};
    #if ENABLE_CONFIG_STATIC_IP
    nvs_get_ip_config(router_ip, root_node_ip, netmask);
    #endif

    //Obtem ultimo fw url
    char fw_url[100] = {0};
    nvs_get_ota_fw_url(fw_url, sizeof(fw_url));

    char response[600];
    sprintf(response, 
        "{"
            "\"device_mac_addr\":\""MACSTR"\","
            "\"router_ssid\":\"%s\","
            "\"router_password\":\"%s\","
            "\"mesh_id\":\""MESHSTR"\"," 
            "\"mesh_password\":\"%s\","
            "\"router_ip\":\"%s\","
            "\"root_ip\":\"%s\","
            "\"netmask\":\"%s\","
            "\"is_connected\":%s,"
            "\"fw_url\":\"%s\""
        "}",
        MAC2STR(STA_MAC_address),
        router_ssid,
        router_password,
        MESH2STR(mesh_id),
        mesh_password,
        router_ip,
        root_node_ip,
        netmask,
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
        ESP_LOGE(TAG, "set_wifi_post_handler: Failed to allocate memory");
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

    // Parseia o JSON recebido
    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        ESP_LOGE(TAG, "set_wifi_post_handler: JSON parsing error");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, NULL, 0);
        free(buf);
        return ESP_FAIL;
    }

    if(process_msg_set_wifi_config(json)) {
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
    cJSON_Delete(json);

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
#if USE_HTML_FROM_EMBED_TXTFILES || USE_HTML_FROM_MACROS
    httpd_resp_send_str_chunk(req, (const char*)mesh_graph_html);
    httpd_resp_send_chunk(req, NULL, 0); // Terminate the response
    return ESP_OK;
#elif USE_HTML_FROM_SPIFFS
    FILE* f = fopen(mesh_graph_html, "r");
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
#endif
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
static esp_err_t ws_update_fw_handler(httpd_req_t *req)
{
    char payload[500];
    
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Websocket handshake");
        if(mount_msg_ota_status(payload, sizeof(payload), "WebSocket handshake from ESP", false, false, 0)) {
            reply_req_ws_frame_str(req, payload);
        }
    } else {
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.payload = (uint8_t *)payload;
        esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(payload));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            return ret;
        }
        
        if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
            payload[ws_pkt.len] = '\0';
            ESP_LOGI(TAG, "Received packet with message: %s", ws_pkt.payload);
            if(!process_msg_fw_update_request(payload)) {
                send_ws_ota_status("Fail to parse JSON", true, true, 0);
            }
        }
    }
    return ESP_OK;
}

static void register_uris(void) {
    // Definição da rota raiz
    httpd_uri_t root_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_get_handler,
        .user_ctx  = NULL
    };

    // Definição da rota "/get_configs" para requisitar configs
    httpd_uri_t get_configs_uri = {
        .uri       = "/get_configs",
        .method    = HTTP_GET,
        .handler   = root_get_configs_handler,
        .user_ctx  = NULL
    };

    // Definição da rota "/mesh_list_data"
    httpd_uri_t mesh_list_data_uri = {
        .uri       = "/mesh_list_data",
        .method    = HTTP_GET,
        .handler   = mesh_list_data_handler,
        .user_ctx  = NULL
    };

    // Definição da rota "/set_wifi"
    httpd_uri_t set_wifi_uri = {
        .uri       = "/set_wifi",
        .method    = HTTP_POST,
        .handler   = set_wifi_post_handler,
        .user_ctx  = NULL
    };

    // Definição da rota "/mesh_tree" para plotar a árvore da rede mesh
    httpd_uri_t mesh_tree_view_uri = {
        .uri       = "/mesh_tree_view",
        .method    = HTTP_GET,
        .handler   = mesh_tree_view_handler,
        .user_ctx  = NULL
    };

    // Definição da rota "/mesh_tree_data"
    httpd_uri_t mesh_tree_data_uri = {
        .uri       = "/mesh_tree_data",
        .method    = HTTP_GET,
        .handler   = mesh_tree_data_handler,
        .user_ctx  = NULL
    };

    // Definição da rota WebSocket "/ws_update" para acompanhar status do OTA
    httpd_uri_t ws_update_fw_uri = {
        .uri        = "/ws_update",
        .method     = HTTP_GET,
        .handler    = ws_update_fw_handler,
        .is_websocket = true,
        .user_ctx   = NULL
    };

    httpd_register_uri_handler(webserver_handle, &root_uri);
    httpd_register_uri_handler(webserver_handle, &get_configs_uri);
    httpd_register_uri_handler(webserver_handle, &mesh_list_data_uri);
    httpd_register_uri_handler(webserver_handle, &set_wifi_uri);
    httpd_register_uri_handler(webserver_handle, &ws_update_fw_uri);
    httpd_register_uri_handler(webserver_handle, &mesh_tree_view_uri);
    httpd_register_uri_handler(webserver_handle, &mesh_tree_data_uri);
}

// Inicia o servidor web
void start_webserver(bool init_wifi_ap) { 
    ESP_LOGI(TAG, "Starting WebServer");

    if(is_webserver_active()) {
        ESP_LOGW(TAG, "WebServer was started already");
        return;
    }

    if(init_wifi_ap) {
        wifi_init_softap();
    }

    initialise_mdns();
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    //config.max_uri_handlers = 8;  // Ajusta o número máximo de manipuladores de URI
    //config.max_resp_headers = 16;  // Ajusta o número máximo de cabeçalhos de resposta
    config.stack_size = 8192;  // Aumenta o tamanho da pilha
    //config.recv_wait_timeout = 10;  // Ajusta o tempo de espera para receber (em segundos)
    //config.send_wait_timeout = 10;  // Ajusta o tempo de espera para enviar (em segundos)

    if (httpd_start(&webserver_handle, &config) == ESP_OK) {
        register_uris();
        ESP_LOGI(TAG, "WebServer init success.");
#if USE_HTML_FROM_SPIFFS
        // Inicialize o SPIFFS, onde fica armazenado os arquivos HTML
        init_spiffs();
#endif
    } else {
        ESP_LOGE(TAG, "WebServer failed.");
        webserver_handle = NULL;
    }
}

// Para o servidor web
void stop_webserver() {
    if (is_webserver_active()) {
        httpd_stop(webserver_handle);
        webserver_handle = NULL;
        ESP_LOGW(TAG, "WebServer was stopped.");
    }
}

bool is_webserver_active(void) {
    return webserver_handle != NULL;
}

// Inicialização do Wi-Fi em modo ponto de acesso (AP)
static void wifi_init_softap(void) {
    ESP_LOGI(TAG, "Initializing WiFi as Access Point");

    // Inicializa o armazenamento não volátil (NVS)
    // nvs_flash_init();

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

#if USE_HTML_FROM_SPIFFS
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
        ESP_LOGI(TAG, "SPIFFS partition size total: %d, used: %d (%u%%)", total, used, (uint8_t)(((float)used/(float)total)*100.0));
    }
}
#endif

