#define _GNU_SOURCE

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/cxl_mem.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define CXL_DEVICE "/dev/cxl/mem0"
#define RAW_COMMAND_ID 2
#define FW_TRANSFER_OPCODE 0x0201
#define BG_STATUS_OPCODE 0x0002
#define BG_STARTED 0x0001
#define BG_STATUS_SIZE 8U
#define BG_TIMEOUT_SECONDS 60
#define FW_HEADER_SIZE 128U
#define QEMU_FW_SIZE 0x02000000U

struct bg_status {
    uint8_t status;
    uint8_t reserved;
    uint16_t opcode;
    uint16_t returncode;
    uint16_t vendor_ext_status;
} __attribute__((packed));

enum fault_type {
    FAULT_INVALID_ACTION,
    FAULT_INVALID_SLOT,
    FAULT_OUT_OF_RANGE_OFFSET,
    FAULT_BIT_FLIP,
    FAULT_TRUNCATED_PAYLOAD,
    FAULT_CONTINUE_WITHOUT_INITIATE,
    FAULT_END_WITHOUT_INITIATE,
    FAULT_DUPLICATE_OFFSET,
    FAULT_OUT_OF_ORDER_OFFSET
};

static void usage(const char *program)
{
    printf("CXL firmware fault-injection utility\n\n");
    printf("Usage: %s --fault NAME [--submit]\n\n", program);
    printf("Faults:\n");
    printf("  invalid-action\n");
    printf("  invalid-slot\n");
    printf("  out-of-range-offset\n");
    printf("  bit-flip\n");
    printf("  truncated-payload\n\n");
    printf("  continue-without-initiate\n");
    printf("  end-without-initiate\n");
    printf("  duplicate-offset\n");
    printf("  out-of-order-offset\n");
    printf("Without --submit, only constructs and displays the request.\n");
    printf("--submit sends the deliberately malformed request to the device.\n");
}

static int parse_fault(const char *name, enum fault_type *fault)
{
    if (strcmp(name, "invalid-action") == 0)
        *fault = FAULT_INVALID_ACTION;
    else if (strcmp(name, "invalid-slot") == 0)
        *fault = FAULT_INVALID_SLOT;
    else if (strcmp(name, "out-of-range-offset") == 0)
        *fault = FAULT_OUT_OF_RANGE_OFFSET;
    else if (strcmp(name, "bit-flip") == 0)
        *fault = FAULT_BIT_FLIP;
    else if (strcmp(name, "truncated-payload") == 0)
        *fault = FAULT_TRUNCATED_PAYLOAD;
    else if (strcmp(name, "continue-without-initiate") == 0)
        *fault = FAULT_CONTINUE_WITHOUT_INITIATE;
    else if (strcmp(name, "end-without-initiate") == 0)
        *fault = FAULT_END_WITHOUT_INITIATE;
    else if (strcmp(name, "duplicate-offset") == 0)
        *fault = FAULT_DUPLICATE_OFFSET;
    else if (strcmp(name, "out-of-order-offset") == 0)
        *fault = FAULT_OUT_OF_ORDER_OFFSET;
    else
        return -1;

    return 0;
}

static const char *fault_name(enum fault_type fault)
{
    switch (fault) {
    case FAULT_INVALID_ACTION:
        return "invalid-action";
    case FAULT_INVALID_SLOT:
        return "invalid-slot";
    case FAULT_OUT_OF_RANGE_OFFSET:
        return "out-of-range-offset";
    case FAULT_BIT_FLIP:
        return "bit-flip";
    case FAULT_TRUNCATED_PAYLOAD:
        return "truncated-payload";
    case FAULT_CONTINUE_WITHOUT_INITIATE:
        return "continue-without-initiate";
    case FAULT_END_WITHOUT_INITIATE:
        return "end-without-initiate";
    case FAULT_DUPLICATE_OFFSET:
        return "duplicate-offset";
    case FAULT_OUT_OF_ORDER_OFFSET:
        return "out-of-order-offset";
    default:
        return "unknown";
    }
}

static uint16_t expected_mailbox_return(
    enum fault_type fault)
{
    switch (fault) {
    case FAULT_INVALID_ACTION:
    case FAULT_OUT_OF_RANGE_OFFSET:
    case FAULT_CONTINUE_WITHOUT_INITIATE:
    case FAULT_END_WITHOUT_INITIATE:
        return 0x02; /* CXL_MBOX_INVALID_INPUT */

    case FAULT_INVALID_SLOT:
        return 0x0B; /* CXL_MBOX_INVALID_SLOT */

    case FAULT_TRUNCATED_PAYLOAD:
        return 0x16; /* CXL_MBOX_INVALID_PAYLOAD_LENGTH */

    case FAULT_BIT_FLIP:
    case FAULT_DUPLICATE_OFFSET:
        return 0x00; /* CXL_MBOX_SUCCESS */

    case FAULT_OUT_OF_ORDER_OFFSET:
        return 0x09; /* CXL_MBOX_FW_XFER_OUT_OF_ORDER */

    default:
        return UINT16_MAX;
    }
}

