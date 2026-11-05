#include "fault_cases.h"
int fault_case_skipped_offset(struct protocol_fault_context *ctx)
{ return fault_run_sequence(ctx,"skipped_offset",2,"Advance beyond previous part leaving a gap; current QEMU accepts it"); }