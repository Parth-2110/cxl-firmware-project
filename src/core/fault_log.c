#include "cxl_fw.h"
#include <sys/stat.h>

static FILE *log_file;
static char active_run_id[64];
static const char *active_timestamp;

static void csv_field(FILE *f,const char *s)
{
    fputc('"',f); for(;*s;s++) { if(*s=='"') fputc('"',f); fputc(*s,f); } fputc('"',f);
}
int fault_log_open(const char *path,const char *run_id,const char *timestamp)
{
    if(mkdir("logs",0755)<0 && errno!=EEXIST) { perror("mkdir logs"); return -1; }
    log_file=fopen(path,"a+"); if(!log_file) { perror("open fault log"); return -1; }
    if(fseek(log_file,0,SEEK_END)!=0) { fclose(log_file); log_file=NULL; return -1; }
    if(ftell(log_file)==0) fputs("run_id,timestamp,test,step,expected_mailbox_return,actual_mailbox_return,ioctl_status,verdict,note\n",log_file);
    snprintf(active_run_id,sizeof(active_run_id),"%s",run_id); active_timestamp=timestamp;
    fflush(log_file); return 0;
}
void fault_log_event(const char *test,const char *step,uint32_t expected,const char *actual,const char *ioctl_status,const char *verdict,const char *note)
{
    if(!log_file) return;
    csv_field(log_file,active_run_id); fputc(',',log_file); csv_field(log_file,active_timestamp); fputc(',',log_file); csv_field(log_file,test); fputc(',',log_file); csv_field(log_file,step); fprintf(log_file,",0x%02" PRIx32 ",",expected); csv_field(log_file,actual); fputc(',',log_file); csv_field(log_file,ioctl_status); fputc(',',log_file); csv_field(log_file,verdict); fputc(',',log_file); csv_field(log_file,note); fputc('\n',log_file); fflush(log_file);
}
void fault_log_close(void) { if(log_file) fclose(log_file); log_file=NULL; }