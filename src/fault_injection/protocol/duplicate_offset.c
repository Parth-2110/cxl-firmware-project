#include "fault_cases.h"
int fault_case_duplicate_offset(struct protocol_fault_context *ctx)
{ return fault_run_sequence(ctx,"duplicate_offset",0,"Repeat the previous offset and payload length"); }
