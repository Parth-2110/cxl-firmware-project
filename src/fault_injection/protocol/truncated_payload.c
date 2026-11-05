#include "fault_cases.h"
int fault_case_truncated_payload(struct protocol_fault_context *ctx)
{
    uint8_t request[FW_TRANSFER_HEADER_SIZE+128],data[128]; size_t length=0; fault_fill_synthetic_data(data,sizeof(data));
    if(fault_build_standard(request,&length,FW_ACTION_INITIATE,ctx->test_slot,0,data,sizeof(data)/2)) return -1;
    return fault_run_single(ctx,"truncated_payload",request,length,CXL_MBOX_SUCCESS,"Complete header with only 64 data bytes");
}