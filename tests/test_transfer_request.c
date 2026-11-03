#include "cxl_fw.h"

static int check_request_case(
    const char *name,
    enum fw_transfer_action action,
    unsigned int slot,
    uint32_t offset_units,
    const uint8_t *data,
    size_t data_size)
{
    uint8_t request[FW_TRANSFER_HEADER_SIZE + 64];
    size_t request_size = 0;

    memset(request, 0xAA, sizeof(request));

    if (build_fw_transfer_request(
            request, sizeof(request), &request_size,
            action, slot, offset_units, data, data_size) != 0) {
        fprintf(stderr,
                "FAIL: %s: request builder returned an error\n", name);
        return -1;
    }

    if (request_size != FW_TRANSFER_HEADER_SIZE + data_size) {
        fprintf(stderr, "FAIL: %s: incorrect total request size\n", name);
        return -1;
    }

    if (request[0] != (uint8_t)action ||
        request[1] != (uint8_t)slot) {
        fprintf(stderr, "FAIL: %s: action or slot mismatch\n", name);
        return -1;
    }

    if (request[2] != 0 || request[3] != 0) {
        fprintf(stderr, "FAIL: %s: reserved header bytes are nonzero\n",
                name);
        return -1;
    }

    uint32_t expected_offset = htole32(offset_units);

    if (memcmp(request + 4, &expected_offset,
               sizeof(expected_offset)) != 0) {
        fprintf(stderr, "FAIL: %s: offset mismatch\n", name);
        return -1;
    }

    for (size_t i = 8; i < FW_TRANSFER_HEADER_SIZE; i++) {
        if (request[i] != 0) {
            fprintf(stderr,
                    "FAIL: %s: reserved header byte %zu is nonzero\n",
                    name, i);
            return -1;
        }
    }

    if (data_size > 0 &&
        memcmp(request + FW_TRANSFER_HEADER_SIZE,
               data, data_size) != 0) {
        fprintf(stderr, "FAIL: %s: data chunk mismatch\n", name);
        return -1;
    }

    printf("PASS: %-10s request_size=%zu header=%u data=%zu\n",
           name, request_size, FW_TRANSFER_HEADER_SIZE, data_size);
    return 0;
}

int test_transfer_request(void)
{
    static const uint8_t test_data[] = {
        0x10, 0x21, 0x32, 0x43,
        0x54, 0x65, 0x76, 0x87
    };

    int failures = 0;

    puts("CXL TRANSFER_FW request tests (local only)");
    puts("Synthetic test bytes are never sent to a device.");

    failures += check_request_case(
        "FULL", FW_ACTION_FULL, 2, 0,
        test_data, sizeof(test_data)) != 0;
    failures += check_request_case(
        "INITIATE", FW_ACTION_INITIATE, 2, 0,
        test_data, sizeof(test_data)) != 0;
    failures += check_request_case(
        "CONTINUE", FW_ACTION_CONTINUE, 2, 15,
        test_data, sizeof(test_data)) != 0;
    failures += check_request_case(
        "END", FW_ACTION_END, 2, 30,
        test_data, sizeof(test_data)) != 0;
    failures += check_request_case(
        "ABORT", FW_ACTION_ABORT, 0, 0,
        NULL, 0) != 0;

    {
        uint8_t small_buffer[FW_TRANSFER_HEADER_SIZE];
        size_t request_size = 0;

        if (build_fw_transfer_request(
                small_buffer, sizeof(small_buffer), &request_size,
                FW_ACTION_FULL, 2, 0,
                test_data, sizeof(test_data)) == 0) {
            fprintf(stderr,
                    "FAIL: undersized output buffer was accepted\n");
            failures++;
        } else {
            puts("PASS: undersized output buffer rejected");
        }
    }

    {
        uint8_t request[FW_TRANSFER_HEADER_SIZE + 8];
        size_t request_size = 0;

        if (build_fw_transfer_request(
                request, sizeof(request), &request_size,
                FW_ACTION_FULL, 2, 0, NULL, 8) == 0) {
            fprintf(stderr, "FAIL: NULL data pointer was accepted\n");
            failures++;
        } else {
            puts("PASS: NULL data pointer rejected");
        }
    }

    {
        uint8_t request[FW_TRANSFER_HEADER_SIZE];
        size_t request_size = 0;

        if (build_fw_transfer_request(
                request, sizeof(request), &request_size,
                FW_ACTION_FULL, 0, 0, NULL, 0) == 0) {
            fprintf(stderr, "FAIL: slot zero was accepted for FULL\n");
            failures++;
        } else {
            puts("PASS: slot zero rejected for FULL");
        }
    }

    if (failures != 0) {
        fprintf(stderr, "%d request test(s) failed.\n", failures);
        return -1;
    }

    puts("All complete-request tests passed.");
    puts("No device was opened and no ioctl was issued.");
    return 0;
}