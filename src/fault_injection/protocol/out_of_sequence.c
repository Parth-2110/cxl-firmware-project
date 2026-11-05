#include "fault_cases.h"
int fault_case_out_of_sequence(struct protocol_fault_context *ctx)
{
    uint8_t request[FW_TRANSFER_HEADER_SIZE+128],data[128]; size_t length=0; fault_fill_synthetic_data(data,sizeof(data));
    if(fault_build_standard(request,&length,FW_ACTION_CONTINUE,ctx->test_slot,0,data,sizeof(data))) return -1;
    return fault_run_single(ctx,"out_of_sequence",request,length,0x02,"CONTINUE without preceding INITIATE");
}