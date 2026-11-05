#include "cxl_fw.h"
#include "fault_cases.h"
#include "fault_log.h"
#include <time.h>

static int dry_run(void)
{
    puts("CXL TRANSFER_FW protocol fault-injection plan (DRY RUN)");
    puts("1. invalid_action       action=0xFF                  expected 0x02");
    puts("2. invalid_slot         FULL using active slot        expected 0x0B");
    puts("3. out_of_sequence      CONTINUE without INITIATE     expected 0x02");
    puts("4. duplicate_offset     INITIATE then repeated offset expected 0x00");
    puts("5. skipped_offset       INITIATE then offset gap      expected 0x00");
    puts("6. out_of_range_offset  beyond 32 MiB                 expected 0x02");
    puts("7. bit_flipped_data     first data byte modified      expected 0x00");
    puts("8. truncated_payload    64-byte data section          expected 0x00");
    puts("9. corrupted_reserved   reserved bytes nonzero        expected 0x00");
    puts("No device is opened and no ioctl is issued."); return 0;
}
int fault_inject_execute(int argc,char **argv)
{
    const char *log_path=DEFAULT_FAULT_LOG_PATH; int all=0,dry=0;
    for(int i=2;i<argc;i++) {
        if(!strcmp(argv[i],"--all")) all=1;
        else if(!strcmp(argv[i],"--dry-run")) dry=1;
        else if(!strcmp(argv[i],"--log")&&i+1<argc) log_path=argv[++i];
        else { fprintf(stderr,"Unknown/incomplete fault-inject option: %s\n",argv[i]); return -1; }
    }
    if(!all) { fprintf(stderr,"Specify fault-inject --all.\n"); return -1; }
    if(dry) return dry_run();
    int fd=open(CXL_DEVICE,O_RDWR|O_CLOEXEC); if(fd<0) { perror("open CXL device"); return -1; }
    struct fw_info info; if(read_fw_info_for_transfer(fd,&info)) { close(fd); return -1; }
    unsigned int active=info.slot_info&7U,slot=0;
    for(unsigned int i=1;i<=info.num_slots;i++) if(i!=active) { slot=i; break; }
    if(!slot) { fprintf(stderr,"No inactive slot available.\n"); close(fd); return -1; }
    unsigned int max=0; if(read_payload_max(&max)||max<256) { fprintf(stderr,"CXL payload_max is unavailable or too small.\n"); close(fd); return -1; }
    time_t now=time(NULL); struct tm tm; localtime_r(&now,&tm); char timestamp[32],run_id[64]; strftime(timestamp,sizeof(timestamp),"%Y-%m-%dT%H:%M:%S%z",&tm); snprintf(run_id,sizeof(run_id),"%ld-%ld",(long)now,(long)getpid());
    if(fault_log_open(log_path,run_id,timestamp)) { close(fd); return -1; }
    printf("PROTOCOL FAULT-INJECTION RUN %s\nDevice: %s\nActive slot: %u; test slot: %u\nCSV: %s\n",run_id,CXL_DEVICE,active,slot,log_path);
    fputs("WARNING: sends deliberately malformed TRANSFER_FW requests. Type INJECT to continue: ",stderr);
    char confirmation[32]={0}; if(!fgets(confirmation,sizeof(confirmation),stdin)||strcmp(confirmation,"INJECT\n")) { puts("Cancelled; no requests sent."); fault_log_close(); close(fd); return -1; }
    struct protocol_fault_context ctx={.fd=fd,.active_slot=active,.test_slot=slot};
    int rc=protocol_fault_run_all(&ctx);
    fault_log_close(); if(close(fd)) { perror("close CXL device"); rc=-1; }
    puts("No firmware activation was performed."); return rc;
}