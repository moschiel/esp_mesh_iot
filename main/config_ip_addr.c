#include <string.h>
#include <inttypes.h>
#include <netdb.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"

#include "config_ip_addr.h"
#include "app_config.h"
#include "utils.h"


static const char* TAG = "CONFIG_IP_ADDR";

static esp_err_t set_dns_server(esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type)
{
    if (addr && (addr != IPADDR_NONE)) {
        esp_netif_dns_info_t dns;
        dns.ip.u_addr.ip4.addr = addr;
        dns.ip.type = IPADDR_TYPE_V4;
        ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, type, &dns));
    }
    return ESP_OK;
}

void set_static_ip(esp_netif_t *netif)
{
    ESP_LOGI(TAG, "Setting static IP address");

    // Get IP address config
    char router_ip[IP_ADDR_LEN] = {0};
    char root_node_ip[IP_ADDR_LEN] = {0};
    char netmask[IP_ADDR_LEN] = {0};
    if(nvs_get_ip_config(router_ip, root_node_ip, netmask) != ESP_OK) {
        return;
    }

    if (esp_netif_dhcpc_stop(netif) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop dhcp client");
        return;
    }
    esp_netif_ip_info_t ip;
    memset(&ip, 0 , sizeof(esp_netif_ip_info_t));
    ip.ip.addr = ipaddr_addr(root_node_ip);
    ip.gw.addr = ipaddr_addr(router_ip);
    ip.netmask.addr = ipaddr_addr(netmask);
    if (esp_netif_set_ip_info(netif, &ip) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ip info");
        return;
    }

    //DNS STATIC AUTOMATIC
    ESP_ERROR_CHECK(set_dns_server(netif, ip.gw.addr, ESP_NETIF_DNS_MAIN));
    ESP_ERROR_CHECK(set_dns_server(netif, ipaddr_addr("0.0.0.0"), ESP_NETIF_DNS_BACKUP));
    
    ESP_LOGD(TAG, "Success to set static ip: %s, gw: %s, netmask: %s", root_node_ip, router_ip, netmask);
}