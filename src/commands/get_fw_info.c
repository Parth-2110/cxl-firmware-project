#include "cxl_fw.h"
static void print_revision(unsigned int slot,const char revision[16])
{ size_t n=16; while(n&&(revision[n-1]=='\0'||revision[n-1]==' ')) n--; printf("  Slot %u revision: ",slot); if(!n) puts("(empty)"); else printf("%.*s\n",(int)n,revision); }
int get_fw_info(int fd)
{
    struct fw_info info={0}; struct cxl_send_command cmd={.id=CXL_MEM_COMMAND_ID_GET_FW_INFO,.out={.size=sizeof(info),.payload=(uint64_t)(uintptr_t)&info}};
    if(ioctl(fd,CXL_MEM_SEND_COMMAND,&cmd)<0) { perror("CXL_MEM_SEND_COMMAND (GET_FW_INFO)"); return -1; }
    printf("GET_FW_INFO retval=0x%08x response=%u bytes\n",cmd.retval,cmd.out.size);
    if(cmd.retval||cmd.out.size!=sizeof(info)) return -1;
    printf("Number of slots: %u\nCurrent slot: %u\nNext slot: %u\nLive activation supported: %s\n",info.num_slots,info.slot_info&7U,(info.slot_info>>3)&7U,(info.activation_cap&1)?"yes":"no");
    for (unsigned int i = 0; i < 4; i++)
        print_revision(i + 1, info.slot_revision[i]);
    return 0;
}
int read_fw_info_for_transfer(int fd,struct fw_info *info)
{
    struct cxl_send_command cmd={0}; memset(info,0,sizeof(*info)); cmd.id=CXL_MEM_COMMAND_ID_GET_FW_INFO; cmd.out.size=sizeof(*info); cmd.out.payload=(uint64_t)(uintptr_t)info;
    if(ioctl(fd,CXL_MEM_SEND_COMMAND,&cmd)<0) { perror("GET_FW_INFO before transfer"); return -1; }
    if(cmd.retval!=CXL_MBOX_SUCCESS||cmd.out.size!=sizeof(*info)) { fprintf(stderr,"GET_FW_INFO failed retval=0x%x size=%u\n",cmd.retval,cmd.out.size); return -1; } return 0;
}