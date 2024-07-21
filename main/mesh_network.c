/* Mesh Internal Communication Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <inttypes.h>
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "esp_mesh_internal.h"
#include "nvs_flash.h"

#include "mesh_network.h"
#include "app_config.h"
#include "web_server.h"
#include "config_ip_addr.h"
#include "messages.h"
#include "mesh_tree.h"

/*******************************************************
 *                Macros
 *******************************************************/
#define MAX_MESH_AP_CONNECTIONS_ON_EACH_NODE 2 //CONFIG_MESH_AP_CONNECTIONS
/*******************************************************
 *                Constants
 *******************************************************/
#define RX_SIZE          (1500)
#define TX_SIZE          (1460)

/*******************************************************
 *                Variable Definitions
 *******************************************************/
static const char *TAG = "MESH_NETWORK";
static uint8_t tx_buf[TX_SIZE] = { 0 };
static uint8_t rx_buf[RX_SIZE] = { 0 };
static bool is_mesh_connected = false;
static int mesh_layer = -1;
static esp_netif_t *netif_sta = NULL;

//node_addr é o endereço relacionado ao WiFi no modo estação 'STA_MAC_address'
//parent_addr do ponto de vista de um node, é o WiFi do pai como ponto de acesso a malha 'AP_MESH_address'
//onde o endereço mac do modo AP é 1 unidade maior do que o mac no modo STA
static mesh_addr_t ROOT_AP_MESH_address;
static mesh_addr_t ROOT_STA_MESH_address;
static mesh_addr_t PARENT_AP_MESH_address;
static mesh_addr_t PARENT_STA_MESH_address;
extern uint8_t STA_MAC_address[6];

/*******************************************************
 *                Function Declarations
 *******************************************************/

/*******************************************************
 *                Function Definitions
 *******************************************************/
void mesh_p2p_tx_task(void *arg)
{
    esp_err_t err;
    mesh_data_t mesh_data;
    mesh_data.data = tx_buf;
    mesh_data.tos = MESH_TOS_P2P;
    uint32_t iteration_count = 0;
    while (1) {
        if(is_mesh_connected) {
            err = ESP_OK;

            if(!esp_mesh_is_root())
            {
                if((iteration_count % 10) == 0) { // reporta cada 10 segundos
                    mesh_data.proto = MESH_PROTO_JSON;
                    mount_msg_node_status((char*)tx_buf, sizeof(tx_buf), STA_MAC_address, PARENT_STA_MESH_address.addr, mesh_layer, CONFIG_APP_PROJECT_VER);
                    mesh_data.size = strlen((char*)tx_buf) + 1;

                    err = esp_mesh_send(
                        NULL,       // mesh_addr_t *to, (use NULL to send to root node)
                        &mesh_data, // mesh_data_t *data           
                        0,          // int flag, If the packet is to the root and “to” parameter is NULL, set this parameter to 0.
                        NULL,       // mesh_opt_t opt[]
                        0           // int opt_count
                    );
                }
            }

            if (err != ESP_OK) {
                ESP_LOGE(TAG, "'esp_mesh_send' fail, msg_id:%i, heap:%" PRId32 "[err:0x%x, proto:%d, tos:%d]",
                    JSON_MSG_NODE_CONNECTED, esp_get_minimum_free_heap_size(), err, mesh_data.proto, mesh_data.tos);
            }
        }

        vTaskDelay(1 * 1000 / portTICK_PERIOD_MS);
        iteration_count++;
    }
    vTaskDelete(NULL);
}

