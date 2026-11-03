#include "cxl_fw.h"

void usage(const char *program)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s query\n"
            "  %s get-fw-info\n"
            "  %s abort-fw-transfer\n"
            "  %s test-header\n"
            "  %s test-transfer-request\n"
            "  %s transfer-fw --file PATH --slot N --payload-capacity BYTES --dry-run\n"
            "  %s transfer-fw --file PATH --slot N --payload-capacity BYTES --execute\n"
            "  %s fault-inject --all [--log PATH] [--dry-run]\n\n"
            "Commands:\n"
            "  query                  List kernel-exposed commands\n"
            "  get-fw-info            Read firmware information\n"
            "  abort-fw-transfer      Clear a stuck transfer after confirmation\n"
            "  test-header            Test header bytes locally\n"
            "  test-transfer-request  Test complete requests locally\n"
            "  transfer-fw            Dry-run by default; --execute writes firmware\n"
            "  fault-inject --all     Run all nine protocol-level fault experiments\n",
            program, program, program, program, program, program, program,
            program);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "abort-fw-transfer") == 0)
        return abort_fw_transfer_execute() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    if (argc == 2 && strcmp(argv[1], "test-header") == 0)
        return test_transfer_header() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    if (argc == 2 && strcmp(argv[1], "test-transfer-request") == 0)
        return test_transfer_request() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

    if (argc >= 2 && strcmp(argv[1], "fault-inject") == 0)
        return fault_inject_execute(argc, argv) == 0
             ? EXIT_SUCCESS : EXIT_FAILURE;

    if (argc >= 2 && strcmp(argv[1], "transfer-fw") == 0) {
        int dry_run = 0;
        int execute = 0;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--dry-run") == 0)
                dry_run = 1;
            if (strcmp(argv[i], "--execute") == 0)
                execute = 1;
        }
        if (dry_run && execute) {
            fprintf(stderr, "Choose either --dry-run or --execute, not both.\n");
            return EXIT_FAILURE;
        }
        if (execute)
            return transfer_fw_execute(argc, argv) == 0
                 ? EXIT_SUCCESS : EXIT_FAILURE;
        if (dry_run)
            return transfer_fw_dry_run(argc, argv) == 0
                 ? EXIT_SUCCESS : EXIT_FAILURE;
        fprintf(stderr, "No transfer mode selected; use --dry-run or --execute.\n");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (argc != 2 ||
        (strcmp(argv[1], "query") != 0 &&
         strcmp(argv[1], "get-fw-info") != 0)) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    int fd = open(CXL_DEVICE, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("open " CXL_DEVICE);
        return EXIT_FAILURE;
    }

    int rc;
    if (strcmp(argv[1], "query") == 0)
        rc = query_commands(fd);
    else
        rc = get_fw_info(fd);

    if (close(fd) < 0) {
        perror("close");
        return EXIT_FAILURE;
    }

    return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
