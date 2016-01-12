#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*
 * This program is designed to support 32-bit dumps
 *
 */




/**************************
 * proc map functions     *
 **************************/

typedef struct proc_map_heap_info{
    uint32_t start_address;
    uint32_t end_address;
    uint32_t size;
}proc_map_heap_info;


//returns 1 if the given line from proc map is the heap
static int proc_map_is_heap(char * line){
    if(strstr(line, "[heap]") != NULL){
        return 1;
    } 
    return 0;
}


//return a pointer to the procmap line describes the heap
static char * proc_map_find_heap(FILE * file){
    char * line = NULL;
    size_t n = 0;

    while(getline(&line, &n, file) != -1){
        if(proc_map_is_heap(line)){
            return line;
        }

        free(line);
        line = NULL;
        n= 0;
    }

    return NULL;
}

//loads the values in the procmap line into a proc_map_heap_info struct
void parse_proc_map_heap(char * line, proc_map_heap_info * heap_struct){
    sscanf(line, "%x-%x", &heap_struct->start_address, &heap_struct->end_address);
    heap_struct->size = heap_struct->end_address - heap_struct->start_address;
}


/**************************
 * proc mem functions     *
 **************************/




int main(int argc, char * argv[]){
    if(argc < 2){
        printf("USE: %s pid\n", argv[0]);
        exit(0);
    }
    
    if(geteuid() != 0){
        fprintf(stderr, "This program requires root.\n");
        exit(1);
    }
    
    char * proc_map_path;
    char * proc_mem_path;
    asprintf(&proc_map_path, "/proc/%s/maps", argv[1]);
    asprintf(&proc_mem_path, "/proc/%s/mem" , argv[1]);
    
    FILE * proc_map_file = fopen(proc_map_path, "r");
    FILE * proc_mem_file = fopen(proc_mem_path, "r");
   
    //parse the proc maps file
    char * proc_map_heap_line = proc_map_find_heap(proc_map_file);
    proc_map_heap_info heap_info;
    parse_proc_map_heap(proc_map_heap_line, &heap_info); 
    
    
    //start doing our memory stuff
    

    puts(  "HEAP");
    printf("  start-address: %x\n", heap_info.start_address);
    printf("  end-address: %x\n", heap_info.end_address);
    printf("  size: %x\n", heap_info.size);


}

