#include <errno.h>
#include <endian.h>
#include <fcntl.h>
#include <linux/cxl_mem.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define CXL_DEVICE "/dev/cxl/mem0"

#define CXL_RAW_COMMAND_ID 2
#define CXL_FW_TRANSFER_OPCODE 0x0201
#define CXL_FW_ACTIVATE_OPCODE 0x0202

struct cxl_fw_activate_request {
    uint8_t action;
    uint8_t slot;
} __attribute__((packed));

_Static_assert(sizeof(struct cxl_fw_activate_request) == 2,
               "ACTIVATE_FW request must be exactly 2 bytes");

#define CXL_BG_STATUS_OPCODE 0x0002
#define CXL_BG_STARTED 0x0001
#define CXL_BG_STATUS_SIZE 8U
#define CXL_BG_TIMEOUT_SECONDS 60


#define CXL_FW_HEADER_SIZE 128U
#define CXL_FW_ALIGNMENT 128U

#define CXL_PAYLOAD_MAX_PATH "/sys/bus/cxl/devices/mem0/payload_max"

#define CXL_FW_ACTION_FULL     0x00
#define CXL_FW_ACTION_INITIATE 0x01
#define CXL_FW_ACTION_CONTINUE 0x02
#define CXL_FW_ACTION_END      0x03

struct cxl_fw_transfer_header {
    uint8_t action;
    uint8_t slot;
    uint8_t reserved[2];
    uint32_t offset_le;
    uint8_t reserved2[120];
    uint8_t data[];
} __attribute__((packed));

_Static_assert(offsetof(struct cxl_fw_transfer_header, data) == 128,
               "Transfer header must be 128 bytes");

static int submit_transfer_request(
    const uint8_t *request,
    size_t request_size);

static int build_transfer_request(
    uint8_t action,
    uint8_t slot,
    size_t byte_offset,
    const uint8_t *data,
    size_t data_size,
    size_t payload_max,
    uint8_t **request_out,
    size_t *request_size_out)
{
    struct cxl_fw_transfer_header *header;
    uint8_t *request;
    size_t request_size;
    uint32_t encoded_offset;

    if (request_out == NULL || request_size_out == NULL) {
        fprintf(stderr, "Error: missing request output parameters.\n");
        return -1;
    }

    *request_out = NULL;
    *request_size_out = 0;

    if (data_size > payload_max ||
        payload_max < CXL_FW_HEADER_SIZE ||
        data_size > payload_max - CXL_FW_HEADER_SIZE) {
        fprintf(stderr, "Error: request exceeds mailbox payload limit.\n");
        return -1;
    }

    if (byte_offset % CXL_FW_ALIGNMENT != 0) {
        fprintf(stderr, "Error: byte offset is not 128-byte aligned.\n");
        return -1;
    }

    if (byte_offset / CXL_FW_ALIGNMENT > UINT32_MAX) {
        fprintf(stderr, "Error: encoded offset is too large.\n");
        return -1;
    }

    if (data_size > 0 && data == NULL) {
        fprintf(stderr, "Error: missing firmware data.\n");
        return -1;
    }

    request_size = CXL_FW_HEADER_SIZE + data_size;
    request = calloc(1, request_size);
    if (request == NULL) {
        perror("calloc");
        return -1;
    }

    header = (struct cxl_fw_transfer_header *)request;
    encoded_offset = (uint32_t)(byte_offset / CXL_FW_ALIGNMENT);

    header->action = action;
    header->slot = slot;
    header->offset_le = htole32(encoded_offset);

    if (data_size > 0)
        memcpy(request + CXL_FW_HEADER_SIZE, data, data_size);

    printf("In-memory TRANSFER_FW request\n");
    printf("------------------------------\n");
    printf("Opcode: 0x%04X (not sent)\n", CXL_FW_TRANSFER_OPCODE);
    printf("Action: 0x%02X\n", action);
    printf("Slot: %u\n", slot);
    printf("Byte offset: %zu\n", byte_offset);
    printf("Encoded offset: %u\n", encoded_offset);
    printf("Header size: %u bytes\n", CXL_FW_HEADER_SIZE);
    printf("Firmware data size: %zu bytes\n", data_size);
    printf("Total payload size: %zu bytes\n", request_size);
    printf("Payload limit: %zu bytes\n", payload_max);
    printf("Device command sent: NO\n");

    if (header->action != action || header->slot != slot ||
        le32toh(header->offset_le) != encoded_offset) {
        fprintf(stderr, "Error: constructed header verification failed.\n");
        free(request);
        return -1;
    }

    if (data_size > 0 &&
        memcmp(request + CXL_FW_HEADER_SIZE, data, data_size) != 0) {
        fprintf(stderr, "Error: constructed data verification failed.\n");
        free(request);
        return -1;
    }

      printf("Header and data verification: PASS\n");

    *request_out = request;
    *request_size_out = request_size;

    return 0;
}


