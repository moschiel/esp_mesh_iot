#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_mesh.h"

#include "mesh_tree.h"
#include "utils.h"
#include "mesh_network.h"

static char* TAG = "MESH_TREE";
static int node_count = 0;
static SemaphoreHandle_t xMutexTree;
//static bool tree_monitor_started = false;

MeshNode mesh_nodes[CONFIG_MESH_ROUTE_TABLE_SIZE] = {0};
static mesh_addr_t routing_table[CONFIG_MESH_ROUTE_TABLE_SIZE];

static void log_tree(void) {
    ESP_LOGW(TAG, "Tree Updated, Nodes Count: %i", node_count);
    for (size_t i = 0; i < node_count; i++) {
        ESP_LOGW(TAG, "(%i) node:"MACSTR", parent:"MACSTR", layer:%i", 
            (i+1), MAC2STR(mesh_nodes[i].node_addr), MAC2STR(mesh_nodes[i].parent_addr), mesh_nodes[i].layer);
    }
}

void update_tree_with_node(uint8_t node_addr[6], uint8_t parent_addr[6], int layer) {
    if(xMutexTree == NULL) {
        xMutexTree = xSemaphoreCreateMutex();
    }

    if (xSemaphoreTake(xMutexTree, portMAX_DELAY) == pdTRUE) {
        // Verifica se a combinação já existe no array ou se o node_addr já existe
        bool node_exist = false;
        bool updated = false;
        for (size_t i = 0; i < node_count; i++) {
            if (compare_mac(mesh_nodes[i].node_addr, node_addr)) {
                // Se o node_addr já existe, 
                node_exist = true;
                if(compare_mac(mesh_nodes[i].parent_addr, parent_addr) == false || mesh_nodes[i].layer != layer) {
                    //existe mas mudou o parent ou o layer, entao sobrescreve
                    memcpy(mesh_nodes[i].parent_addr, parent_addr, 6);
                    mesh_nodes[i].layer = layer;
                    updated = true;
                }
            }
        }

        // Se o node não existe e há espaço no array, adiciona o novo item
        if (node_exist == false && node_count < CONFIG_MESH_ROUTE_TABLE_SIZE) {
            memcpy(mesh_nodes[node_count].node_addr, node_addr, 6);
            memcpy(mesh_nodes[node_count].parent_addr, parent_addr, 6);
            mesh_nodes[node_count].layer = layer;
            node_count++;
            updated = true;
        }

        if(updated) {
            log_tree();
        }

        xSemaphoreGive(xMutexTree);
    }
}

void remove_node_from_tree(uint8_t node_addr[6]) {
    size_t new_count = 0;

    for (size_t i = 0; i < node_count; i++) {
        if (!compare_mac(mesh_nodes[i].node_addr, node_addr)) {
            // Mantém o node no array se ele não é o que deve ser removido
            if (new_count != i) {
                mesh_nodes[new_count] = mesh_nodes[i];
            }
            new_count++;
        }
    }

    //bool updated = node_count != new_count;
    node_count = new_count;
    //if(updated) log_tree();
}

void clear_node_tree(void) {
    node_count = 0;
}


void remove_non_existing_node_from_tree(void) {
    if(xMutexTree == NULL) {
        xMutexTree = xSemaphoreCreateMutex();
    }

    if (xSemaphoreTake(xMutexTree, portMAX_DELAY) == pdTRUE) {
        int routing_table_size = 0;
        esp_mesh_get_routing_table((mesh_addr_t *)&routing_table, CONFIG_MESH_ROUTE_TABLE_SIZE * 6, &routing_table_size);
        
        bool updated = false;
        int starting_index = 0;
SCAN_REMOVE_NODE:
        for(int i=starting_index; i<node_count; i++) {
            bool exist_on_tree = false;
            for(int k=0; k<routing_table_size;k++) {
                if(compare_mac(mesh_nodes[i].node_addr, routing_table[k].addr)) {
                    exist_on_tree = true;
                    break;
                }
            }
            if(exist_on_tree == false) {
                remove_node_from_tree(mesh_nodes[i].node_addr);
                updated = true;
                starting_index = i;
                goto SCAN_REMOVE_NODE;
            }
        }

        if(updated) {
            log_tree();
        }
    }
}

/*
static void monitor_tree_task(void *arg) {
    if(xMutexTree == NULL) {
        xMutexTree = xSemaphoreCreateMutex();
    }

    ESP_LOGW(TAG, "Started Tree Task");
    
    while(1) {
        if (xSemaphoreTake(xMutexTree, portMAX_DELAY) == pdTRUE) {
            if(esp_mesh_is_root() && is_mesh_parent_connected()) {
                remove_non_existing_node_from_tree();
            } else {
                clear_tree();
                break; //sai do while, e vair deletar a tarefa
            }
            
        }
        vTaskDelay(1 * 1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGW(TAG, "Deleted Tree Task");
    tree_monitor_started = false;
    vTaskDelete(NULL);
}

void start_tree_monitor(void) {
    if (!tree_monitor_started) {
        tree_monitor_started = true;
        xTaskCreate(monitor_tree_task, "monitor_tree_task", 1024, NULL, 5, NULL);

    }
}
*/