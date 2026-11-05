#include "fault_cases.h"
int fault_case_invalid_action(struct protocol_fault_context *ctx)
{
    uint8_t request[FW_TRANSFER_HEADER_SIZE+128],data[128]; size_t length=0; fault_fill_synthetic_data(data,sizeof(data));
    if(fault_build_standard(request,&length,FW_ACTION_FULL,ctx->test_slot,0,data,sizeof(data))) return -1;
    request[0]=0xff;
    return fault_run_single(ctx,"invalid_action",request,length,0x02,"Unsupported action value 0xFF");
}