static int read_payload_max(size_t *payload_max)
{
    FILE *fp;
    unsigned long value;
    char extra;

    fp = fopen(CXL_PAYLOAD_MAX_PATH, "r");
    if (fp == NULL) {
        perror("Cannot open CXL payload_max");
        return -1;
    }

    if (fscanf(fp, "%lu %c", &value, &extra) != 1) {
        fprintf(stderr, "Error: invalid payload_max value.\n");
        fclose(fp);
        return -1;
    }

    if (fclose(fp) != 0) {
        perror("Cannot close payload_max");
        return -1;
    }

    if (value < CXL_FW_HEADER_SIZE ||
        value > SIZE_MAX ||
        (value - CXL_FW_HEADER_SIZE) % CXL_FW_ALIGNMENT != 0) {
        fprintf(stderr, "Error: unsupported CXL payload_max value: %lu\n",
                value);
        return -1;
    }

    *payload_max = (size_t)value;
    return 0;
}

static int plan_transfer(size_t image_size, size_t payload_max)
{
    size_t offset = 0;
    unsigned int command_number = 0;

size_t max_chunk;

if (payload_max <= CXL_FW_HEADER_SIZE) {
    fprintf(stderr, "Error: payload limit is too small.\n");
    return -1;
}

max_chunk = payload_max - CXL_FW_HEADER_SIZE;

if (max_chunk % CXL_FW_ALIGNMENT != 0) {
    fprintf(stderr, "Error: maximum chunk is not 128-byte aligned.\n");
    return -1;
}

    if (image_size == 0) {
        fprintf(stderr, "Error: firmware image is empty.\n");
        return -1;
    }
      

    if (image_size > UINT32_MAX) {
        fprintf(stderr, "Error: firmware image exceeds supported size.\n");
        return -1;
    }

    /*
     * The kernel requires each chunk offset to be 128-byte aligned.
     * Every non-final chunk must therefore be a multiple of 128 bytes.
     */
  
    printf("Dry-run transfer plan\n");
    printf("---------------------\n");
    printf("Image size: %zu bytes\n", image_size);
    printf("Mailbox payload limit: %zu bytes\n", payload_max);
printf("Maximum data per command: %zu bytes\n", max_chunk);
    if (image_size <= max_chunk)  {
        printf("Command 1: FULL, image bytes=%zu, encoded offset=0\n",
               image_size);
        printf("\nDry run only. No device commands were sent.\n");
        return 0;
    }

    while (offset < image_size) {
        size_t remaining = image_size - offset;
        size_t chunk = remaining < max_chunk
             ? remaining : max_chunk;
        unsigned int action;

       if (offset % CXL_FW_ALIGNMENT != 0) {
    fprintf(stderr,
            "Error: transfer offset %zu is not 128-byte aligned.\n",
            offset);
    return -1;
}
        if (offset == 0)
            action = CXL_FW_ACTION_INITIATE;
        else if (chunk == remaining)
            action = CXL_FW_ACTION_END;
        else
            action = CXL_FW_ACTION_CONTINUE;

        if (chunk == 0 ||
            (offset + chunk < image_size &&
             chunk % CXL_FW_ALIGNMENT != 0)) {
            fprintf(stderr,
                    "Error: calculated chunk violates alignment rules.\n");
            return -1;
        }

        printf("Command %u: action=%s, byte_offset=%zu, "
               "encoded_offset=%zu, data_bytes=%zu, payload_bytes=%zu\n",
               ++command_number,
               action == CXL_FW_ACTION_INITIATE ? "INITIATE" :
               action == CXL_FW_ACTION_CONTINUE ? "CONTINUE" : "END",
               offset, offset / CXL_FW_ALIGNMENT,
               chunk, CXL_FW_HEADER_SIZE + chunk);

        offset += chunk;
    }

    printf("\nDry run only. No device commands were sent.\n");
    return 0;
}

