#include "cxl_fw.h"
static int check(const char *name,enum fw_transfer_action action,unsigned int slot,uint32_t offset)
{
    uint8_t b[FW_TRANSFER_HEADER_SIZE]; if(build_fw_transfer_header(b,sizeof(b),action,slot,offset)) return -1; uint32_t le=htole32(offset);
    if(b[0]!=(uint8_t)action||b[1]!=(uint8_t)slot||b[2]||b[3]||memcmp(b+4,&le,4)) return -1;
    for (size_t i = 8; i < sizeof(b); i++)
        if (b[i]) return -1;
    printf("PASS: %s\n", name);
    return 0;
}
int test_transfer_header(void)
{
    int fail=0; puts("CXL TRANSFER_FW header tests (local only)"); fail+=check("FULL",FW_ACTION_FULL,2,0)!=0; fail+=check("INITIATE",FW_ACTION_INITIATE,2,0)!=0; fail+=check("CONTINUE",FW_ACTION_CONTINUE,2,15)!=0; fail+=check("END",FW_ACTION_END,2,30)!=0; fail+=check("ABORT",FW_ACTION_ABORT,0,0)!=0;
    if(fail) fprintf(stderr,"%d header test(s) failed.\n",fail); else puts("All header tests passed; no device or ioctl used."); return fail?-1:0;
}