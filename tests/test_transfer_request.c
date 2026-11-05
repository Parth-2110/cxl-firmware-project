#include "cxl_fw.h"
static int check_request(const char *name,enum fw_transfer_action action,unsigned int slot,uint32_t offset,const uint8_t *data,size_t n)
{
    uint8_t req[FW_TRANSFER_HEADER_SIZE+64]; size_t len=0; memset(req,0xaa,sizeof(req));
    if(build_fw_transfer_request(req,sizeof(req),&len,action,slot,offset,data,n)||len!=FW_TRANSFER_HEADER_SIZE+n||req[0]!=(uint8_t)action||req[1]!=(uint8_t)slot) return -1;
    uint32_t le=htole32(offset); if(memcmp(req+4,&le,4)) return -1; for(size_t i=8;i<128;i++) if(req[i]) return -1; if(n&&memcmp(req+128,data,n)) return -1; printf("PASS: %s\n",name); return 0;
}
int test_transfer_request(void)
{
    static const uint8_t data[]={0x10,0x21,0x32,0x43,0x54,0x65,0x76,0x87}; int fail=0;
    puts("CXL TRANSFER_FW request tests (local only)"); fail+=check_request("FULL",FW_ACTION_FULL,2,0,data,sizeof(data))!=0; fail+=check_request("INITIATE",FW_ACTION_INITIATE,2,0,data,sizeof(data))!=0; fail+=check_request("CONTINUE",FW_ACTION_CONTINUE,2,15,data,sizeof(data))!=0; fail+=check_request("END",FW_ACTION_END,2,30,data,sizeof(data))!=0; fail+=check_request("ABORT",FW_ACTION_ABORT,0,0,NULL,0)!=0;
    uint8_t b[128]; size_t n=0; if(build_fw_transfer_request(b,sizeof(b),&n,FW_ACTION_FULL,2,0,data,sizeof(data))==0) fail++; if(build_fw_transfer_request(b,sizeof(b),&n,FW_ACTION_FULL,2,0,NULL,8)==0) fail++; if(build_fw_transfer_request(b,sizeof(b),&n,FW_ACTION_FULL,0,0,NULL,0)==0) fail++;
    if(fail) fprintf(stderr,"%d request test(s) failed.\n",fail); else puts("All request tests passed; no device or ioctl used."); return fail?-1:0;
}