static int build_image_requests(
    int image_fd,
    size_t image_size,
    size_t payload_max,
    int submit_enabled)
{
    size_t offset = 0;
    size_t max_chunk;
    unsigned int command_number = 0;

    if (payload_max <= CXL_FW_HEADER_SIZE) {
        fprintf(stderr, "Error: payload limit is too small.\n");
        return -1;
    }

    max_chunk = payload_max - CXL_FW_HEADER_SIZE;

    if (image_size == 0) {
        fprintf(stderr, "Error: firmware image is empty.\n");
        return -1;
    }

    if (image_size > UINT32_MAX) {
        fprintf(stderr, "Error: firmware image exceeds supported size.\n");
        return -1;
    }

    if (image_size <= max_chunk) {
        uint8_t *data = malloc(image_size);
        uint8_t *request = NULL;
        size_t request_size = 0;
        size_t bytes_read = 0;

        if (data == NULL) {
            perror("malloc");
            return -1;
        }

        while (bytes_read < image_size) {
            ssize_t n = read(image_fd, data + bytes_read,
                             image_size - bytes_read);

            if (n < 0) {
                if (errno == EINTR)
                    continue;

                perror("Cannot read firmware image");
                free(data);
                return -1;
            }

            if (n == 0) {
                fprintf(stderr, "Error: unexpected end of image.\n");
                free(data);
                return -1;
            }

            bytes_read += (size_t)n;
        }

        int rc = build_transfer_request(
            CXL_FW_ACTION_FULL, 2, 0,
            data, image_size, payload_max,
            &request, &request_size);

                        if (rc == 0) {
            printf("Built request %u: action=FULL, offset=0, "
                   "data_bytes=%zu, request_bytes=%zu\n",
                   ++command_number, image_size, request_size);

            if (submit_enabled) {
                rc = submit_transfer_request(request, request_size);
            }

            free(request);
        }

        free(data);
        return rc;
}

    while (offset < image_size) {
        size_t remaining = image_size - offset;
        size_t chunk = remaining < max_chunk ? remaining : max_chunk;
        unsigned int action;
        uint8_t *data = malloc(chunk);
        uint8_t *request = NULL;
        size_t request_size = 0;
        size_t bytes_read = 0;

        if (data == NULL) {
            perror("malloc");
            return -1;
        }

        if (offset % CXL_FW_ALIGNMENT != 0 ||
            (chunk < remaining && chunk % CXL_FW_ALIGNMENT != 0)) {
            fprintf(stderr, "Error: invalid chunk alignment.\n");
            free(data);
            return -1;
        }

        if (offset == 0)
            action = CXL_FW_ACTION_INITIATE;
        else if (chunk == remaining)
            action = CXL_FW_ACTION_END;
        else
            action = CXL_FW_ACTION_CONTINUE;

        while (bytes_read < chunk) {
            ssize_t n = read(image_fd, data + bytes_read,
                             chunk - bytes_read);

            if (n < 0) {
                if (errno == EINTR)
                    continue;

                perror("Cannot read firmware image");
                free(data);
                return -1;
            }

            if (n == 0) {
                fprintf(stderr, "Error: unexpected end of image.\n");
                free(data);
                return -1;
            }

            bytes_read += (size_t)n;
        }

        int rc = build_transfer_request(
            (uint8_t)action, 2, offset,
            data, chunk, payload_max,
            &request, &request_size);

        if (rc != 0) {
            free(data);
            return -1;
        }

        printf("Built request %u: action=%s, offset=%zu, "
               "data_bytes=%zu, request_bytes=%zu\n",
               ++command_number,
               action == CXL_FW_ACTION_INITIATE ? "INITIATE" :
               action == CXL_FW_ACTION_CONTINUE ? "CONTINUE" : "END",
               offset, chunk, request_size);

                if (submit_enabled) {
            if (submit_transfer_request(request, request_size) != 0) {
                free(request);
                free(data);
                return -1;
            }
        }

        free(request);
        free(data);
        offset += chunk;
    }

    if (submit_enabled){
    printf("\nAll TRANSFER_FW requests submitted successfully.\n");}
else{
    printf("\nBuild-only test complete. No device commands were sent.\n");
}
    return 0;
}


