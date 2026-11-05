#include "cxl_fw.h"

int fw_transfer_submit_ioctl(void *context,const uint8_t *request,size_t request_size,uint32_t *mailbox_return)
{
    int fd=*(int *)context;
    return send_raw_command(fd,CXL_FW_TRANSFER_OPCODE,(uint8_t *)request,request_size,NULL,0,mailbox_return);
}

static int submit_request(struct fw_transfer_engine *engine,uint8_t *request,size_t *request_size,uint64_t part)
{
    uint32_t result=UINT32_MAX;
    if(fw_transfer_run_prepared(engine,request,*request_size,part,&result)!=0) return -1;
    printf("Part %" PRIu64 ": request=%zu bytes mailbox_return=0x%02" PRIx32 "\n",part+1,*request_size,result);
    return 0;
}

int fw_transfer_run_prepared(struct fw_transfer_engine *engine,uint8_t *request,size_t request_size,uint64_t part,uint32_t *mailbox_return)
{
    if(!engine || !engine->submit || !request || request_size>engine->payload_capacity || request_size<FW_TRANSFER_HEADER_SIZE) return -1;
    if(engine->hook && engine->hook(request,&request_size,engine->payload_capacity,part,engine->hook_context)!=0) return -1;
    if(request_size>engine->payload_capacity || request_size<FW_TRANSFER_HEADER_SIZE) return -1;
    uint32_t result=UINT32_MAX;
    int rc=engine->submit(engine->submit_context,request,request_size,&result);
    if(rc<0) return -1;
    if(mailbox_return) *mailbox_return=result;
    if(engine->require_success && result!=CXL_MBOX_SUCCESS) return -1;
    return 0;
}

int fw_transfer_run_image(struct fw_transfer_engine *engine,FILE *firmware,uint64_t image_size)
{
    if(!engine || !engine->submit || !firmware || engine->payload_capacity<2*FW_TRANSFER_HEADER_SIZE || engine->payload_capacity%FW_TRANSFER_ALIGNMENT) return -1;
    size_t max_chunk=((engine->payload_capacity-FW_TRANSFER_HEADER_SIZE)/FW_TRANSFER_ALIGNMENT)*FW_TRANSFER_ALIGNMENT;
    uint8_t *request=malloc(engine->payload_capacity); uint8_t *chunk=malloc(max_chunk);
    if(!request || !chunk) { perror("allocate transfer buffers"); free(request); free(chunk); return -1; }
    uint64_t offset=0,part=0; int rc=0;
    while(offset<image_size) {
        uint64_t remaining=image_size-offset; size_t amount=remaining>max_chunk?max_chunk:(size_t)remaining;
        enum fw_transfer_action action=image_size<=max_chunk?FW_ACTION_FULL:(offset==0?FW_ACTION_INITIATE:((uint64_t)amount==remaining?FW_ACTION_END:FW_ACTION_CONTINUE));
        if(fread(chunk,1,amount,firmware)!=amount) { fprintf(stderr,"Could not read firmware chunk at byte offset %" PRIu64 ".\n",offset); rc=-1; break; }
        size_t request_size=0;
        if(build_fw_transfer_request(request,engine->payload_capacity,&request_size,action,engine->slot,(uint32_t)(offset/FW_TRANSFER_ALIGNMENT),chunk,amount)!=0 || submit_request(engine,request,&request_size,part)!=0) { rc=-1; break; }
        offset+=amount; part++;
    }
    if(rc==0) { int extra=fgetc(firmware); if(extra!=EOF || ferror(firmware)) { fprintf(stderr,"Firmware file changed or could not be read completely.\n"); rc=-1; } }
    free(request); free(chunk);
    if(rc==0) printf("TRANSFER_FW sequence completed for %" PRIu64 " bytes. Firmware was NOT activated.\n",image_size);
    return rc;
}