static int build_fault_request(
    enum fault_type fault,
    uint8_t *payload,
    size_t *payload_size)
{
    memset(payload, 0, FW_HEADER_SIZE + 16);

    /* Default: a minimally sized transfer header plus 16 data bytes. */
    payload[0] = 0x00; /* FULL */
    payload[1] = 2;    /* Slot 2 */
    *payload_size = FW_HEADER_SIZE + 16;

    switch (fault) {
    case FAULT_INVALID_ACTION:
        payload[0] = 0x7f;
        break;

    case FAULT_INVALID_SLOT:
        payload[0] = 0x00; /* FULL */
        payload[1] = 0;
        break;

    case FAULT_OUT_OF_RANGE_OFFSET: {
        uint32_t offset_units =
            htole32((QEMU_FW_SIZE / 128U) + 1U);
        memcpy(payload + 4, &offset_units, sizeof(offset_units));
        break;
    }

    case FAULT_BIT_FLIP:
        for (size_t i = FW_HEADER_SIZE;
             i < FW_HEADER_SIZE + 16; i++)
            payload[i] = (uint8_t)(i - FW_HEADER_SIZE);

        /* Flip one bit in the first firmware data byte. */
        payload[FW_HEADER_SIZE] ^= 0x01;
        break;

    case FAULT_TRUNCATED_PAYLOAD:
        /* Deliberately shorter than the 128-byte transfer header. */
        *payload_size = FW_HEADER_SIZE - 1;
        break;

    case FAULT_CONTINUE_WITHOUT_INITIATE:
         payload[0] = 0x02; /* CONTINUE without INITIATE */
         payload[1] = 2;    /* Slot 2 */
         break;

    case FAULT_END_WITHOUT_INITIATE:
         payload[0] = 0x03; /* END without INITIATE */
         payload[1] = 2;    /* Slot 2 */
         break;

    case FAULT_DUPLICATE_OFFSET:
    /* Handled separately by submit_duplicate_offset_test(). */
      return -1;

    case FAULT_OUT_OF_ORDER_OFFSET:
    /* Handled separately by submit_out_of_order_test(). */
    return -1;
    }

    return 0;
}

static int wait_for_background_completion(void)
{
    struct cxl_send_command cmd = {0};
    struct bg_status status = {0};
    int fd;
    int rc;
    int elapsed_ms = 0;

    fd = open(CXL_DEVICE, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("Cannot open CXL device for background status");
        return -1;
    }

    while (elapsed_ms < BG_TIMEOUT_SECONDS * 1000) {
        memset(&cmd, 0, sizeof(cmd));
        memset(&status, 0, sizeof(status));

        cmd.id = RAW_COMMAND_ID;
        cmd.raw.opcode = BG_STATUS_OPCODE;
        cmd.in.size = 0;
        cmd.in.payload = 0;
        cmd.out.size = BG_STATUS_SIZE;
        cmd.out.payload = (__u64)(uintptr_t)&status;

        rc = ioctl(fd, CXL_MEM_SEND_COMMAND, &cmd);

        if (rc < 0) {
            perror("Background status ioctl failed");
            close(fd);
            return -1;
        }

        if (cmd.retval != 0) {
            fprintf(stderr, "Background status error: 0x%08X\n",
                    cmd.retval);
            close(fd);
            return -1;
        }

        if (cmd.out.size != BG_STATUS_SIZE) {
            fprintf(stderr, "Unexpected background status size: %u\n",
                    cmd.out.size);
            close(fd);
            return -1;
        }

        uint16_t opcode = le16toh(status.opcode);
        uint16_t returncode = le16toh(status.returncode);
        unsigned int percent = status.status >> 1;
        int running = status.status & 1;

        printf("\rBackground operation: %u%%", percent);
        fflush(stdout);

        if (opcode != FW_TRANSFER_OPCODE) {
            printf("\n");
            fprintf(stderr, "Background opcode mismatch.\n");
            close(fd);
            return -1;
        }

        if (!running) {
            printf("\n");
            close(fd);

            if (returncode != 0) {
                fprintf(stderr,
                        "Background firmware transfer failed: 0x%04X\n",
                        returncode);
                return -1;
            }

            printf("Background firmware transfer completed successfully.\n");
            return 0;
        }

        struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = 250000000L
        };

        while (nanosleep(&delay, &delay) < 0) {
            if (errno != EINTR) {
                perror("nanosleep failed");
                close(fd);
                return -1;
            }
        }

        elapsed_ms += 250;
    }

    printf("\n");
    fprintf(stderr, "Timed out waiting for background operation.\n");
    close(fd);
    return -1;
}

