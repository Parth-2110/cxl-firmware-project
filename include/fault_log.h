#ifndef CXL_FAULT_LOG_H
#define CXL_FAULT_LOG_H
#include "cxl_fw.h"
int fault_log_open(const char*,const char*,const char*);
void fault_log_event(const char*,const char*,uint32_t,const char*,const char*,const char*,const char*);
void fault_log_close(void);
#endif