#include "fault_cases.h"
#define MAX_FW_BYTES (32U*1024U*1024U)
int fault_case_out_of_range_offset(struct protocol_fault_context *ctx)
{
    uint8_t request[FW_TRANSFER_HEADER_SIZE+128],data[128]; size_t length=0; fault_fill_synthetic_data(data,sizeof(data));
    if(fault_build_standard(request,&length,FW_ACTION_INITIATE,ctx->test_slot,MAX_FW_BYTES/128,data,sizeof(data))) return -1;
    return fault_run_single(ctx,"out_of_range_offset",request,length,0x02,"Offset plus data exceeds 32 MiB limit");
}