static int submit_activate_request(uint8_t slot, uint8_t action)
{
    struct cxl_fw_activate_request request = {
        .action = action,
        .slot = slot
    };
    struct cxl_send_command cmd = {0};
    int fd;
    int rc;

    if (slot == 0 || slot > 2) {
        fprintf(stderr, "Error: slot must be 1 or 2 for this QEMU setup.\n");
        return -1;
    }

    if (action > 1) {
        fprintf(stderr, "Error: action must be 0 (online) or 1 (after reset).\n");
        return -1;
    }

    printf("ACTIVATE_FW request (not yet submitted)\n");
    printf("----------------------------------------\n");
    printf("Opcode: 0x%04X\n", CXL_FW_ACTIVATE_OPCODE);
    printf("Action: %u (%s)\n", action,
           action == 0 ? "online" : "after reset");
    printf("Target slot: %u\n", slot);
    printf("Payload size: %zu bytes\n", sizeof(request));
    printf("Payload bytes: %02x %02x\n", request.action, request.slot);

    fd = open(CXL_DEVICE, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("Cannot open CXL device");
        return -1;
    }

    cmd.id = CXL_RAW_COMMAND_ID;
    cmd.raw.opcode = CXL_FW_ACTIVATE_OPCODE;
    cmd.raw.rsvd = 0;
    cmd.in.size = sizeof(request);
    cmd.in.rsvd = 0;
    cmd.in.payload = (__u64)(uintptr_t)&request;

    printf("Submitting ACTIVATE_FW through CXL_MEM_SEND_COMMAND...\n");

    rc = ioctl(fd, CXL_MEM_SEND_COMMAND, &cmd);
    if (rc < 0) {
        perror("ACTIVATE_FW ioctl failed");
        close(fd);
        return -1;
    }

    printf("ioctl succeeded. Mailbox return value: %u (0x%08X)\n",
           cmd.retval, cmd.retval);

    if (close(fd) < 0)
        perror("Warning: failed to close CXL device");

    if (cmd.retval != 0) {
        fprintf(stderr, "ACTIVATE_FW failed: mailbox return code 0x%08X\n",
                cmd.retval);
        return -1;
    }

    printf("ACTIVATE_FW command completed successfully.\n");
    printf("Note: this confirms QEMU's command response, not firmware bootability.\n");
    return 0;
}

