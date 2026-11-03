#include "cxl_fw.h"

#include <sys/stat.h>

#define QEMU_FW_SIZE_BYTES (32U * 1024U * 1024U)
#define TEST_DATA_SIZE 128U
#define REQUEST_CAPACITY (FW_TRANSFER_HEADER_SIZE + TEST_DATA_SIZE)
#define DEFAULT_LOG_PATH "logs/protocol-fault-injection.csv"

static FILE *fault_log;

static void print_hex_result(uint32_t value, char *buffer, size_t size)
{
    (void)snprintf(buffer, size, "0x%02" PRIx32, value);
}

static void log_event(const char *test,
                      const char *step,
                      const char *expected,
                      const char *actual,
                      const char *ioctl_status,
                      const char *verdict,
                      const char *note)
{
    if (fault_log == NULL)
        return;

    fprintf(fault_log, "%s,%s,%s,%s,%s,%s,%s\n",
            test, step, expected, actual, ioctl_status, verdict, note);
    fflush(fault_log);
}

static int submit_and_check(int fd,
                            const char *test,
                            const char *step,
                            const uint8_t *request,
                            size_t request_size,
                            uint32_t expected_return,
                            const char *note)
{
    uint32_t mailbox_return = UINT32_MAX;
    char expected_text[16];
    char actual_text[32];

    print_hex_result(expected_return, expected_text, sizeof(expected_text));

    int out_size = send_raw_command(fd, CXL_FW_TRANSFER_OPCODE,
                                    (uint8_t *)request, request_size,
                                    NULL, 0, &mailbox_return);

    if (out_size < 0) {
        log_event(test, step, expected_text, "IOCTL_ERROR",
                  "ERROR", "FAIL", note);
        printf("  %-20s ioctl failed\n", step);
        return -1;
    }

    print_hex_result(mailbox_return, actual_text, sizeof(actual_text));
    int passed = mailbox_return == expected_return;

    log_event(test, step, expected_text, actual_text,
              "OK", passed ? "PASS" : "FAIL", note);

    printf("  %-20s expected=%s actual=%s  %s\n",
           step, expected_text, actual_text, passed ? "PASS" : "FAIL");

    return passed ? 0 : -1;
}

static int send_abort_cleanup(int fd, const char *test)
{
    uint8_t request[FW_TRANSFER_HEADER_SIZE] = {0};
    request[0] = FW_ACTION_ABORT;
    request[1] = 0; /* QEMU handles ABORT before checking slot or offset. */

    return submit_and_check(fd, test, "cleanup_abort",
                            request, sizeof(request),
                            CXL_MBOX_SUCCESS,
                            "Clear any in-progress transfer state");
}

static int build_test_request(uint8_t *request,
                              size_t *request_size,
                              enum fw_transfer_action action,
                              unsigned int slot,
                              uint32_t offset_units,
                              const uint8_t *data,
                              size_t data_size)
{
    return build_fw_transfer_request(request, REQUEST_CAPACITY,
                                     request_size, action, slot,
                                     offset_units, data, data_size);
}

static void fill_test_data(uint8_t data[TEST_DATA_SIZE])
{
    for (size_t i = 0; i < TEST_DATA_SIZE; i++)
        data[i] = (uint8_t)(0xA5U ^ (uint8_t)i);
}

static int run_single_fault(int fd,
                            const char *name,
                            const uint8_t *request,
                            size_t request_size,
                            uint32_t expected_return,
                            const char *note)
{
    printf("\n[%s]\n", name);
    int test_rc = submit_and_check(fd, name, "fault_request",
                                   request, request_size,
                                   expected_return, note);
    int cleanup_rc = send_abort_cleanup(fd, name);
    return (test_rc == 0 && cleanup_rc == 0) ? 0 : -1;
}

static int run_sequence_fault(int fd,
                              const char *name,
                              unsigned int slot,
                              uint32_t second_offset_units,
                              int expect_duplicate,
                              const uint8_t data[TEST_DATA_SIZE])
{
    uint8_t request[REQUEST_CAPACITY];
    size_t request_size = 0;
    int rc = 0;

    printf("\n[%s]\n", name);

    if (build_test_request(request, &request_size,
                           FW_ACTION_INITIATE, slot, 0,
                           data, TEST_DATA_SIZE) != 0) {
        fprintf(stderr, "Could not build INITIATE request for %s.\n", name);
        (void)send_abort_cleanup(fd, name);
        return -1;
    }

    if (submit_and_check(fd, name, "baseline_initiate",
                         request, request_size, CXL_MBOX_SUCCESS,
                         "Establish a valid partitioned transfer") != 0) {
        rc = -1;
        goto cleanup;
    }

    if (build_test_request(request, &request_size,
                           FW_ACTION_CONTINUE, slot,
                           second_offset_units,
                           data, TEST_DATA_SIZE) != 0) {
        fprintf(stderr, "Could not build fault request for %s.\n", name);
        rc = -1;
        goto cleanup;
    }

    if (submit_and_check(fd, name, "injected_continue",
                         request, request_size, CXL_MBOX_SUCCESS,
                         expect_duplicate
                         ? "Repeat the previous offset and payload length"
                         : "Advance beyond the previous part leaving a gap") != 0)
        rc = -1;

cleanup:
    if (send_abort_cleanup(fd, name) != 0)
        rc = -1;
    return rc;
}

