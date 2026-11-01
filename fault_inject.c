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
    FAULT_TRUNCATED_PAYLOAD
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
    default:
        return "unknown";
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
        return 1;
    }

    printf("Mailbox command completed successfully.\n");
    return 0;

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
    int rc = submit_request(payload, payload_size);

    return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