int main(int argc, char **argv)
{
    int submit_enabled = 0;

    /*
     * Syntax: --activate <slot> --action <0|1>
     * This branch is implemented separately from TRANSFER_FW.
     */
    if (argc == 5 && strcmp(argv[1], "--activate") == 0 &&
        strcmp(argv[3], "--action") == 0) {
        char *slot_end = NULL;
        char *action_end = NULL;
        unsigned long slot_value;
        unsigned long action_value;

        errno = 0;
        slot_value = strtoul(argv[2], &slot_end, 10);
        if (errno != 0 || slot_end == argv[2] || *slot_end != '\0' ||
            slot_value > UINT8_MAX) {
            fprintf(stderr, "Error: invalid slot argument.\n");
            return EXIT_FAILURE;
        }

        errno = 0;
        action_value = strtoul(argv[4], &action_end, 10);
        if (errno != 0 || action_end == argv[4] || *action_end != '\0' ||
            action_value > UINT8_MAX) {
            fprintf(stderr, "Error: invalid action argument.\n");
            return EXIT_FAILURE;
        }

        if (submit_activate_request((uint8_t)slot_value,
                                    (uint8_t)action_value) != 0)
            return EXIT_FAILURE;

        return EXIT_SUCCESS;
    }


    if (argc == 3 && strcmp(argv[1], "--submit") == 0) {
        submit_enabled = 1;
    }

     if (argc == 3 && strcmp(argv[1], "--build-only") == 0) {
        int fd;
        struct stat st;
        size_t payload_max;
        int rc;



        fd = open(argv[2], O_RDONLY);
        if (fd < 0) {
            perror("Cannot open firmware image");
            return EXIT_FAILURE;
        }

        if (fstat(fd, &st) < 0) {
            perror("Cannot inspect firmware image");
            close(fd);
            return EXIT_FAILURE;
        }

        if (!S_ISREG(st.st_mode) || st.st_size < 0) {
            fprintf(stderr, "Error: input must be a regular file.\n");
            close(fd);
            return EXIT_FAILURE;
        }

        if (read_payload_max(&payload_max) < 0) {
            close(fd);
            return EXIT_FAILURE;
        }

        printf("Build-only mode: %s\n", argv[2]);
        printf("No device commands will be sent.\n");

        rc = build_image_requests(fd, (size_t)st.st_size, payload_max,0);
        close(fd);

        return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    struct stat st;
    size_t payload_max;
    int fd;
    int rc;

               if (submit_enabled) {
        /* Open the firmware image. */
        fd = open(argv[2], O_RDONLY);
        if (fd < 0) {
            perror("Cannot open firmware image");
            return EXIT_FAILURE;
        }

        /* Validate the image file. */
        if (fstat(fd, &st) < 0) {
            perror("Cannot inspect firmware image");
            close(fd);
            return EXIT_FAILURE;
        }

        if (!S_ISREG(st.st_mode) || st.st_size <= 0) {
            fprintf(stderr,
                    "Error: firmware image must be a non-empty regular file.\n");
            close(fd);
            return EXIT_FAILURE;
        }

        /* Read the device's maximum mailbox payload size. */
        if (read_payload_max(&payload_max) < 0) {
            close(fd);
            return EXIT_FAILURE;
        }

        printf("Firmware image: %s\n", argv[2]);
        printf("Image size: %lld bytes\n", (long long)st.st_size);
        printf("Payload maximum: %zu bytes\n", payload_max);
        printf("Submission mode: ENABLED\n");

        /*
         * Build each TRANSFER_FW request and submit it through ioctl.
         * Stop if any request fails.
         */
        rc = build_image_requests(
            fd,
            (size_t)st.st_size,
            payload_max,
            1
        );

        if (close(fd) < 0)
            perror("Warning: failed to close firmware image");

        if (rc != 0) {
            fprintf(stderr,
                    "Firmware transfer failed. Stopping submission.\n");
            return EXIT_FAILURE;
        }

        printf("Firmware transfer requests completed successfully.\n");
        return EXIT_SUCCESS;
    }

	    if (argc == 3 && strcmp(argv[1], "--inspect-file") == 0) {
        uint8_t data[16];
        uint8_t *request = NULL;
        size_t request_size = 0;
        size_t bytes_read;
        int image_fd;
        int inspect_rc;

        image_fd = open(argv[2], O_RDONLY);
        if (image_fd < 0) {
            perror("Cannot open test image");
            return EXIT_FAILURE;
        }

        bytes_read = 0;
        while (bytes_read < sizeof(data)) {
            ssize_t n = read(image_fd, data + bytes_read,
                             sizeof(data) - bytes_read);

            if (n < 0) {
                if (errno == EINTR)
                    continue;

                perror("Cannot read test image");
                close(image_fd);
                return EXIT_FAILURE;
            }

            if (n == 0)
                break;

            bytes_read += (size_t)n;
        }

        close(image_fd);

        if (bytes_read != sizeof(data)) {
            fprintf(stderr, "Error: test image contains fewer than 16 bytes.\n");
            return EXIT_FAILURE;
        }

        printf("Read %zu bytes from %s\n", bytes_read, argv[2]);

        inspect_rc = build_transfer_request(
            CXL_FW_ACTION_CONTINUE,
            2,
            1920,
            data,
            bytes_read,
            2048,
            &request,
            &request_size
        );

        if (inspect_rc != 0)
            return EXIT_FAILURE;

        printf("Returned request size: %zu bytes\n", request_size);
        printf("First request data bytes: ");

        for (size_t i = 0; i < bytes_read; i++)
            printf("%02x ", request[CXL_FW_HEADER_SIZE + i]);

        printf("\nDevice command sent: NO\n");

        free(request);
        return EXIT_SUCCESS;
    }

     if (argc == 2 && strcmp(argv[1], "--inspect") == 0) {
    uint8_t dummy_data[16];
    uint8_t *request = NULL;
    size_t request_size = 0;
    int inspect_rc;

    for (size_t i = 0; i < sizeof(dummy_data); i++)
        dummy_data[i] = (uint8_t)i;

    inspect_rc = build_transfer_request(
        CXL_FW_ACTION_CONTINUE,
        2,
        1920,
        dummy_data,
        sizeof(dummy_data),
        2048,
        &request,
        &request_size
    );

    if (inspect_rc != 0)
        return EXIT_FAILURE;

    printf("Returned request size: %zu bytes\n", request_size);
    free(request);

    return EXIT_SUCCESS;
}

        if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s <firmware-image>\n", argv[0]);
        printf("       %s --build-only <firmware-image>\n", argv[0]);
        printf("       %s --submit <firmware-image>\n", argv[0]);
        printf("       %s --activate <slot> --action <0|1>\n", argv[0]);
        printf("       %s --inspect\n", argv[0]);
        printf("       %s --inspect-file <test-image>\n", argv[0]);

        printf("\nModes:\n");
        printf("  <firmware-image>          Display a transfer plan.\n");
        printf("  --build-only <image>      Build and validate requests only.\n");
        printf("  --submit <image>          Submit TRANSFER_FW requests through ioctl.\n");
        printf("  --activate <slot> --action <0|1>  Submit ACTIVATE_FW through ioctl.\n");
         printf("  --inspect                 Inspect a request built with dummy data.\n");
        printf("  --inspect-file <image>    Inspect a request built from a file.\n");

        printf("\nSafety:\n");
        printf("  --submit and --activate send commands to the CXL device.\n");
        printf("  Other modes do not send firmware commands.\n");
        return EXIT_SUCCESS;
    }

    if (argc != 2) {
        fprintf(stderr, "Usage: %s --help\n", argv[0]);
        return EXIT_FAILURE;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("Cannot open firmware image");
        return EXIT_FAILURE;
    }

    if (fstat(fd, &st) < 0) {
        perror("Cannot inspect firmware image");
        close(fd);
        return EXIT_FAILURE;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "Error: firmware image must be a regular file.\n");
        close(fd);
        return EXIT_FAILURE;
    }

