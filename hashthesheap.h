#pragma once
#include <stdint.h>

typedef struct flags{
    pid_t process;
    char * dump_file_name;
    int build_heap_tree;
    size_t heap_tree_height;
    int verbose;
}flags;

typedef struct proc_map_heap_info{
    uint32_t start_address;
    uint32_t end_address;
    uint32_t size;
}proc_map_heap_info;

typedef struct hash_tree_node{
    uint8_t hash[16];
    size_t chunk_offset;
    size_t chunk_size;
    struct hash_tree_node * left;
    struct hash_tree_node * right;
}hash_tree_node;
