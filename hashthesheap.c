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

#include <openssl/md5.h>

#include "hashthesheap.h"
#include "interface.h"

#define NO_COLOR    "\x1B[0m"
#define RED         "\x1B[31m"
#define GREEN       "\x1B[32m"
#define YELLOW      "\x1B[33m"
#define BLUE        "\x1B[34m"
#define MAGINTA     "\x1B[35m"
#define CYAN        "\x1B[36m"
#define WHTITE      "\x1B[37m"


/*
 * This program is designed to support 32-bit dumps
 *
 */




/**************************
 * hash_tree functions    *
 **************************/

// The hash tree has the following structure 
//
// hash[offset, size]
//
//            hash[offset, length]
//       ___________|____________
//      |                        |
//  hash[offset, length/2]     hash[offset + length/2, length/2]
//

//Returns a tree of hashes of the heap memory of height [height]
static hash_tree_node * generate_hash_tree(uint8_t * heap, size_t chunk_size, size_t current_offset, size_t height){

    //if we have hit the max height of our tree, return a NULL pointer 
    if(height == 0){
        return NULL;
    }

    hash_tree_node * root = malloc(sizeof(struct hash_tree_node));
    
    //puts("going left");
    root->left  = generate_hash_tree(heap, chunk_size/2, current_offset, height-1);
    //puts("going right");
    root->right = generate_hash_tree(heap, chunk_size/2, current_offset + (chunk_size/2), height-1);
    
    //read in a chunk
    char * chunk = malloc(chunk_size);
    memcpy(chunk, heap + current_offset, chunk_size);

    //hash chunk (we will update everything at once rather than in pieces)
    MD5_CTX md5_context;
    MD5_Init(&md5_context);
    MD5_Update(&md5_context, chunk, chunk_size);
    MD5_Final(root->hash, &md5_context);
    root->chunk_offset = current_offset;
    root->chunk_size = chunk_size;

    free(chunk);
    
    return root;
}

static uint8_t * load_heap_from_file(FILE * heap, size_t heap_start, size_t size){
    fseek(heap, heap_start, SEEK_SET);
    uint8_t * chunk = malloc(size);
    fread(chunk, 1, size, heap);
    return chunk;
}

static hash_tree_node * copy_hash_tree(hash_tree_node * root){
    if(root == NULL){
        return NULL;
    }
    
    hash_tree_node * temp = malloc(sizeof(struct hash_tree_node));
    memcpy(temp, root, sizeof(struct hash_tree_node));
    temp->left =copy_hash_tree(root->left);
    temp->right = copy_hash_tree(root->right);

    return temp;
}

static char * hash_to_string(unsigned char * hash){
    char * hash_string = calloc(1,32+1);

    for(int i=0; i<16;i++){
        sprintf(&hash_string[i*2], "%02x", hash[i]);
    }

    hash_string[32] = '\x00';
                    
    return hash_string;
}


//preorder tree traversal and printing
static void print_hash_tree(hash_tree_node * root, int n){
    if(root == NULL){
        return;
    }
    
    char * hash = hash_to_string(root->hash);
    printf("hash %s, height %d\n", hash, n);
    free(hash);

    print_hash_tree(root->left, n+1);
    print_hash_tree(root->right, n+1);
     
}

static void free_hash_tree_branch(hash_tree_node * root){
    if(root == NULL){
        return;
    }
    
    free_hash_tree_branch(root->left);
    free_hash_tree_branch(root->right);
    
    root->left = NULL;
    root->right = NULL;
    free(root);
}

//This is the next "major function" it traverses the existing hash tree
//and verfies that the hashes still match. If we encounter hash in the
//tree thats the same terminate that entire branch. Otherwise keep traversing
//until we hit a NULL terminated branch

static int diff_hash_tree(hash_tree_node * first_snapshot, hash_tree_node * second_snapshot){
    if((first_snapshot == NULL) || (second_snapshot == NULL)){
        return 0;
    }
    
    if(memcmp(first_snapshot->hash, second_snapshot->hash, 16) == 0){
        free_hash_tree_branch(second_snapshot);
        return 1;
    }
    
    if(diff_hash_tree(first_snapshot->left, second_snapshot->left) == 1){
        second_snapshot->left = NULL;
    }
    if(diff_hash_tree(first_snapshot->right, second_snapshot->right) == 1){
        second_snapshot->right = NULL;
    }
    return 0;
}


/**************************
 * start/stop functions   *
 **************************/

static void stop_and_wait(pid_t pid){
    int status;
    ptrace(PTRACE_ATTACH, pid, NULL, NULL);
    waitpid(pid, &status, 0);
}