static int submit_request(
    const uint8_t *payload,
    size_t payload_size)
{
    struct cxl_send_command cmd = {0};
    int fd = open(CXL_DEVICE, O_RDWR | O_CLOEXEC);

    if (fd < 0) {
        perror("Cannot open CXL device");
        return -1;
    }

    cmd.id = RAW_COMMAND_ID;
    cmd.raw.opcode = FW_TRANSFER_OPCODE;
    cmd.raw.rsvd = 0;
    cmd.in.size = payload_size;
    cmd.in.rsvd = 0;
    cmd.in.payload = (__u64)(uintptr_t)payload;

    printf("Submitting fault through CXL_MEM_SEND_COMMAND...\n");

    int rc = ioctl(fd, CXL_MEM_SEND_COMMAND, &cmd);

    if (rc < 0) {
        perror("CXL_MEM_SEND_COMMAND ioctl failed");
        close(fd);
        return -1;
    }

    printf("ioctl returned successfully.\n");
    printf("Mailbox return value: %u (0x%08X)\n",
           cmd.retval, cmd.retval);

    close(fd);

    if (cmd.retval == BG_STARTED) {
        printf("Background firmware operation started.\n");
        return wait_for_background_completion();
    }

    if (cmd.retval != 0) {
        printf("Device rejected the mailbox command.\n");
        return (int)cmd.retval;
    }

    printf("Mailbox command completed successfully.\n");
    return 0;

}

static int submit_duplicate_offset_test(void)
{
    uint8_t payload[FW_HEADER_SIZE + 16] = {0};

    /* Prepare the INITIATE request. */
    payload[0] = 0x01; /* INITIATE */
    payload[1] = 2;    /* Slot 2 */

    for (size_t i = FW_HEADER_SIZE;
         i < FW_HEADER_SIZE + 16; i++) {
        payload[i] = (uint8_t)(i - FW_HEADER_SIZE);
    }

    printf("\nStep 1: Sending INITIATE request\n");
    printf("Action: 0x01, Offset: 0, Data length: 16\n");

    int rc = submit_request(payload, sizeof(payload));
    if (rc != 0) {
        printf("INITIATE failed. Duplicate test stopped.\n");
        return rc;
    }

    /* Change only the action. Keep offset and length identical. */
    payload[0] = 0x02; /* CONTINUE */

    printf("\nStep 2: Sending duplicate CONTINUE request\n");
    printf("Action: 0x02, Offset: 0, Data length: 16\n");

    rc = submit_request(payload, sizeof(payload));

    if (rc == 0) {
    printf("PASS: QEMU accepted the duplicate CONTINUE as expected.\n");
    } else {
    printf("FAIL: Expected mailbox success (0x00), received 0x%04X.\n",
           (unsigned int)rc);
    }

   /* Always attempt to clean up the partitioned transfer. */
    printf("\nCleanup: Sending ABORT request\n");
    payload[0] = 0x04; /* ABORT */

    int abort_rc = submit_request(payload, sizeof(payload));

    if (abort_rc != 0) {
       printf("WARNING: ABORT cleanup failed.\n");
       return abort_rc;
     }

     printf("Partitioned transfer cleanup completed.\n");

   return rc;
}

