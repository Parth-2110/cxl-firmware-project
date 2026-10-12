# Troubleshooting Log

Record each issue with the date, environment, command, observed output,
cause, fix, and final result.

## GitHub authentication — October 2026

### Issue

HTTPS clone failed with:

`Password authentication is not supported for Git operations.`

### Resolution

Installed GitHub CLI, authenticated using GitHub's device-login flow
through the Windows browser, and retried the clone.

### Result

Repository cloned successfully into:

`~/kernel-workspace/cxl-firmware-project`

### Lesson

GitHub HTTPS Git operations require an accepted authentication method;
a normal account password is not supported.
