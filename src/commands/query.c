#include "cxl_fw.h"
int query_commands(int fd)
{
    struct cxl_mem_query_commands q={0}; if(ioctl(fd,CXL_MEM_QUERY_COMMANDS,&q)<0) { perror("CXL_MEM_QUERY_COMMANDS count"); return -1; }
    size_t bytes=sizeof(q)+(size_t)q.n_commands*sizeof(struct cxl_command_info); struct cxl_mem_query_commands *r=calloc(1,bytes); if(!r) { perror("calloc"); return -1; } r->n_commands=q.n_commands;
    if(ioctl(fd,CXL_MEM_QUERY_COMMANDS,r)<0) { perror("CXL_MEM_QUERY_COMMANDS list"); free(r); return -1; }
    unsigned int empty=0; puts("Kernel command table entries:"); for(uint32_t i=0;i<r->n_commands;i++) { const struct cxl_command_info *c=&r->commands[i]; if(!c->id) { empty++; continue; } printf("  ID=%-2u flags=0x%08x input=%-8u output=%u\n",c->id,c->flags,c->size_in,c->size_out); }
    printf("Empty slots: %u\n",empty); free(r); return 0;
}