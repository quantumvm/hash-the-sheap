#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int main(){
   
    printf("My pid is %d", (int) getpid());
    fflush(stdout);
    
    getchar();
    char * string = "hello world"; 
    char * string_copy = strdup(string);
    puts(string);

    getchar();
    char * string_2 = "goodbye world"; 
    char * string_copy_2 = strdup(string_2);
    puts(string_2);

    while(1){
        sleep(1);
    }
}
