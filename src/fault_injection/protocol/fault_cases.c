#include "fault_cases.h"
#include "fault_log.h"

int protocol_fault_run_all(struct protocol_fault_context *ctx)
{
    static const struct { const char *name; protocol_fault_fn run; } cases[] = {
        {"invalid_action",fault_case_invalid_action},
        {"invalid_slot",fault_case_invalid_slot},
        {"out_of_sequence",fault_case_out_of_sequence},
        {"duplicate_offset",fault_case_duplicate_offset},
        {"skipped_offset",fault_case_skipped_offset},
        {"out_of_range_offset",fault_case_out_of_range_offset},
        {"bit_flipped_data",fault_case_bit_flipped_data},
        {"truncated_payload",fault_case_truncated_payload},
        {"corrupted_reserved_fields",fault_case_corrupted_reserved_fields}
    };
    unsigned int passed=0,failed=0;
    if(fault_abort_cleanup(ctx,"initial_cleanup")!=0) {
        fprintf(stderr,"Initial ABORT failed; stopping before fault injection.\n");
        return -1;
    }
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        int rc=cases[i].run(ctx);
        if(rc==0) passed++; else failed++;
    }
    printf("\nProtocol suite summary: %u PASS, %u FAIL\n",passed,failed);
    return failed? -1:0;
}