static void countinue_stopped_process(pid_t pid){
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
    
    fseek(file, 0, SEEK_SET);

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
static void parse_proc_map_heap(char * line, proc_map_heap_info * heap_struct){
    sscanf(line, "%x-%x", &heap_struct->start_address, &heap_struct->end_address);
    heap_struct->size = heap_struct->end_address - heap_struct->start_address;
}

static void print_heap_info(proc_map_heap_info * heap_info){
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


/**************************
 * command line functions *
 **************************/
static void print_help(){
    puts("hashthesheap [ options ]");
    puts("  -p pid      - Process to analyze");
    puts("  -t          - Build hash tree");
    puts("  -i int      - Set hash tree height (defaults to 8 if left blank)");
    puts("  -d file     - Take a single snapshot of the heap and dump to a file");
    puts("  -h          - Print help screen");   
    puts("  -v          - verbose mode (prints hash tree)");   
}

int main(int argc, char * argv[]){
    
    if(geteuid() != 0){
        fprintf(stderr, "This program requires root.\n");
        exit(1);
    }
    
    flags f = {0};
    f.heap_tree_height = 8;

    int opt;
    while((opt = getopt(argc, argv, "p:ti:d:hv")) != -1){
        switch(opt){
            case 'p':
                f.process = (pid_t) atoi(optarg);
                break;
            case 't':
                f.build_heap_tree = 1;
                break;
            case 'i':
                f.heap_tree_height = (size_t) atoi(optarg);
                break;
            case 'd':
                f.dump_file_name = optarg;
                break;
            case 'h':
                print_help();
                exit(0);
                break;
            case 'v':
                f.verbose = 1;
                break;
            default:
                print_help();
                exit(0);

        }
    }
    
    if(optind == 1){
        print_help();
        exit(0);
    }

    if(f.process == 0){
        puts("Missing option -p (process ID)");
        exit(1);
    }
    

    //get maps and mem files from /proc
    char * proc_map_path;
    char * proc_mem_path;
    asprintf(&proc_map_path, "/proc/%d/maps", f.process);
    asprintf(&proc_mem_path, "/proc/%d/mem" , f.process); 
    FILE * proc_map_file = fopen(proc_map_path, "r");
    FILE * proc_mem_file = fopen(proc_mem_path, "r");
   
    
    //hash tree section 
    if(f.build_heap_tree == 1){
        char * proc_map_heap_line;
        
        hash_tree_node * first_hash_tree  = NULL;
        hash_tree_node * second_hash_tree = NULL;
        proc_map_heap_info heap_info_first  ={0};
        proc_map_heap_info heap_info_second ={0};
        uint8_t * first_chunk = NULL;
        uint8_t * second_chunk = NULL;

        //take the first snapshot of the process
        stop_and_wait(f.process);
            fflush(proc_map_file);
            proc_map_heap_line = proc_map_find_heap(proc_map_file);
            parse_proc_map_heap(proc_map_heap_line, &heap_info_first);
            first_chunk = load_heap_from_file(proc_mem_file, heap_info_first.start_address, heap_info_first.size);
            
            printf("%sSnapshot 1:%s\n", RED, NO_COLOR);
            print_heap_info(&heap_info_first);
            
            //unless we close the files we will get old data (there's probably a better solution to this)
            fclose(proc_mem_file);
            fclose(proc_map_file);
            free(proc_map_heap_line);
            proc_map_heap_line = NULL;
        countinue_stopped_process(f.process);
        
        //prompt user for second snapshot
        printf("\n%sPress enter to take second snapshot.%s\n", GREEN, NO_COLOR);
        getchar();

        //take the second snapshot of the process
        proc_map_file = fopen(proc_map_path, "r");
        proc_mem_file = fopen(proc_mem_path, "r");
        stop_and_wait(f.process);
            proc_map_heap_line = proc_map_find_heap(proc_map_file);
            parse_proc_map_heap(proc_map_heap_line, &heap_info_second);
            second_chunk = load_heap_from_file(proc_mem_file,heap_info_second.start_address, heap_info_second.size);

            printf("%sSnapshot 2:%s\n", RED, NO_COLOR);
            print_heap_info(&heap_info_second);
            
            free(proc_map_heap_line);
            proc_map_heap_line = NULL;
        countinue_stopped_process(f.process);       
        
        //If the first snapshot used less memory than the first resize the first chunk
        //so our hashing algorythem still works. The else if probably won't happen.
        if(heap_info_second.size > heap_info_first.size){
            heap_info_first.size = heap_info_second.size;
            first_chunk = realloc(first_chunk, heap_info_second.size);
        }
        else if(heap_info_second.size < heap_info_first.size){
            heap_info_second.size = heap_info_first.size;
            second_chunk = realloc(second_chunk, heap_info_first.size);
        }
        
        //generate the hash_trees
        first_hash_tree = generate_hash_tree(first_chunk, 
                                             heap_info_first.size, 
                                             0,
                                             f.heap_tree_height);
        free(first_chunk);
        first_chunk = NULL;

        second_hash_tree = generate_hash_tree(second_chunk, 
                                              heap_info_second.size, 
                                              0,
                                              f.heap_tree_height);
        free(second_chunk);
        second_chunk = NULL;
        
        
        int same_tree =  diff_hash_tree(first_hash_tree, second_hash_tree);

        //handle verbose hash printing
        if(f.verbose){
            printf("\n%sFIRST%s\n", YELLOW, NO_COLOR);
            print_hash_tree(first_hash_tree,0);
            
            printf("\n%sSECOND%s\n", YELLOW, NO_COLOR);
            print_hash_tree(second_hash_tree,0);

            //diff the hash trees and print the solution
            if(same_tree!=1){            
                printf("\n%sDIFF%s\n", YELLOW, NO_COLOR);
                print_hash_tree(second_hash_tree, 0);
            }else{
                second_hash_tree = NULL;
                puts("SAME TREE!!!");
            }
        }

        //start visualization
        draw_heap_visualization(second_hash_tree, heap_info_second.size, f.heap_tree_height);

    }        
    
    //dump a single heap snapshot to a file
    if(f.dump_file_name != NULL){ 
        proc_map_heap_info heap_info = {0};
        char * proc_map_heap_line = proc_map_find_heap(proc_map_file);
        parse_proc_map_heap(proc_map_heap_line, &heap_info);
        
        FILE * heap_dump_file = fopen(f.dump_file_name, "w");
        stop_and_wait(f.process);
        dump_to_file(proc_mem_file, heap_dump_file, &heap_info);
        countinue_stopped_process(f.process);     
    }

    return 0;
}

