#include "cxl_fw.h"
int abort_fw_transfer_execute(void)
{
    char answer[32]={0}; fputs("WARNING: sends TRANSFER_FW ABORT to clear in-progress state. Type ABORT: ",stderr);
    if(!fgets(answer,sizeof(answer),stdin)||strcmp(answer,"ABORT\n")) { puts("Cancelled."); return -1; }
    int fd=open(CXL_DEVICE,O_RDWR|O_CLOEXEC); if(fd<0) { perror("open CXL device"); return -1; }
    struct fw_transfer_header req={.action=FW_ACTION_ABORT,.slot=2}; uint32_t result=UINT32_MAX; int rc=send_raw_command(fd,CXL_FW_TRANSFER_OPCODE,(uint8_t *)&req,sizeof(req),NULL,0,&result)<0||result!=0?-1:0;
    if (!rc)
        puts("TRANSFER_FW ABORT accepted; no firmware was activated.");
    if (close(fd)) rc = -1;
    return rc;
}