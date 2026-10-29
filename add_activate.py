from pathlib import Path

path = Path("transfer_fw.c")
source = path.read_text()

define_anchor = "#define CXL_FW_TRANSFER_OPCODE 0x0201\n"
function_anchor = "int main(int argc, char **argv)\n"

if "CXL_FW_ACTIVATE_OPCODE" in source:
    raise SystemExit("Activation definitions already exist; no changes made.")

if source.count(define_anchor) != 1:
    raise SystemExit("Could not uniquely locate the opcode definitions.")

if source.count(function_anchor) != 1:
    raise SystemExit("Could not uniquely locate main().")

definitions = """#define CXL_FW_ACTIVATE_OPCODE 0x0202

struct cxl_fw_activate_request {
    uint8_t action;
    uint8_t slot;
} __attribute__((packed));

_Static_assert(sizeof(struct cxl_fw_activate_request) == 2,
               "ACTIVATE_FW request must be exactly 2 bytes");

"""

source = source.replace(
    define_anchor,
    define_anchor + definitions,
    1
)

activation_function = r"""
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

"""

source = source.replace(
    function_anchor,
    activation_function + function_anchor,
    1
)

main_anchor = """int main(int argc, char **argv)
{

 int submit_enabled = 0;
"""

main_replacement = """int main(int argc, char **argv)
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
        if (errno != 0 || slot_end == argv[2] || *slot_end != '\\0' ||
            slot_value > UINT8_MAX) {
            fprintf(stderr, "Error: invalid slot argument.\\n");
            return EXIT_FAILURE;
        }

        errno = 0;
        action_value = strtoul(argv[4], &action_end, 10);
        if (errno != 0 || action_end == argv[4] || *action_end != '\\0' ||
            action_value > UINT8_MAX) {
            fprintf(stderr, "Error: invalid action argument.\\n");
            return EXIT_FAILURE;
        }

        if (submit_activate_request((uint8_t)slot_value,
                                    (uint8_t)action_value) != 0)
            return EXIT_FAILURE;

        return EXIT_SUCCESS;
    }

"""

if source.count(main_anchor) != 1:
    raise SystemExit("Could not uniquely locate main() start; no changes saved.")

source = source.replace(main_anchor, main_replacement, 1)

help_anchor = '        printf("       %s --submit <firmware-image>\\\\n", argv[0]);'
if source.count(help_anchor) == 1:
    source = source.replace(
        help_anchor,
        help_anchor + '\n        printf("       %s --activate <slot> --action <0|1>\\\\n", argv[0]);',
        1
    )

path.write_text(source)
print("ACTIVATE_FW code added. Existing transfer logic was not intentionally changed.")