void mesh_p2p_rx_task(void *arg)
{
    esp_err_t err;
    mesh_addr_t from;
    mesh_data_t mesh_data;
    int flag = 0;
    mesh_data.data = rx_buf;
    mesh_data.size = RX_SIZE;

    while (1) {
        mesh_data.size = RX_SIZE;
        err = esp_mesh_recv(&from, &mesh_data, portMAX_DELAY, &flag, NULL, 0);
        if (err != ESP_OK || !mesh_data.size) {
            ESP_LOGE(TAG, "'esp_mesh_recv' err:0x%x, size:%d", err, mesh_data.size);
            continue;
        }

        if(mesh_data.proto == MESH_PROTO_BIN) {
            if(mesh_data.size >= sizeof(uint16_t)) //size of msg id
            {
                uint16_t msg_id;
                memcpy((uint8_t*)&msg_id, mesh_data.data, sizeof(uint16_t));
                switch(msg_id)
                {
                    case BIN_MSG_FW_PACKET:
                        if(!esp_mesh_is_root()) {
                            process_msg_firmware_packet((firmware_packet_t*)mesh_data.data);
                        }
                    break;
                }
            }
        }
        else if(mesh_data.proto == MESH_PROTO_JSON)
        {
            if(strlen((char*)mesh_data.data) < RX_SIZE) {
                ESP_LOGI(TAG, "Recv from "MACSTR": %s", MAC2STR(from.addr), (char*)mesh_data.data);
            }

            cJSON *root = cJSON_Parse((char*)mesh_data.data);
            if(root == NULL) {
                ESP_LOGE(TAG, "Fail to parse JSON");
                continue;
            }

            cJSON *msg_id_json = cJSON_GetObjectItem(root, "msg_id");

            bool process_fail = false;
            if(msg_id_json && cJSON_IsNumber(msg_id_json)) {
                switch (msg_id_json->valueint)
                {
                case JSON_MSG_NODE_CONNECTED:
                    if(esp_mesh_is_root()) {
                        if(!process_msg_node_status(root)) {
                            process_fail = true;
                        }
                    }    
                    break;
                
                default:
                    break;
                }    
            }

            if(process_fail) {
                 ESP_LOGE(TAG, "Fail to parse json msg id: %i", JSON_MSG_NODE_CONNECTED);
            }

            // Libera a memória usada pelo objeto JSON
            cJSON_Delete(root);

        }
    }
    vTaskDelete(NULL);
}

esp_err_t esp_mesh_comm_p2p_start(void)
{
    static bool is_comm_p2p_started = false;
    if (!is_comm_p2p_started) {
        is_comm_p2p_started = true;
        xTaskCreate(mesh_p2p_tx_task, "mesh_p2p_tx_task", 3072, NULL, 5, NULL);
        xTaskCreate(mesh_p2p_rx_task, "mesh_p2p_rx_task", 3072, NULL, 5, NULL);
    }
    return ESP_OK;
}

bool is_mesh_parent_connected(void) {
    return is_mesh_connected;
}

char* get_mesh_root_ip(void)
{
    if(netif_sta && esp_mesh_is_root()) {
        esp_netif_ip_info_t ip_info;
        if(esp_netif_get_ip_info(netif_sta, &ip_info) == ESP_OK)
        {
            return ip4addr_ntoa((const ip4_addr_t*)&(ip_info.ip));
        }
    }
    return NULL;
}

