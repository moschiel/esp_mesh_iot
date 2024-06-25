#include <string.h>
#include <inttypes.h>
#include <netdb.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"

#include "config_ip_addr.h"
#include "app_config.h"


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
    ESP_LOGI(TAG, "Configurando IP Estatico");

    if (esp_netif_dhcpc_stop(netif) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop dhcp client");
        return;
    }
    esp_netif_ip_info_t ip;
    memset(&ip, 0 , sizeof(esp_netif_ip_info_t));
    ip.ip.addr = ipaddr_addr(DEFAULT_STATIC_IP_ADDR);
    ip.netmask.addr = ipaddr_addr(DEFAULT_STATIC_NETMASK_ADDR);
    ip.gw.addr = ipaddr_addr(DEFAULT_STATIC_GW_ADDR);
    if (esp_netif_set_ip_info(netif, &ip) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ip info");
        return;
    }
    ESP_LOGD(TAG, "Success to set static ip: %s, netmask: %s, gw: %s", DEFAULT_STATIC_IP_ADDR, DEFAULT_STATIC_NETMASK_ADDR, DEFAULT_STATIC_GW_ADDR);
    ESP_ERROR_CHECK(set_dns_server(netif, ipaddr_addr(DEFAULT_MAIN_DNS_SERVER), ESP_NETIF_DNS_MAIN));
    ESP_ERROR_CHECK(set_dns_server(netif, ipaddr_addr(DEFAULT_BACKUP_DNS_SERVER), ESP_NETIF_DNS_BACKUP));
}