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

/*
 * This program is designed to support 32-bit dumps
 *
 */

typedef struct flags{
    pid_t process;
    char * dump_file_name;
    int build_heap_tree;
    size_t heap_tree_height;
}flags;

typedef struct proc_map_heap_info{
    uint32_t start_address;
    uint32_t end_address;
    uint32_t size;
}proc_map_heap_info;

typedef struct hash_tree_node{
    uint8_t hash[16];
    struct hash_tree_node * left;
    struct hash_tree_node * right;
}hash_tree_node;


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
static hash_tree_node * generate_hash_tree(FILE * heap, size_t chunk_size, size_t current_offset, size_t height){
    
    //if we have hit the max height of our tree, return a NULL pointer 
    if(height == 0){
        return NULL;
    }

    hash_tree_node * root = malloc(sizeof(struct hash_tree_node));
    
    root->left  = generate_hash_tree(heap, chunk_size/2, current_offset, height-1);
    root->right = generate_hash_tree(heap, chunk_size/2, current_offset + (chunk_size/2), height-1);
    
    //read in a chunk
    fseek(heap, current_offset, SEEK_SET);
    uint8_t * chunk = malloc(chunk_size);
    fread(chunk, 1, chunk_size, heap);
    
    //hash chunk (we will update everything at once rather than in pieces)
    MD5_CTX md5_context;
    MD5_Init(&md5_context);
    MD5_Update(&md5_context, chunk, chunk_size);
    MD5_Final(root->hash, &md5_context);
    
    free(chunk);

    return root;
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
void print_hash_tree(hash_tree_node * root, int n){
    if(root == NULL){
        return;
    }
    
    char * hash = hash_to_string(root->hash);
    printf("hash %s, height %d\n", hash, n);
    free(hash);

    print_hash_tree(root->left, n+1);
    print_hash_tree(root->right, n+1);
     
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


/**************************
 * command line functions *
 **************************/
static void print_help(){
    puts("hashthesheap [ options ]");
    puts("  -p pid      - Process to analyze");
    puts("  -t          - Build hash tree");
    puts("  -i int      - Set hash tree height (defaults to 8 if left blank)");
    puts("  -d file     - Dump heap to file ");
    puts("  -h          - Print help screen");   
}

int main(int argc, char * argv[]){
    
    if(geteuid() != 0){
        fprintf(stderr, "This program requires root.\n");
        exit(1);
    }
    
    flags f = {0};
    f.heap_tree_height = 8;

    int opt;
    while((opt = getopt(argc, argv, "p:ti:d:h")) != -1){
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
   
    //parse the proc maps file
    char * proc_map_heap_line = proc_map_find_heap(proc_map_file);

    proc_map_heap_info heap_info;
    parse_proc_map_heap(proc_map_heap_line, &heap_info);  
    print_heap_info(&heap_info); 
    
    //create a hash tree 
    if(f.build_heap_tree == 1){
        stop_and_wait((pid_t) atoi(argv[1]));
        hash_tree_node * hash_tree_root = NULL; 
        hash_tree_root = generate_hash_tree( proc_mem_file, 
                                             heap_info.size, 
                                             heap_info.start_address,
                                             f.heap_tree_height);
        
        countinue_stopped_process((pid_t) atoi(argv[1]));
        print_hash_tree(hash_tree_root, 0);
    }        
    
    if(f.dump_file_name != NULL){ 
        FILE * heap_dump_file = fopen(f.dump_file_name, "w");
        stop_and_wait((pid_t) atoi(argv[1]));
        dump_to_file(proc_mem_file, heap_dump_file, &heap_info);
        countinue_stopped_process((pid_t) atoi(argv[1]));     
    }

    return 0;
}

