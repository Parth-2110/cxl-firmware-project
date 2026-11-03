#include "cxl_fw.h"

static int check_header_case(
    const char *name,
    enum fw_transfer_action action,
    unsigned int slot,
    uint32_t offset_units)
{
    uint8_t buffer[FW_TRANSFER_HEADER_SIZE];
    uint32_t expected_offset = htole32(offset_units);

    if (build_fw_transfer_header(buffer, sizeof(buffer), action,
                                 slot, offset_units) != 0) {
        fprintf(stderr, "FAIL: %s: header builder returned an error\n",
                name);
        return -1;
    }

    if (buffer[0] != (uint8_t)action ||
        buffer[1] != (uint8_t)slot ||
        buffer[2] != 0 || buffer[3] != 0 ||
        memcmp(buffer + 4, &expected_offset,
               sizeof(expected_offset)) != 0) {
        fprintf(stderr, "FAIL: %s: header fields mismatch\n", name);
        return -1;
    }

    for (size_t i = 8; i < sizeof(buffer); i++) {
        if (buffer[i] != 0) {
            fprintf(stderr,
                    "FAIL: %s: reserved byte %zu is nonzero\n",
                    name, i);
            return -1;
        }
    }

    printf("PASS: %-10s action=%u slot=%u offset_units=%" PRIu32 "\n",
           name, (unsigned int)action, slot, offset_units);
    return 0;
}

int test_transfer_header(void)
{
    int failures = 0;

    puts("CXL TRANSFER_FW header tests (local only)");
    printf("Opcode: 0x%04x; header size: %u bytes\n",
           CXL_FW_TRANSFER_OPCODE, FW_TRANSFER_HEADER_SIZE);

    failures += check_header_case(
        "FULL", FW_ACTION_FULL, 2, 0) != 0;
    failures += check_header_case(
        "INITIATE", FW_ACTION_INITIATE, 2, 0) != 0;
    failures += check_header_case(
        "CONTINUE", FW_ACTION_CONTINUE, 2, 15) != 0;
    failures += check_header_case(
        "END", FW_ACTION_END, 2, 30) != 0;
    failures += check_header_case(
        "ABORT", FW_ACTION_ABORT, 0, 0) != 0;

    if (failures != 0) {
        fprintf(stderr, "%d header test(s) failed.\n", failures);
        return -1;
    }

    puts("All header tests passed.");
    puts("No device was opened and no ioctl was issued.");
    return 0;
}