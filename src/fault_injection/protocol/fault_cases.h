#ifndef CXL_FAULT_CASES_H
#define CXL_FAULT_CASES_H
#include "cxl_fw.h"
struct protocol_fault_context { int fd; unsigned int active_slot; unsigned int test_slot; };
typedef int (*protocol_fault_fn)(struct protocol_fault_context *);
void fault_fill_synthetic_data(uint8_t *,size_t);
int fault_build_standard(uint8_t *,size_t *,enum fw_transfer_action,unsigned int,uint32_t,const uint8_t *,size_t);
int fault_submit_checked(struct protocol_fault_context *,const char *,const char *,const uint8_t *,size_t,uint32_t,const char *);
int fault_abort_cleanup(struct protocol_fault_context *,const char *);
int fault_run_single(struct protocol_fault_context *,const char *,const uint8_t *,size_t,uint32_t,const char *);
int fault_run_sequence(struct protocol_fault_context *,const char *,uint32_t,const char *);
int fault_case_invalid_action(struct protocol_fault_context *);
int fault_case_invalid_slot(struct protocol_fault_context *);
int fault_case_out_of_sequence(struct protocol_fault_context *);
int fault_case_duplicate_offset(struct protocol_fault_context *);
int fault_case_skipped_offset(struct protocol_fault_context *);
int fault_case_out_of_range_offset(struct protocol_fault_context *);
int fault_case_bit_flipped_data(struct protocol_fault_context *);
int fault_case_truncated_payload(struct protocol_fault_context *);
int fault_case_corrupted_reserved_fields(struct protocol_fault_context *);
int protocol_fault_run_all(struct protocol_fault_context *);
#endif