#include "cxl_fw.h"

int transfer_fw_dry_run(int argc,char **argv)
{
    const char *file_path=NULL; unsigned int slot=0,capacity=0,size_arg=0; int have_size=0;
    for(int i=2;i<argc;i++) {
        if(!strcmp(argv[i],"--dry-run")) continue;
        else if(!strcmp(argv[i],"--file") && i+1<argc) { if(file_path||have_size) return -1; file_path=argv[++i]; }
        else if(!strcmp(argv[i],"--size") && i+1<argc) { if(file_path||have_size||parse_uint(argv[++i],&size_arg)<0) return -1; have_size=1; }
        else if(!strcmp(argv[i],"--slot") && i+1<argc) { if(parse_uint(argv[++i],&slot)<0||slot>UINT8_MAX) return -1; }
        else if(!strcmp(argv[i],"--payload-capacity") && i+1<argc) { if(parse_uint(argv[++i],&capacity)<0) return -1; }
        else { fprintf(stderr,"Unknown/incomplete option: %s\n",argv[i]); return -1; }
    }
    if((!file_path&&!have_size)||!slot||!capacity||capacity<256||capacity%128) { fprintf(stderr,"Dry run requires --file PATH or --size BYTES, --slot N, --payload-capacity BYTES and --dry-run.\n"); return -1; }
    uint64_t image=size_arg;
    if(file_path) { FILE *f=fopen(file_path,"rb"); if(!f) { perror("open firmware"); return -1; } if(fseeko(f,0,SEEK_END)) { fclose(f); return -1; } off_t n=ftello(f); fclose(f); if(n<=0) return -1; image=(uint64_t)n; }
    if(!image||image>DRY_RUN_MAX_IMAGE_SIZE||image%128) { fprintf(stderr,"Image size must be nonzero, <=32 MiB and a multiple of 128 bytes.\n"); return -1; }
    unsigned int chunk=((capacity-128)/128)*128; uint64_t off=0; unsigned int part=0;
    puts("CXL firmware transfer planner — DRY RUN"); printf("Input: %s\nImage size: %" PRIu64 " bytes\nSlot: %u\nCapacity: %u\nChunk: %u\n",file_path?file_path:"size-only",image,slot,capacity,chunk);
    while(off<image) { uint64_t left=image-off; unsigned int n=left>chunk?chunk:(unsigned int)left; const char *a=image<=chunk?"FULL":(off==0?"INITIATE":((uint64_t)n==left?"END":"CONTINUE")); printf("%u. %-8s offset_units=%" PRIu64 " data=%u\n",++part,a,off/128,n); off+=n; }
    puts("No device was opened and no ioctl was issued."); return 0;
}

int read_payload_max(unsigned int *out)
{
    FILE *f=fopen("/sys/bus/cxl/devices/mem0/payload_max","r"); if(!f) { perror("open payload_max"); return -1; }
    unsigned int v=0; int ok=fscanf(f,"%u",&v)==1; int close_rc=fclose(f); if(!ok||close_rc||!v) return -1; *out=v; return 0;
}

int transfer_fw_execute(int argc,char **argv)
{
    const char *path=NULL; unsigned int slot=0,capacity=0; int execute=0;
    for(int i=2;i<argc;i++) {
        if(!strcmp(argv[i],"--execute")) execute=1;
        else if(!strcmp(argv[i],"--file")&&i+1<argc) path=argv[++i];
        else if(!strcmp(argv[i],"--slot")&&i+1<argc) { if(parse_uint(argv[++i],&slot)<0||slot>UINT8_MAX) return -1; }
        else if(!strcmp(argv[i],"--payload-capacity")&&i+1<argc) { if(parse_uint(argv[++i],&capacity)<0) return -1; }
        else { fprintf(stderr,"Unknown/incomplete option: %s\n",argv[i]); return -1; }
    }
    if(!execute||!path||!slot||!capacity) { fprintf(stderr,"Execution requires --file PATH --slot N --payload-capacity BYTES --execute.\n"); return -1; }
    FILE *firmware=fopen(path,"rb"); if(!firmware) { perror("open firmware image"); return -1; }
    if(fseeko(firmware,0,SEEK_END)) { perror("seek firmware"); fclose(firmware); return -1; }
    off_t signed_size=ftello(firmware); if(signed_size<=0) { fclose(firmware); return -1; }
    uint64_t image_size=(uint64_t)signed_size;
    if(image_size>FW_TRANSFER_MAX_IMAGE_SIZE||image_size%128||fseeko(firmware,0,SEEK_SET)) { fprintf(stderr,"Image must be <=32 MiB and a multiple of 128 bytes.\n"); fclose(firmware); return -1; }
    unsigned int device_capacity=0; if(read_payload_max(&device_capacity)||capacity>device_capacity||capacity<256||capacity%128) { fprintf(stderr,"Invalid requested payload capacity (device maximum %u).\n",device_capacity); fclose(firmware); return -1; }
    int fd=open(CXL_DEVICE,O_RDWR|O_CLOEXEC); if(fd<0) { perror("open CXL device"); fclose(firmware); return -1; }
    struct fw_info info; if(read_fw_info_for_transfer(fd,&info)) { close(fd); fclose(firmware); return -1; }
    unsigned int active=info.slot_info&7U;
    if(slot>info.num_slots||slot>4||slot==active) { fprintf(stderr,"Target slot %u is invalid or active (active=%u slots=%u).\n",slot,active,info.num_slots); close(fd); fclose(firmware); return -1; }
    printf("REAL CXL TRANSFER\nDevice: %s\nImage: %s (%" PRIu64 " bytes)\nTarget slot: %u; active: %u\nPayload capacity: %u\n",CXL_DEVICE,path,image_size,slot,active,capacity);
    fputs("WARNING: writes firmware to the selected inactive slot; does not activate it. Type TRANSFER to continue: ",stderr);
    char confirm[32]={0}; if(!fgets(confirm,sizeof(confirm),stdin)||strcmp(confirm,"TRANSFER\n")) { puts("Cancelled; no transfer issued."); close(fd); fclose(firmware); return -1; }
    struct fw_transfer_engine engine={.slot=slot,.payload_capacity=capacity,.submit=fw_transfer_submit_ioctl,.submit_context=&fd,.require_success=1};
    int rc=fw_transfer_run_image(&engine,firmware,image_size);
    if(rc) fprintf(stderr,"Transfer failed. Target slot may be incomplete; do not activate it.\n");
    if(close(fd)) { perror("close device"); rc=-1; } if(fclose(firmware)) { perror("close firmware"); rc=-1; }
    return rc;
}