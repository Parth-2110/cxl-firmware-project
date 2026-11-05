#include "fault_cases.h"
int fault_case_corrupted_reserved_fields(struct protocol_fault_context *ctx)
{
    uint8_t request[FW_TRANSFER_HEADER_SIZE+128],data[128]; size_t length=0; fault_fill_synthetic_data(data,sizeof(data));
    if(fault_build_standard(request,&length,FW_ACTION_INITIATE,ctx->test_slot,0,data,sizeof(data))) return -1;
    request[2]=0xa5; request[8]=0x5a;
    return fault_run_single(ctx,"corrupted_reserved_fields",request,length,CXL_MBOX_SUCCESS,"Reserved bytes 2 and 8 are nonzero");
}
