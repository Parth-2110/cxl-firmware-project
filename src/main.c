#include "cxl_fw.h"
void usage(const char *p)
{
    fprintf(stderr,"Usage:\n  %s query\n  %s get-fw-info\n  %s abort-fw-transfer\n  %s transfer-fw --file PATH --slot N --payload-capacity BYTES --dry-run|--execute\n  %s fault-inject --all [--dry-run] [--log PATH]\n  %s test-header|test-transfer-request\n",p,p,p,p,p,p);
}
int main(int argc,char **argv)
{
    if(argc==2&&!strcmp(argv[1],"test-header")) return test_transfer_header()?1:0;
    if(argc==2&&!strcmp(argv[1],"test-transfer-request")) return test_transfer_request()?1:0;
    if(argc==2&&!strcmp(argv[1],"abort-fw-transfer")) return abort_fw_transfer_execute()?1:0;
    if(argc>=2&&!strcmp(argv[1],"fault-inject")) return fault_inject_execute(argc,argv)?1:0;
    if(argc>=2&&!strcmp(argv[1],"transfer-fw")) { int dry=0,execute=0; for(int i=2;i<argc;i++) { if(!strcmp(argv[i],"--dry-run")) dry=1; if(!strcmp(argv[i],"--execute")) execute=1; } if(dry==execute) { fprintf(stderr,"Choose exactly one of --dry-run or --execute.\n"); return 1; } return (dry?transfer_fw_dry_run(argc,argv):transfer_fw_execute(argc,argv))?1:0; }
    if(argc!=2||(strcmp(argv[1],"query")&&strcmp(argv[1],"get-fw-info"))) { usage(argv[0]); return 1; }
    int fd=open(CXL_DEVICE,O_RDWR|O_CLOEXEC); if(fd<0) { perror("open CXL device"); return 1; } int rc=!strcmp(argv[1],"query")?query_commands(fd):get_fw_info(fd); if(close(fd)) rc=-1; return rc?1:0;
}