void mesh_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    mesh_addr_t id = {0,};
    static uint16_t last_layer = 0;

    switch (event_id) {
    case MESH_EVENT_STARTED: {
        esp_mesh_get_id(&id);
        ESP_LOGI(TAG, "<MESH_EVENT_MESH_STARTED>ID:"MACSTR"", MAC2STR(id.addr));
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_STOPPED: {
        ESP_LOGI(TAG, "<MESH_EVENT_STOPPED>");
        is_mesh_connected = false;
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_CHILD_CONNECTED: {
        mesh_event_child_connected_t *child_connected = (mesh_event_child_connected_t *)event_data;
        ESP_LOGI(TAG, "<MESH_EVENT_CHILD_CONNECTED>aid:%d, "MACSTR"",
                 child_connected->aid,
                 MAC2STR(child_connected->mac));
    }
    break;
    case MESH_EVENT_CHILD_DISCONNECTED: {
        mesh_event_child_disconnected_t *child_disconnected = (mesh_event_child_disconnected_t *)event_data;
        ESP_LOGI(TAG, "<MESH_EVENT_CHILD_DISCONNECTED>aid:%d, "MACSTR"",
                 child_disconnected->aid,
                 MAC2STR(child_disconnected->mac));
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_ADD: {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(TAG, "<MESH_EVENT_ROUTING_TABLE_ADD>add %d, new:%d, layer:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new, mesh_layer);
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_REMOVE: {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(TAG, "<MESH_EVENT_ROUTING_TABLE_REMOVE>remove %d, new:%d, layer:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new, mesh_layer);
        
        if (esp_mesh_is_root()) {         
            remove_non_existing_node_from_tree();
        }
    }
    break;
    case MESH_EVENT_NO_PARENT_FOUND: {
        mesh_event_no_parent_found_t *no_parent = (mesh_event_no_parent_found_t *)event_data;
        ESP_LOGI(TAG, "<MESH_EVENT_NO_PARENT_FOUND>scan times:%d",
                 no_parent->scan_times);
    }
    /* TODO handler for the failure */
    break;
    case MESH_EVENT_PARENT_CONNECTED: {
        mesh_event_connected_t *connected = (mesh_event_connected_t *)event_data;
        esp_mesh_get_id(&id);
        mesh_layer = connected->self_layer;
        memcpy(PARENT_AP_MESH_address.addr, connected->connected.bssid, 6);
        memcpy(PARENT_STA_MESH_address.addr, PARENT_AP_MESH_address.addr, 6);
        //O endereço mac no modo AP é 1 unidade maior do que o mac no modo STA
        PARENT_STA_MESH_address.addr[5] = PARENT_AP_MESH_address.addr[5] - 1;

        ESP_LOGI(TAG,
                 "<MESH_EVENT_PARENT_CONNECTED>layer:%d-->%d, parent: sta"MACSTR"/ ap"MACSTR" %s, ID:"MACSTR", duty:%d",
                 last_layer, mesh_layer, MAC2STR(PARENT_STA_MESH_address.addr), MAC2STR(PARENT_AP_MESH_address.addr),
                 esp_mesh_is_root() ? "<ROOT>" :
                 (mesh_layer == 2) ? "<layer2>" : "", MAC2STR(id.addr), connected->duty);
        last_layer = mesh_layer;
        //mesh_connected_indicator(mesh_layer); //LED RGB, setava a cor do LED de acordo com a camada do nó (Ex: camada 1: red, 2: verde, 3: azul, e assim por diante)
        is_mesh_connected = true;

        if (esp_mesh_is_root()) {
            esp_netif_dhcpc_stop(netif_sta);
            esp_netif_dhcpc_start(netif_sta);
            
            set_static_ip(netif_sta);

            clear_node_tree();
           // start_tree_monitor();
        }
        esp_mesh_comm_p2p_start();
    }
    break;
    case MESH_EVENT_PARENT_DISCONNECTED: {
        mesh_event_disconnected_t *disconnected = (mesh_event_disconnected_t *)event_data;
        ESP_LOGI(TAG,
                 "<MESH_EVENT_PARENT_DISCONNECTED>reason:%d",
                 disconnected->reason);
        is_mesh_connected = false;
        //mesh_disconnected_indicator(); //LED RGB, setava uma cor de alerta pra dizer que nao esta conectado ao pai, logo nao esta em nenhuma camada.
        mesh_layer = esp_mesh_get_layer();

        if (esp_mesh_is_root()) {
            stop_webserver();
        }
    }
    break;
    case MESH_EVENT_LAYER_CHANGE: {
        mesh_event_layer_change_t *layer_change = (mesh_event_layer_change_t *)event_data;
        mesh_layer = layer_change->new_layer;
        ESP_LOGI(TAG, "<MESH_EVENT_LAYER_CHANGE>layer:%d-->%d%s",
                 last_layer, mesh_layer,
                 esp_mesh_is_root() ? "<ROOT>" :
                 (mesh_layer == 2) ? "<layer2>" : "");
        last_layer = mesh_layer;
        //mesh_connected_indicator(mesh_layer); //LED RGB, setava a cor do LED de acordo com a camada do nó (Ex: camada 1: red, 2: verde, 3: azul, e assim por diante)
    }
    break;
    case MESH_EVENT_ROOT_ADDRESS: {
        mesh_event_root_address_t *root_addr = (mesh_event_root_address_t *)event_data;
        memcpy(ROOT_AP_MESH_address.addr, root_addr->addr, 6);
        memcpy(ROOT_STA_MESH_address.addr, ROOT_AP_MESH_address.addr, 6);
        //O endereço mac no modo AP é 1 unidade maior do que o mac no modo STA
        ROOT_STA_MESH_address.addr[5] = ROOT_AP_MESH_address.addr[5] - 1;

        ESP_LOGI(TAG, "<MESH_EVENT_ROOT_ADDRESS>root address: sta"MACSTR" / ap"MACSTR"",
                 MAC2STR(ROOT_STA_MESH_address.addr), MAC2STR(ROOT_AP_MESH_address.addr));
    }
    break;
    case MESH_EVENT_VOTE_STARTED: {
        mesh_event_vote_started_t *vote_started = (mesh_event_vote_started_t *)event_data;
        ESP_LOGI(TAG,
                 "<MESH_EVENT_VOTE_STARTED>attempts:%d, reason:%d, rc_addr:"MACSTR"",
                 vote_started->attempts,
                 vote_started->reason,
                 MAC2STR(vote_started->rc_addr.addr));
    }
    break;
    case MESH_EVENT_VOTE_STOPPED: {
        ESP_LOGI(TAG, "<MESH_EVENT_VOTE_STOPPED>");
        break;
    }
    case MESH_EVENT_ROOT_SWITCH_REQ: {
        mesh_event_root_switch_req_t *switch_req = (mesh_event_root_switch_req_t *)event_data;
        ESP_LOGI(TAG,
                 "<MESH_EVENT_ROOT_SWITCH_REQ>reason:%d, rc_addr:"MACSTR"",
                 switch_req->reason,
                 MAC2STR( switch_req->rc_addr.addr));
    }
    break;
    case MESH_EVENT_ROOT_SWITCH_ACK: {
        /* new root */
        mesh_layer = esp_mesh_get_layer();
        esp_mesh_get_parent_bssid(&PARENT_AP_MESH_address);
        ESP_LOGI(TAG, "<MESH_EVENT_ROOT_SWITCH_ACK>layer:%d, parent:"MACSTR"", mesh_layer, MAC2STR(PARENT_AP_MESH_address.addr));
    }
    break;
    case MESH_EVENT_TODS_STATE: {
        mesh_event_toDS_state_t *toDs_state = (mesh_event_toDS_state_t *)event_data;
        ESP_LOGI(TAG, "<MESH_EVENT_TODS_REACHABLE>state:%d", *toDs_state);
    }
    break;
    case MESH_EVENT_ROOT_FIXED: {
        mesh_event_root_fixed_t *root_fixed = (mesh_event_root_fixed_t *)event_data;
        ESP_LOGI(TAG, "<MESH_EVENT_ROOT_FIXED>%s",
                 root_fixed->is_fixed ? "fixed" : "not fixed");
    }
    break;
    case MESH_EVENT_ROOT_ASKED_YIELD: {
        mesh_event_root_conflict_t *root_conflict = (mesh_event_root_conflict_t *)event_data;
        ESP_LOGI(TAG,
                 "<MESH_EVENT_ROOT_ASKED_YIELD>"MACSTR", rssi:%d, capacity:%d",
                 MAC2STR(root_conflict->addr),
                 root_conflict->rssi,
                 root_conflict->capacity);
    }
    break;
    case MESH_EVENT_CHANNEL_SWITCH: {
        mesh_event_channel_switch_t *channel_switch = (mesh_event_channel_switch_t *)event_data;
        ESP_LOGI(TAG, "<MESH_EVENT_CHANNEL_SWITCH>new channel:%d", channel_switch->channel);
    }
    break;
    case MESH_EVENT_SCAN_DONE: {
        mesh_event_scan_done_t *scan_done = (mesh_event_scan_done_t *)event_data;
        ESP_LOGI(TAG, "<MESH_EVENT_SCAN_DONE>number:%d",
                 scan_done->number);
    }
    break;
    case MESH_EVENT_NETWORK_STATE: {
        mesh_event_network_state_t *network_state = (mesh_event_network_state_t *)event_data;
        ESP_LOGI(TAG, "<MESH_EVENT_NETWORK_STATE>is_rootless:%d",
                 network_state->is_rootless);
    }
    break;
    case MESH_EVENT_STOP_RECONNECTION: {
        ESP_LOGI(TAG, "<MESH_EVENT_STOP_RECONNECTION>");
    }
    break;
    case MESH_EVENT_FIND_NETWORK: {
        mesh_event_find_network_t *find_network = (mesh_event_find_network_t *)event_data;
        ESP_LOGI(TAG, "<MESH_EVENT_FIND_NETWORK>new channel:%d, router BSSID:"MACSTR"",
                 find_network->channel, MAC2STR(find_network->router_bssid));
    }
    break;
    case MESH_EVENT_ROUTER_SWITCH: {
        mesh_event_router_switch_t *router_switch = (mesh_event_router_switch_t *)event_data;
        ESP_LOGI(TAG, "<MESH_EVENT_ROUTER_SWITCH>new router:%s, channel:%d, "MACSTR"",
                 router_switch->ssid, router_switch->channel, MAC2STR(router_switch->bssid));
    }
    break;
    case MESH_EVENT_PS_PARENT_DUTY: {
        mesh_event_ps_duty_t *ps_duty = (mesh_event_ps_duty_t *)event_data;
        ESP_LOGI(TAG, "<MESH_EVENT_PS_PARENT_DUTY>duty:%d", ps_duty->duty);
    }
    break;
    case MESH_EVENT_PS_CHILD_DUTY: {
        mesh_event_ps_duty_t *ps_duty = (mesh_event_ps_duty_t *)event_data;
        ESP_LOGI(TAG, "<MESH_EVENT_PS_CHILD_DUTY>cidx:%d, "MACSTR", duty:%d", ps_duty->child_connected.aid-1,
                MAC2STR(ps_duty->child_connected.mac), ps_duty->duty);
    }
    break;
    default:
        ESP_LOGI(TAG, "unknown id:%" PRId32 "", event_id);
        break;
    }
}

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    switch(event_id)
    {
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "<WIFI_EVENT_STA_CONNECTED>");
        break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "<WIFI_EVENT_STA_DISCONNECTED>");
        break;
    }

}

void ip_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;

    switch(event_id)
    {
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "<IP_EVENT_STA_GOT_IP>IP:" IPSTR, IP2STR(&event->ip_info.ip));
            if(esp_mesh_is_root()) {
                start_webserver(false);
            }
        break;

        case IP_EVENT_STA_LOST_IP:
            ESP_LOGW(TAG, "<IP_EVENT_STA_LOST_IP>");
        break;
    }

}