static int open_log(const char *path)
{
    if (mkdir("logs", 0755) < 0 && errno != EEXIST) {
        perror("mkdir logs");
        return -1;
    }

    fault_log = fopen(path, "a");
    if (fault_log == NULL) {
        perror("open fault-injection log");
        return -1;
    }

    if (fseek(fault_log, 0, SEEK_END) != 0) {
        perror("seek fault-injection log");
        fclose(fault_log);
        fault_log = NULL;
        return -1;
    }

    if (ftell(fault_log) == 0) {
        fputs("test,step,expected_mailbox_return,actual_mailbox_return,"
              "ioctl_status,verdict,note\n", fault_log);
        fflush(fault_log);
    }
    return 0;
}

static int dry_run_plan(void)
{
    puts("CXL TRANSFER_FW protocol fault-injection plan (DRY RUN)");
    puts("No device will be opened and no ioctl will be issued.\n");
    puts("1. invalid_action       action=0xFF                  expected 0x02");
    puts("2. invalid_slot         FULL using active slot        expected 0x0B");
    puts("3. out_of_sequence      CONTINUE without INITIATE     expected 0x02");
    puts("4. duplicate_offset     INITIATE then same offset     expected 0x00");
    puts("5. skipped_offset       INITIATE then offset gap      expected 0x00");
    puts("6. out_of_range_offset  offset beyond 32 MiB          expected 0x02");
    puts("7. bit_flipped_data     INITIATE with one bit flipped expected 0x00");
    puts("8. truncated_payload    INITIATE with 64 data bytes   expected 0x00");
    puts("9. reserved_fields      INITIATE with nonzero reserved expected 0x00");
    puts("\nEvery test is followed by TRANSFER_FW ABORT cleanup.");
    return 0;
}