close(fd);

if (read_payload_max(&payload_max) != 0)
    return EXIT_FAILURE;

printf("Read payload limit from %s\n", CXL_PAYLOAD_MAX_PATH);

rc = plan_transfer((size_t)st.st_size, payload_max);
return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int wait_for_background_completion(void)
{
    struct cxl_send_command cmd = {0};
    struct {
        uint8_t status;
        uint8_t reserved;
        uint16_t opcode;
        uint16_t returncode;
        uint16_t vendor_ext_status;
    } __attribute__((packed)) status = {0};
    int fd;
    int rc;
    int elapsed_ms = 0;

    fd = open(CXL_DEVICE, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("Cannot open CXL device for background status");
        return -1;
    }

    while (elapsed_ms < CXL_BG_TIMEOUT_SECONDS * 1000) {
        memset(&cmd, 0, sizeof(cmd));
        memset(&status, 0, sizeof(status));

        cmd.id = CXL_RAW_COMMAND_ID;
        cmd.raw.opcode = CXL_BG_STATUS_OPCODE;
        cmd.raw.rsvd = 0;
        cmd.in.size = 0;
        cmd.in.payload = 0;
        cmd.out.size = CXL_BG_STATUS_SIZE;
        cmd.out.payload = (__u64)(uintptr_t)&status;

        rc = ioctl(fd, CXL_MEM_SEND_COMMAND, &cmd);
        if (rc < 0) {
            perror("Background status ioctl failed");
            close(fd);
            return -1;
        }

        if (cmd.retval != 0) {
            fprintf(stderr, "Background status mailbox error: 0x%08X\n",
                    cmd.retval);
            close(fd);
            return -1;
        }

        if (cmd.out.size != CXL_BG_STATUS_SIZE) {
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

        if (opcode != CXL_FW_TRANSFER_OPCODE) {
            printf("\n");
            fprintf(stderr,
                    "Background opcode mismatch: expected 0x%04X, got 0x%04X\n",
                    CXL_FW_TRANSFER_OPCODE, opcode);
            close(fd);
            return -1;
        }

        if (!running) {
            printf("\n");
            close(fd);

            if (returncode != 0) {
                fprintf(stderr,
                        "Background TRANSFER_FW failed: return code 0x%04X\n",
                        returncode);
                return -1;
            }

            printf("Background TRANSFER_FW completed successfully.\n");
            return 0;
        }

        struct timespec delay = {.tv_sec = 0, .tv_nsec = 250000000L};
        while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {
        }
        elapsed_ms += 250;
    }

    printf("\n");
    fprintf(stderr, "Timed out waiting for TRANSFER_FW completion.\n");
    close(fd);
    return -1;
}

static int submit_transfer_request(
    const uint8_t *request,
    size_t request_size)
{
    struct cxl_send_command cmd = {0};
    int fd;
    int rc;

    if (request == NULL || request_size == 0 ||
        request_size > UINT32_MAX) {
        fprintf(stderr, "Error: invalid transfer request size or pointer.\n");
        return -1;
    }

    fd = open(CXL_DEVICE, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("Cannot open CXL device");
        return -1;
    }

    cmd.id = CXL_RAW_COMMAND_ID;
    cmd.raw.opcode = CXL_FW_TRANSFER_OPCODE;
    cmd.raw.rsvd = 0;
    cmd.in.size = (__u32)request_size;
    cmd.in.payload = (__u64)(uintptr_t)request;

    printf("Submitting TRANSFER_FW (opcode 0x%04X)...\n",
           CXL_FW_TRANSFER_OPCODE);

    rc = ioctl(fd, CXL_MEM_SEND_COMMAND, &cmd);
    if (rc < 0) {
        perror("CXL_MEM_SEND_COMMAND failed");
        close(fd);
        return -1;
    }

    printf("ioctl succeeded. Mailbox return value: %u (0x%08X)\n",
           cmd.retval, cmd.retval);

    if (close(fd) < 0)
        perror("Warning: failed to close CXL device");

    if (cmd.retval == CXL_BG_STARTED) {
        printf("Background TRANSFER_FW started; waiting for completion.\n");
        return wait_for_background_completion();
    }

    if (cmd.retval != 0) {
        fprintf(stderr, "TRANSFER_FW failed: mailbox return code 0x%08X\n",
                cmd.retval);
        return -1;
    }

    printf("TRANSFER_FW completed synchronously.\n");
    return 0;
}