void start_mesh(char* router_ssid, char* router_password, uint8_t mesh_id[6], char* mesh_password)
{
    // Inicializa o armazenamento não volátil (NVS)
    // ESP_ERROR_CHECK(nvs_flash_init());

    /*  tcpip initialization */
    ESP_ERROR_CHECK(esp_netif_init());
    /*  event initialization */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /*  create network interfaces for mesh (only station instance saved for further manipulation, soft AP instance ignored */
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_mesh_netifs(&netif_sta, NULL));
    /*  wifi initialization */
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());
    /*  mesh initialization */
    ESP_ERROR_CHECK(esp_mesh_init());
    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));
    /*  set mesh topology */
    ESP_ERROR_CHECK(esp_mesh_set_topology(CONFIG_MESH_TOPOLOGY));
    /*  set mesh max layer according to the topology */
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(CONFIG_MESH_MAX_LAYER));
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(1));
    ESP_ERROR_CHECK(esp_mesh_set_xon_qsize(128));
#ifdef CONFIG_MESH_ENABLE_PS
    /* Enable mesh PS function */
    ESP_ERROR_CHECK(esp_mesh_enable_ps());
    /* better to increase the associate expired time, if a small duty cycle is set. */
    ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(60));
    /* better to increase the announce interval to avoid too much management traffic, if a small duty cycle is set. */
    ESP_ERROR_CHECK(esp_mesh_set_announce_interval(600, 3300));
