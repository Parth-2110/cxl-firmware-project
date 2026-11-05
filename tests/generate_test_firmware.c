#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc,char **argv)
{
    const char *path=argc>1?argv[1]:"firmware/synthetic-test-8k.bin"; size_t size=argc>2?(size_t)strtoull(argv[2],NULL,10):8192;
    if(argc>3||size==0||size>32U*1024U*1024U||size%128) { fprintf(stderr,"Usage: %s [output-path] [size-multiple-of-128]\n",argv[0]); return 2; }
    FILE *f=fopen(path,"wb"); if(!f) { perror("fopen"); return 1; }
    for(size_t i=0;i<size;i++) { uint8_t byte=(uint8_t)((i*37U+0x5aU)&0xffU); if(fwrite(&byte,1,1,f)!=1) { perror("write"); fclose(f); return 1; } }
    if(fclose(f)) { perror("close"); return 1; } printf("Generated %s (%zu bytes)\n",path,size); return 0;
}