#include "fault_cases.h"
#include "fault_log.h"

#define DATA_SIZE 128U
#define REQUEST_CAPACITY (FW_TRANSFER_HEADER_SIZE + DATA_SIZE)
static uint8_t synthetic_data[DATA_SIZE];

static int engine_submit(void *opaque,const uint8_t *request,size_t length,uint32_t *actual)
{
    struct protocol_fault_context *ctx=opaque;
    return send_raw_command(ctx->fd,CXL_FW_TRANSFER_OPCODE,(uint8_t *)request,length,NULL,0,actual);
}

int fault_build_standard(uint8_t *request,size_t *length,enum fw_transfer_action action,unsigned int slot,uint32_t offset,const uint8_t *data,size_t data_size)
{
    return build_fw_transfer_request(request,REQUEST_CAPACITY,length,action,slot,offset,data,data_size);
}

int fault_submit_checked(struct protocol_fault_context *ctx,const char *test,const char *step,const uint8_t *request,size_t length,uint32_t expected,const char *note)
{
    struct fw_transfer_engine engine={.slot=ctx->test_slot,.payload_capacity=REQUEST_CAPACITY,.submit=engine_submit,.submit_context=ctx,.require_success=0};
    uint8_t copy[REQUEST_CAPACITY];
    if(length>sizeof(copy)) return -1;
    memcpy(copy,request,length);
    uint32_t actual=UINT32_MAX;
    if(fw_transfer_run_prepared(&engine,copy,length,0,&actual)<0) {
        fault_log_event(test,step,expected,"IOCTL_ERROR","ERROR","FAIL",note);
        printf("  %-18s ioctl failed\n",step);
        return -1;
    }
    char value[32]; snprintf(value,sizeof(value),"0x%02" PRIx32,actual);
    int pass=actual==expected;
    fault_log_event(test,step,expected,value,"OK",pass?"PASS":"FAIL",note);
    printf("  %-18s expected=0x%02" PRIx32 " actual=%s %s\n",step,expected,value,pass?"PASS":"FAIL");
    return pass?0:-1;
}

int fault_abort_cleanup(struct protocol_fault_context *ctx,const char *name)
{
    uint8_t request[FW_TRANSFER_HEADER_SIZE]={0}; request[0]=FW_ACTION_ABORT;
    return fault_submit_checked(ctx,name,"cleanup_abort",request,sizeof(request),CXL_MBOX_SUCCESS,"Clear in-progress transfer state");
}

int fault_run_single(struct protocol_fault_context *ctx,const char *name,const uint8_t *request,size_t length,uint32_t expected,const char *note)
{
    printf("\n[%s]\n",name);
    int test_rc=fault_submit_checked(ctx,name,"fault_request",request,length,expected,note);
    int cleanup_rc=fault_abort_cleanup(ctx,name);
    return (test_rc==0&&cleanup_rc==0)?0:-1;
}

int fault_run_sequence(struct protocol_fault_context *ctx,const char *name,uint32_t next_offset,const char *note)
{
    uint8_t request[REQUEST_CAPACITY]; size_t length=0;
    printf("\n[%s]\n",name);
    for(size_t i=0;i<DATA_SIZE;i++) synthetic_data[i]=(uint8_t)(0xA5U^(uint8_t)i);
    int rc=fault_build_standard(request,&length,FW_ACTION_INITIATE,ctx->test_slot,0,synthetic_data,DATA_SIZE);
    if(rc==0) rc=fault_submit_checked(ctx,name,"baseline_initiate",request,length,CXL_MBOX_SUCCESS,"Establish valid transfer state");
    if(rc==0) rc=fault_build_standard(request,&length,FW_ACTION_CONTINUE,ctx->test_slot,next_offset,synthetic_data,DATA_SIZE);
    if(rc==0) rc=fault_submit_checked(ctx,name,"fault_continue",request,length,CXL_MBOX_SUCCESS,note);
    if(fault_abort_cleanup(ctx,name)!=0) rc=-1;
    return rc;
}

void fault_fill_synthetic_data(uint8_t *data,size_t size)
{
    for(size_t i=0;i<size;i++) data[i]=(uint8_t)(0xA5U^(uint8_t)i);
}