int fault_inject_execute(int argc, char **argv)
{
    const char *log_path = DEFAULT_LOG_PATH;
    int run_all = 0;
    int dry_run = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--all") == 0) {
            if (run_all) {
                fprintf(stderr, "--all was specified more than once.\n");
                return -1;
            }
            run_all = 1;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            dry_run = 1;
        } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            log_path = argv[++i];
        } else {
            fprintf(stderr, "Unknown or incomplete fault-inject option: %s\n",
                    argv[i]);
            return -1;
        }
    }

    if (!run_all) {
        fprintf(stderr, "Specify fault-inject --all.\n");
        return -1;
    }

    if (dry_run)
        return dry_run_plan();

    int fd = open(CXL_DEVICE, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open " CXL_DEVICE);
        return -1;
    }

    struct fw_info info;
    if (read_fw_info_for_transfer(fd, &info) != 0) {
        close(fd);
        return -1;
    }

    unsigned int active_slot = info.slot_info & 0x07U;
    unsigned int test_slot = 0;

    for (unsigned int slot = 1; slot <= info.num_slots; slot++) {
        if (slot != active_slot) {
            test_slot = slot;
            break;
        }
    }

    if (test_slot == 0) {
        fprintf(stderr,
                "No inactive firmware slot is available (slots=%u active=%u).\n",
                info.num_slots, active_slot);
        close(fd);
        return -1;
    }

    unsigned int payload_max = 0;
    if (read_payload_max(&payload_max) != 0) {
        close(fd);
        return -1;
    }
    if (payload_max < REQUEST_CAPACITY) {
        fprintf(stderr,
                "Device payload_max=%u; at least %u bytes are required.\n",
                payload_max, REQUEST_CAPACITY);
        close(fd);
        return -1;
    }

    if (open_log(log_path) != 0) {
        close(fd);
        return -1;
    }

    printf("CXL PROTOCOL FAULT-INJECTION SUITE\n"
           "Device: %s\nMailbox opcode: 0x%04x\n"
           "Reported slots: %u\nActive slot: %u\n"
           "Selected inactive slot: %u\nPayload maximum: %u bytes\n"
           "Log: %s\n\n",
           CXL_DEVICE, CXL_FW_TRANSFER_OPCODE, info.num_slots,
           active_slot, test_slot, payload_max, log_path);

    fprintf(stderr,
            "WARNING: this sends deliberately malformed TRANSFER_FW requests "
            "to the QEMU CXL device.\n"
            "The suite sends ABORT before and after tests to clear any "
            "in-progress transfer.\n"
            "Do not run while another firmware transfer is in progress.\n"
            "No firmware activation is performed.\n"
            "Type INJECT in uppercase to continue: ");

    char confirmation[32] = {0};
    if (fgets(confirmation, sizeof(confirmation), stdin) == NULL ||
        strcmp(confirmation, "INJECT\n") != 0) {
        fprintf(stderr, "Confirmation not received; no fault requests sent.\n");
        fclose(fault_log);
        fault_log = NULL;
        close(fd);
        return -1;
    }

    uint8_t data[TEST_DATA_SIZE];
    fill_test_data(data);

    int failures = 0;
    uint8_t request[REQUEST_CAPACITY];
    size_t request_size = 0;

    puts("\n[initial_state_cleanup]");
    if (send_abort_cleanup(fd, "initial_state_cleanup") != 0) {
        fprintf(stderr,
                "Initial ABORT failed. Stopping to avoid running tests "
                "against an unknown transfer state.\n");
        failures++;
        goto done;
    }

    /* 1. Invalid action: build a valid FULL request, then corrupt byte 0. */
    if (build_test_request(request, &request_size, FW_ACTION_FULL,
                           test_slot, 0, data, TEST_DATA_SIZE) != 0) {
        failures++;
        goto done;
    }
    request[0] = 0xFF;
    if (run_single_fault(fd, "invalid_action",
                         request, request_size, 0x02,
                         "Unsupported action value 0xFF") != 0)
        failures++;

    /* 2. Invalid slot: use the active slot; QEMU rejects it for FULL. */
    if (build_test_request(request, &request_size, FW_ACTION_FULL,
                           test_slot, 0, data, TEST_DATA_SIZE) != 0) {
        failures++;
        goto done;
    }
    request[1] = (uint8_t)active_slot;
    if (run_single_fault(fd, "invalid_slot",
                         request, request_size, 0x0B,
                         "FULL request targets the currently active slot") != 0)
        failures++;

    /* 3. Out of sequence: CONTINUE without a preceding INITIATE. */
    if (build_test_request(request, &request_size, FW_ACTION_CONTINUE,
                           test_slot, 0, data, TEST_DATA_SIZE) != 0) {
        failures++;
        goto done;
    }
    if (run_single_fault(fd, "out_of_sequence",
                         request, request_size, 0x02,
                         "CONTINUE is sent without an active transfer") != 0)
        failures++;

    /* 4. Duplicate offset: QEMU explicitly permits exact back-to-back retransmission. */
    if (run_sequence_fault(fd, "duplicate_offset",
                           test_slot, 0, 1, data) != 0)
        failures++;

    /* 5. Skipped offset: current QEMU checks overlap but not continuity. */
    if (run_sequence_fault(fd, "skipped_offset",
                           test_slot, 2, 0, data) != 0)
        failures++;

    /* 6. Out-of-range offset: 32 MiB boundary plus one 128-byte data unit. */
    if (build_test_request(request, &request_size, FW_ACTION_INITIATE,
                           test_slot,
                           QEMU_FW_SIZE_BYTES / FW_TRANSFER_ALIGNMENT,
                           data, TEST_DATA_SIZE) != 0) {
        failures++;
        goto done;
    }
    if (run_single_fault(fd, "out_of_range_offset",
                         request, request_size, 0x02,
                         "Offset plus data length exceeds QEMU 32 MiB limit") != 0)
        failures++;

    /* 7. Bit-flipped data: corrupt one bit in the data area, not the header. */
    if (build_test_request(request, &request_size, FW_ACTION_INITIATE,
                           test_slot, 0, data, TEST_DATA_SIZE) != 0) {
        failures++;
        goto done;
    }
    request[FW_TRANSFER_HEADER_SIZE] ^= 0x01;
    if (run_single_fault(fd, "bit_flipped_data",
                         request, request_size, CXL_MBOX_SUCCESS,
                         "First data byte has bit 0 flipped; QEMU does not validate contents") != 0)
        failures++;

    /* 8. Truncated data section: 64 bytes instead of the intended 128. */
    if (build_test_request(request, &request_size, FW_ACTION_INITIATE,
                           test_slot, 0, data, TEST_DATA_SIZE / 2) != 0) {
        failures++;
        goto done;
    }
    if (run_single_fault(fd, "truncated_payload",
                         request, request_size, CXL_MBOX_SUCCESS,
                         "Data section shortened from 128 to 64 bytes; header remains complete") != 0)
        failures++;

    /* 9. Reserved fields: mutate both reserved header areas. */
    if (build_test_request(request, &request_size, FW_ACTION_INITIATE,
                           test_slot, 0, data, TEST_DATA_SIZE) != 0) {
        failures++;
        goto done;
    }
    request[2] = 0xA5;
    request[8] = 0x5A;
    if (run_single_fault(fd, "corrupted_reserved_fields",
                         request, request_size, CXL_MBOX_SUCCESS,
                         "Reserved header bytes 2 and 8 are nonzero") != 0)
        failures++;

done:
    printf("\n==================================================\n");
    printf("Protocol fault-injection suite finished.\n");
    printf("Tests with unexpected results or cleanup errors: %d\n", failures);
    printf("Log file: %s\n", log_path);
    printf("Firmware activation: NOT performed\n");
    printf("==================================================\n");

    if (fault_log != NULL) {
        if (fclose(fault_log) != 0) {
            perror("close fault log");
            failures++;
        }
        fault_log = NULL;
    }
    if (close(fd) != 0) {
        perror("close CXL device");
        failures++;
    }

    return failures == 0 ? 0 : -1;
}