#else
    /* Disable mesh PS function */
    ESP_ERROR_CHECK(esp_mesh_disable_ps());
    ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(10));
#endif
    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
    /* mesh ID */
    memcpy((uint8_t *) &cfg.mesh_id, mesh_id, 6);
    
    /* router */
    cfg.channel = CONFIG_MESH_CHANNEL;
    cfg.router.ssid_len = strlen(router_ssid);
    memcpy((uint8_t *) &cfg.router.ssid, router_ssid, cfg.router.ssid_len);
    memcpy((uint8_t *) &cfg.router.password, router_password, strlen(router_password));

    /* mesh softAP */
    ESP_ERROR_CHECK(esp_mesh_set_ap_authmode(CONFIG_MESH_AP_AUTHMODE));
    cfg.mesh_ap.max_connection = MAX_MESH_AP_CONNECTIONS_ON_EACH_NODE;
    cfg.mesh_ap.nonmesh_max_connection = CONFIG_MESH_NON_MESH_AP_CONNECTIONS;
    if((cfg.mesh_ap.max_connection + cfg.mesh_ap.nonmesh_max_connection) > 10) {
        ESP_LOGE(TAG, "Config ultrapassa o maximo de conexoes permitidas");
        return;
    }
    memcpy((uint8_t *) &cfg.mesh_ap.password, mesh_password, strlen(mesh_password));
    ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));
    /* mesh start */
    ESP_ERROR_CHECK(esp_mesh_start());
#ifdef CONFIG_MESH_ENABLE_PS
    /* set the device active duty cycle. (default:10, MESH_PS_DEVICE_DUTY_REQUEST) */
    ESP_ERROR_CHECK(esp_mesh_set_active_duty_cycle(CONFIG_MESH_PS_DEV_DUTY, CONFIG_MESH_PS_DEV_DUTY_TYPE));
    /* set the network active duty cycle. (default:10, -1, MESH_PS_NETWORK_DUTY_APPLIED_ENTIRE) */
    ESP_ERROR_CHECK(esp_mesh_set_network_duty_cycle(CONFIG_MESH_PS_NWK_DUTY, CONFIG_MESH_PS_NWK_DUTY_DURATION, CONFIG_MESH_PS_NWK_DUTY_RULE));
#endif
    ESP_LOGI(TAG, "mesh starts successfully, heap:%" PRId32 ", %s<%d>%s, ps:%d",  esp_get_minimum_free_heap_size(),
             esp_mesh_is_root_fixed() ? "root fixed" : "root not fixed",
             esp_mesh_get_topology(), esp_mesh_get_topology() ? "(chain)":"(tree)", esp_mesh_is_ps_enabled());
}
