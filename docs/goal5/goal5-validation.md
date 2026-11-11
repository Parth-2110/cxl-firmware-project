
# Goal 5 — Validation, Regression Suite & Reporting

## 1. Objective

Convert the Goal 4 fault catalogue into a repeatable automated regression suite with clear pass/fail criteria.

## 2. Test Environment

- Ubuntu guest running inside QEMU.
- QEMU-emulated CXL Type-3 device.
- CXL device interface: /dev/cxl/mem0.
- C userspace firmware client.
- Python pytest framework.

## 3. Automated Fault Catalogue

The regression suite covers nine fault-injection cases:

1. invalid-action
2. invalid-slot
3. out-of-range-offset
4. bit-flip
5. truncated-payload
6. continue-without-initiate
7. end-without-initiate
8. duplicate-offset
9. out-of-order-offset

Each case checks the expected mailbox response.

## 4. Quick Demo Mode

Command:

    ./run-regression.sh quick

Quick Demo:
- Executes all nine fault-injection cases.
- Checks expected mailbox responses.
- Detects command failures and timeouts.
- Generates separate TXT and JUnit XML reports.

Quick Demo does not perform device health checks or subsequent clean firmware updates.

## 5. Full Regression Mode

Command:

    ./run-regression.sh full

For each fault case, Full Regression performs:

1. Fault injection and expected mailbox response verification.
2. Get FW Info to check device responsiveness.
3. A subsequent clean firmware update.
4. Timeout and command completion checks.

The suite produces separate TXT and JUnit XML reports.

## 6. Pass/Fail Criteria

A fault case passes when:

- Fault injection completes without timeout.
- The C program exits successfully.
- The expected mailbox response is confirmed.
- Get FW Info completes successfully in Full Regression.
- A subsequent clean firmware update completes successfully in Full Regression.

Any failed assertion or timeout results in a failed test.

## 7. Test Reports

Reports are stored in:

    reports/goal5/

Quick Demo:
- quick-results.txt
- quick-results.xml

Full Regression:
- full-results.txt
- full-results.xml

## 8. Known Limitations

The current QEMU environment does not establish physical firmware bootability or persistent firmware state.

Therefore:

- Last-known-good slot bootability is not verified.
- Firmware persistence across device or QEMU restart is not verified.
- A successful Get FW Info response proves responsiveness, not bootability.
- A successful clean update proves command completion, not successful physical firmware activation.
- Background-operation completion and recovery after a real device failure are not established by these tests.

These limitations must not be reported as passing tests.


## 9. Actual Test Execution Results

Both automated modes were executed successfully.

### Quick Demo

- Tests collected: 9
- Passed: 9
- Failed: 0
- Execution time: 36.06 seconds
- Result: PASS

Report files:
- reports/goal5/quick-results.txt
- reports/goal5/quick-results.xml

### Full Regression

- Tests collected: 9
- Passed: 9
- Failed: 0
- Execution time: 306.33 seconds (5 minutes 6 seconds)
- Result: PASS

Report files:
- reports/goal5/full-results.txt
- reports/goal5/full-results.xml

### Interpretation

All nine fault cases passed their expected mailbox-response checks.

For every fault case, Full Regression also confirmed:
- Get FW Info completed successfully.
- A subsequent clean firmware update completed successfully.
- No configured subprocess timeout occurred.

These results demonstrate successful command-level fault handling and subsequent update operation in the QEMU environment.

They do not establish physical firmware bootability, firmware persistence, or successful activation.

## 10. Conclusion

Goal 5 provides two automated validation modes:

- Quick Demo for rapid fault-response demonstration.
- Full Regression for fault-response, device-responsiveness, and subsequent clean-update checks.

The suite provides repeatable pass/fail results and machine-readable JUnit XML reports.

