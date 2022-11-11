#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <sys/mman.h>

#include <inttypes.h>

int main(int argc, char **argv){

    uint64_t num = 0x1FF000000001;
        
    printf("%lld\n", num);

    num = num >> 63; 
    printf("%lld", num);


}