static int submit_out_of_order_test(void)
{
    uint8_t payload[FW_HEADER_SIZE + 32] = {0};

    /* Step 1: Send INITIATE with 16 bytes. */
    payload[0] = 0x01; /* INITIATE */
    payload[1] = 2;    /* Slot 2 */

    for (size_t i = FW_HEADER_SIZE;
         i < FW_HEADER_SIZE + 16; i++) {
        payload[i] = (uint8_t)(i - FW_HEADER_SIZE);
    }

    printf("\nStep 1: Sending INITIATE request\n");
    printf("Action: 0x01, Offset: 0, Data length: 16\n");

    int rc = submit_request(payload, FW_HEADER_SIZE + 16);

    if (rc != 0) {
        printf("INITIATE failed. Test stopped.\n");
        return rc;
    }

    /* Step 2: Send overlapping CONTINUE with 32 bytes. */
    payload[0] = 0x02; /* CONTINUE */

    printf("\nStep 2: Sending overlapping CONTINUE request\n");
    printf("Action: 0x02, Offset: 0, Data length: 32\n");

    rc = submit_request(payload, sizeof(payload));

    if (rc != 0x09) {
        printf("FAIL: Expected QEMU to reject the overlapping CONTINUE.\n");
    }
    else {
        printf("PASS: QEMU rejected the overlapping CONTINUE as expected.\n");
    }

    int test_passed = (rc == 0x09);

    /* Step 3: Abort to clean up the transfer state. */
    printf("\nCleanup: Sending ABORT request\n");
    payload[0] = 0x04; /* ABORT */

    int abort_rc = submit_request(payload, sizeof(payload));

    if (abort_rc != 0) {
        printf("WARNING: ABORT cleanup failed.\n");
        return abort_rc;
    }

    printf("Partitioned transfer cleanup completed.\n");

   return test_passed ? 0 : 1;
}

int main(int argc, char **argv)
{
    enum fault_type fault = FAULT_INVALID_ACTION;
    uint8_t payload[FW_HEADER_SIZE + 16] = {0};
    size_t payload_size = 0;
    int submit = 0;
    int fault_supplied = 0;
    int option;


    static const struct option options[] = {
        {"fault", required_argument, NULL, 'f'},
        {"submit", no_argument, NULL, 's'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    while ((option = getopt_long(
        argc, argv, "f:sh", options, NULL)) != -1) {
        switch (option) {
        case 'f':
            if (parse_fault(optarg, &fault) != 0) {
                fprintf(stderr, "Unknown fault: %s\n", optarg);
                return EXIT_FAILURE;
           }

	    fault_supplied = 1;
            break;

        case 's':
            submit = 1;
            break;
        case 'h':
            usage(argv[0]);
            return EXIT_SUCCESS;
        default:
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (optind != argc) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }


    if (!fault_supplied) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (fault == FAULT_DUPLICATE_OFFSET) {
        printf("\nFault injection: duplicate-offset\n");
        printf("Opcode: 0x%04X\n", FW_TRANSFER_OPCODE);

        if (!submit) {
            printf("Dry run: duplicate test requires two requests.\n");
            printf("Submission: DISABLED\n");
            return EXIT_SUCCESS;
        }

        printf("Submission: ENABLED\n");
        printf("\nWARNING: sending a partitioned firmware transfer.\n");

        int rc = submit_duplicate_offset_test();
        return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (fault == FAULT_OUT_OF_ORDER_OFFSET) {
        printf("\nFault injection: out-of-order-offset\n");
        printf("Opcode: 0x%04X\n", FW_TRANSFER_OPCODE);

        if (!submit) {
            printf("Dry run: overlapping transfer test requires two requests.\n");
            printf("Submission: DISABLED\n");
            return EXIT_SUCCESS;
        }

        printf("Submission: ENABLED\n");
        printf("\nWARNING: sending an overlapping firmware transfer.\n");

        int rc = submit_out_of_order_test();
        return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (build_fault_request(fault, payload, &payload_size) != 0) {
        fprintf(stderr, "Failed to build fault request.\n");
        return EXIT_FAILURE;
    }

    printf("\nFault injection request\n");
    printf("-----------------------\n");
    printf("Fault: %s\n", fault_name(fault));
    printf("Opcode: 0x%04X\n", FW_TRANSFER_OPCODE);
    printf("Payload size: %zu bytes\n", payload_size);
    printf("Submission: %s\n", submit ? "ENABLED" : "DISABLED");

    printf("Payload first bytes:");
    for (size_t i = 0; i < payload_size && i < 16; i++)
        printf(" %02x", payload[i]);
    printf("\n");

    if (!submit) {
        printf("Dry run: no device command sent.\n");
        return EXIT_SUCCESS;
    }

    printf("\nWARNING: sending an intentionally malformed command.\n");
    int rc = submit_request(payload,payload_size);
    uint16_t expected = expected_mailbox_return(fault);

    if (rc < 0 || expected == UINT16_MAX) {
    printf("FAIL: Submission or test configuration error.\n");
    return EXIT_FAILURE;
    }

    if ((uint16_t)rc == expected) {
    printf("PASS: Received expected mailbox response 0x%04X.\n",
           expected);
    return EXIT_SUCCESS;
    }

    printf("FAIL: Expected 0x%04X but received 0x%04X.\n",
       expected, (uint16_t)rc);

    return EXIT_FAILURE;

    }
