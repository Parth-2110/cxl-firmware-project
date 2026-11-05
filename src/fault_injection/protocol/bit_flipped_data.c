#include "fault_cases.h"
int fault_case_bit_flipped_data(struct protocol_fault_context *ctx)
{
    uint8_t request[FW_TRANSFER_HEADER_SIZE+128],data[128]; size_t length=0; fault_fill_synthetic_data(data,sizeof(data));
    if(fault_build_standard(request,&length,FW_ACTION_INITIATE,ctx->test_slot,0,data,sizeof(data))) return -1;
    request[FW_TRANSFER_HEADER_SIZE]^=0x01;
    return fault_run_single(ctx,"bit_flipped_data",request,length,CXL_MBOX_SUCCESS,"Flip one data bit; QEMU does not validate image contents");
}
