#ifndef MAIN_MESH_TREE_H_
#define MAIN_MESH_TREE_H_

#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>

// Definindo a estrutura
typedef struct {
    uint8_t node_addr[6];
    uint8_t parent_addr[6];
    int layer;
} MeshNode;

//void start_tree_monitor(void);
void update_tree_with_node(uint8_t node_addr[6], uint8_t parent_addr[6], int layer);
void remove_node_from_tree(uint8_t node_addr[6]);
void remove_non_existing_node_from_tree(void);
void clear_node_tree(void);

#endif /* MAIN_MESH_TABLE_H_ */
