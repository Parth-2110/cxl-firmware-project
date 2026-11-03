#include "cxl_fw.h"

int transfer_fw_dry_run(int argc, char **argv)
{
    const char *file_path = NULL;
    unsigned int slot = 0;
    unsigned int payload_capacity = 0;
    uint64_t requested_size = 0;
    int have_size = 0;
    int dry_run_requested = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--dry-run") == 0) {
            dry_run_requested = 1;
        } else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            if (file_path != NULL || have_size) {
                fprintf(stderr,
                        "Specify exactly one of --file or --size.\n");
                return -1;
            }

            file_path = argv[++i];
        } else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            unsigned int parsed_size = 0;

            if (file_path != NULL || have_size ||
                parse_uint(argv[++i], &parsed_size) < 0) {
                fprintf(stderr,
                        "Invalid --size, or --file/--size supplied more "
                        "than once.\n");
                return -1;
            }

            requested_size = parsed_size;
            have_size = 1;
        } else if (strcmp(argv[i], "--slot") == 0 && i + 1 < argc) {
            if (parse_uint(argv[++i], &slot) < 0 || slot > UINT8_MAX) {
                fprintf(stderr, "Invalid slot number.\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--payload-capacity") == 0 &&
                   i + 1 < argc) {
            if (parse_uint(argv[++i], &payload_capacity) < 0) {
                fprintf(stderr, "Invalid payload capacity.\n");
                return -1;
            }
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            return -1;
        }
    }

    if (!dry_run_requested || (file_path == NULL && !have_size) ||
        slot == 0 || payload_capacity == 0) {
        fprintf(stderr,
                "Dry run requires --file PATH or --size BYTES, plus "
                "--slot N --payload-capacity BYTES --dry-run.\n");
        return -1;
    }

    if (payload_capacity < 2 * FW_TRANSFER_HEADER_SIZE ||
        payload_capacity % FW_TRANSFER_ALIGNMENT != 0) {
        fprintf(stderr,
                "Capacity must be a multiple of 128 and at least 256 "
                "bytes.\n");
        return -1;
    }

    uint64_t image_size = requested_size;

    if (file_path != NULL) {
        FILE *file = fopen(file_path, "rb");

        if (file == NULL) {
            perror("fopen firmware file");
            return -1;
        }

        if (fseeko(file, 0, SEEK_END) != 0) {
            perror("fseeko");
            fclose(file);
            return -1;
        }

        off_t file_size = ftello(file);

        if (file_size < 0) {
            perror("ftello");
            fclose(file);
            return -1;
        }

        if (fclose(file) != 0) {
            perror("fclose");
            return -1;
        }

        image_size = (uint64_t)file_size;
    }

    if (image_size == 0 || image_size > DRY_RUN_MAX_IMAGE_SIZE) {
        fprintf(stderr,
                "Image size must be between 1 byte and 32 MiB for this "
                "dry-run planner. This is an illustrative client limit.\n");
        return -1;
    }

    if (image_size % FW_TRANSFER_ALIGNMENT != 0) {
        fprintf(stderr,
                "Image size (%" PRIu64 ") is not a multiple of 128 bytes. "
                "Refusing to plan this transfer.\n",
                image_size);
        return -1;
    }

    unsigned int max_chunk =
        ((payload_capacity - FW_TRANSFER_HEADER_SIZE) /
         FW_TRANSFER_ALIGNMENT) * FW_TRANSFER_ALIGNMENT;

    if (max_chunk == 0) {
        fprintf(stderr, "Calculated maximum chunk size is zero.\n");
        return -1;
    }

    puts("CXL firmware transfer planner — DRY RUN");
    puts("---------------------------------------");

    if (file_path != NULL)
        printf("Firmware file:      %s\n", file_path);
    else
        puts("Input mode:         size-only (no firmware file)");

    printf("Image size:         %" PRIu64 " bytes\n", image_size);
    printf("Target slot:        %u\n", slot);
    printf("Supplied capacity:  %u bytes\n", payload_capacity);
    printf("Header size:        %u bytes\n", FW_TRANSFER_HEADER_SIZE);
    printf("Maximum data chunk: %u bytes\n", max_chunk);
    puts("32 MiB limit:       illustrative client-side limit");
    puts("\nNo device was opened and no CXL ioctl was issued.");
    puts("Slot validity and actual device capacity are not verified.\n");

    if (image_size + FW_TRANSFER_HEADER_SIZE <= payload_capacity) {
        printf("Planned commands: 1\n");
        printf("  1. FULL     slot=%u offset_units=0 data=%" PRIu64
               " bytes\n", slot, image_size);
    } else {
        uint64_t offset = 0;
        unsigned int part = 0;

        while (offset < image_size) {
            uint64_t remaining = image_size - offset;
            unsigned int chunk =
                remaining > max_chunk ? max_chunk : (unsigned int)remaining;

            const char *action;

            if (offset == 0)
                action = "INITIATE";
            else if ((uint64_t)chunk == remaining)
                action = "END";
            else
                action = "CONTINUE";

            printf("  %u. %-8s slot=%u offset_units=%" PRIu64
                   " data=%u bytes\n",
                   ++part, action, slot,
                   offset / FW_TRANSFER_ALIGNMENT, chunk);

            offset += chunk;
        }

        printf("\nPlanned commands: %u\n", part);
    }

    puts("\nDRY RUN COMPLETE: no firmware was transferred.");
    return 0;
}

