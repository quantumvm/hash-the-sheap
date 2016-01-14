#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int main(){
    char * string = "hello world"; 
    char * string_copy = strdup(string);
    
    printf("My pid is %d", (int) getpid());
    fflush(stdout);

    while(1){
        sleep(1);
    }
}
