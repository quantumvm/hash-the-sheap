#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>


/*
 * This program is designed to support 32-bit dumps
 *
 */

typedef struct proc_map_heap_info{
    uint32_t start_address;
    uint32_t end_address;
    uint32_t size;
}proc_map_heap_info;


void stop_and_wait(pid_t pid){
    int status;
    ptrace(PTRACE_ATTACH, pid, NULL, NULL);
    waitpid(pid, &status, 0);
}

void countinue_stopped_process(pid_t pid){
    int status;
    ptrace(PTRACE_DETACH, pid, NULL, NULL);
    waitpid(pid, &status, 0);   
}

/**************************
 * proc map functions     *
 **************************/

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

void print_heap_info(proc_map_heap_info * heap_info){
    puts(  "HEAP");
    printf("  start-address: %x\n", heap_info->start_address);
    printf("  end-address: %x\n", heap_info->end_address);
    printf("  size: %x\n", heap_info->size);
}


/**************************
 * proc mem functions     *
 **************************/

static void dump_to_file(FILE * mem_file, FILE * out_file, proc_map_heap_info * heap_info){
    //seek to the start of the heap
    fseek(mem_file, heap_info->start_address, SEEK_SET);
    
    char read_buf[1024];
    int chunks = heap_info->size/sizeof(read_buf);
    chunks = ((chunks%sizeof(read_buf)) == 0) ? (chunks) : (chunks +1);


    for(int i = 0; i<chunks; i++){
        size_t in = fread(read_buf, 1, sizeof(read_buf), mem_file);
        fwrite(read_buf, 1, in, out_file);
    }
    
}



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
     
    print_heap_info(&heap_info); 
    
    //stop the program and dump the heap
    FILE * heap_dump_file = fopen("heapdump.bin", "w");
    stop_and_wait((pid_t) atoi(argv[1]));
    dump_to_file(proc_mem_file, heap_dump_file, &heap_info);
    countinue_stopped_process((pid_t) atoi(argv[1]));
}