int read_payload_max(unsigned int *payload_max)
{
    FILE *file = fopen("/sys/bus/cxl/devices/mem0/payload_max", "r");
    if (file == NULL) {
        perror("open /sys/bus/cxl/devices/mem0/payload_max");
        return -1;
    }

    unsigned int value = 0;
    int scanned = fscanf(file, "%u", &value);
    int close_rc = fclose(file);
    if (scanned != 1 || close_rc != 0 || value == 0) {
        fprintf(stderr, "Could not parse the device payload_max sysfs value.\n");
        return -1;
    }
    *payload_max = value;
    return 0;
}

int transfer_fw_execute(int argc, char **argv)
{
    const char *file_path = NULL;
    unsigned int slot = 0;
    unsigned int payload_capacity = 0;
    int execute_requested = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--execute") == 0) {
            execute_requested = 1;
        } else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            if (file_path != NULL) {
                fprintf(stderr, "Specify --file only once.\n");
                return -1;
            }
            file_path = argv[++i];
        } else if (strcmp(argv[i], "--slot") == 0 && i + 1 < argc) {
            if (parse_uint(argv[++i], &slot) < 0 || slot > UINT8_MAX) {
                fprintf(stderr, "Invalid slot number.\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--payload-capacity") == 0 && i + 1 < argc) {
            if (parse_uint(argv[++i], &payload_capacity) < 0) {
                fprintf(stderr, "Invalid payload capacity.\n");
                return -1;
            }
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            return -1;
        }
    }

    if (!execute_requested || file_path == NULL || slot == 0 || payload_capacity == 0) {
        fprintf(stderr,
                "Execution requires --file PATH --slot N --payload-capacity BYTES --execute.\n");
        return -1;
    }

    FILE *firmware = fopen(file_path, "rb");
    if (firmware == NULL) {
        perror("open firmware image");
        return -1;
    }
    if (fseeko(firmware, 0, SEEK_END) != 0) {
        perror("seek firmware image");
        fclose(firmware);
        return -1;
    }
    off_t file_size_signed = ftello(firmware);
    if (file_size_signed <= 0) {
        fprintf(stderr, "Firmware image is empty or its size could not be read.\n");
        fclose(firmware);
        return -1;
    }
    uint64_t image_size = (uint64_t)file_size_signed;
    if (image_size > FW_TRANSFER_MAX_IMAGE_SIZE ||
        image_size % FW_TRANSFER_ALIGNMENT != 0) {
        fprintf(stderr,
                "Firmware image must be <=32 MiB and a multiple of 128 bytes.\n");
        fclose(firmware);
        return -1;
    }
    if (fseeko(firmware, 0, SEEK_SET) != 0) {
        perror("rewind firmware image");
        fclose(firmware);
        return -1;
    }

    unsigned int actual_payload_max = 0;
    if (read_payload_max(&actual_payload_max) != 0) {
        fclose(firmware);
        return -1;
    }
    if (payload_capacity > actual_payload_max) {
        fprintf(stderr,
                "Requested payload capacity %u exceeds device payload_max %u.\n",
                payload_capacity, actual_payload_max);
        fclose(firmware);
        return -1;
    }
    if (payload_capacity < 2U * FW_TRANSFER_HEADER_SIZE ||
        payload_capacity % FW_TRANSFER_ALIGNMENT != 0) {
        fprintf(stderr,
                "Payload capacity must be a multiple of 128 and at least 256 bytes.\n");
        fclose(firmware);
        return -1;
    }

    int fd = open(CXL_DEVICE, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open " CXL_DEVICE);
        fclose(firmware);
        return -1;
    }

    struct fw_info info;
    if (read_fw_info_for_transfer(fd, &info) != 0) {
        close(fd);
        fclose(firmware);
        return -1;
    }
    unsigned int active_slot = info.slot_info & 0x07U;
    if (slot > info.num_slots || slot > 4U) {
        fprintf(stderr, "Target slot %u is outside the device's reported slot range (%u).\n",
                slot, info.num_slots);
        close(fd);
        fclose(firmware);
        return -1;
    }
    if (slot == active_slot) {
        fprintf(stderr,
                "Refusing transfer to active slot %u. Choose a different inactive slot.\n",
                active_slot);
        close(fd);
        fclose(firmware);
        return -1;
    }

    unsigned int max_chunk =
        ((payload_capacity - FW_TRANSFER_HEADER_SIZE) / FW_TRANSFER_ALIGNMENT) *
        FW_TRANSFER_ALIGNMENT;
    uint8_t *request = malloc(payload_capacity);
    if (request == NULL) {
        perror("malloc transfer request");
        close(fd);
        fclose(firmware);
        return -1;
    }

    printf("REAL CXL TRANSFER REQUESTED\n"
           "Device: %s\nFirmware: %s\nImage size: %" PRIu64 " bytes\n"
           "Target slot: %u (reported active slot: %u)\n"
           "Mailbox payload capacity: %u bytes\nMaximum data chunk: %u bytes\n\n",
           CXL_DEVICE, file_path, image_size, slot, active_slot,
           payload_capacity, max_chunk);
    fprintf(stderr,
            "WARNING: this will write firmware data to the QEMU-emulated CXL device.\n"
            "It does not activate firmware, but a failed transfer may leave the target slot incomplete.\n"
            "Type TRANSFER in uppercase to continue: ");
    char confirmation[32] = {0};
    if (fgets(confirmation, sizeof(confirmation), stdin) == NULL ||
        strcmp(confirmation, "TRANSFER\n") != 0) {
        fprintf(stderr, "Confirmation not received; no transfer command was sent.\n");
        free(request);
        close(fd);
        fclose(firmware);
        return -1;
    }

    int rc = 0;
    uint64_t offset = 0;
    unsigned int part = 0;
    while (offset < image_size) {
        uint64_t remaining = image_size - offset;
        size_t chunk_size = remaining > max_chunk ? max_chunk : (size_t)remaining;
        enum fw_transfer_action action;
        if (image_size <= max_chunk)
            action = FW_ACTION_FULL;
        else if (offset == 0)
            action = FW_ACTION_INITIATE;
        else if ((uint64_t)chunk_size == remaining)
            action = FW_ACTION_END;
        else
            action = FW_ACTION_CONTINUE;

        uint8_t *chunk_copy = malloc(chunk_size);
        if (chunk_copy == NULL) {
            perror("malloc firmware chunk");
            rc = -1;
            break;
        }
        if (fread(chunk_copy, 1, chunk_size, firmware) != chunk_size) {
            fprintf(stderr, "Could not read firmware chunk at offset %" PRIu64 ".\n", offset);
            free(chunk_copy);
            rc = -1;
            break;
        }
        size_t request_size = 0;
        if (build_fw_transfer_request(request, payload_capacity, &request_size,
                                      action, slot,
                                      (uint32_t)(offset / FW_TRANSFER_ALIGNMENT),
                                      chunk_copy, chunk_size) != 0) {
            free(chunk_copy);
            rc = -1;
            break;
        }
        free(chunk_copy);

        uint32_t mailbox_return = UINT32_MAX;
        int out_size = send_raw_command(fd, CXL_FW_TRANSFER_OPCODE,
                                        request, request_size, NULL, 0,
                                        &mailbox_return);
        if (out_size < 0) {
            rc = -1;
            break;
        }
        /*
         * The PCI mailbox transport waits for background completion before
         * returning from the ioctl. Therefore cmd.retval (mailbox_return)
         * contains the final device return code, not the initial BACKGROUND
         * status. Treat every non-success mailbox return code as failure.
         */
        if (mailbox_return != CXL_MBOX_SUCCESS) {
            fprintf(stderr,
                    "TRANSFER_FW part %u failed with final mailbox return code 0x%x.\n",
                    part + 1, mailbox_return);
            rc = -1;
            break;
        }
        printf("Part %u: action=%u offset_units=%" PRIu64
               " data=%zu bytes final_mailbox_return=0x%x\n",
               ++part, (unsigned int)action,
               offset / FW_TRANSFER_ALIGNMENT, chunk_size, mailbox_return);
        offset += chunk_size;
    }

    if (rc == 0) {
        int extra = fgetc(firmware);
        if (extra != EOF || ferror(firmware)) {
            fprintf(stderr,
                    "Firmware file changed or could not be read completely during transfer.\n");
            rc = -1;
        }
    }
    if (rc == 0)
        printf("TRANSFER_FW sequence completed for %" PRIu64
               " bytes. Firmware was NOT activated.\n", image_size);
    else
        fprintf(stderr,
                "Transfer did not complete successfully. The target slot may be incomplete; do not activate it.\n");

    free(request);
    if (close(fd) != 0) {
        perror("close CXL device");
        rc = -1;
    }
    if (fclose(firmware) != 0) {
        perror("close firmware file");
        rc = -1;
    }
    return rc;
}