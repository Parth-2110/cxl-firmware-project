
#!/usr/bin/env python3

"""
Protocol-Level Fault Injection Report Generator

Input:
    logs/protocol-fault-injection.csv

Output:
    reports/protocol/Protocol-level-fault-report_<timestamp>.md

Uses only Python's standard library.
"""

import argparse
import csv
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path


FAULTS = {
    "invalid_action": {
        "description": "Tests whether the device rejects an unsupported firmware-transfer action.",
        "injection": "The action byte is overwritten with 0xFF.",
        "parameters": "Action = 0xFF; payload = 128 bytes.",
        "expected": "Mailbox return 0x02 (invalid action).",
        "location": "TRANSFER_FW request action field.",
    },
    "invalid_slot": {
        "description": "Tests whether the device prevents firmware transfer to the active slot.",
        "injection": "The target slot is changed to the currently active slot.",
        "parameters": "Action = FULL; target slot = active slot.",
        "expected": "Mailbox return 0x0B (invalid slot).",
        "location": "TRANSFER_FW request slot field.",
    },
    "out_of_sequence": {
        "description": "Tests whether the device rejects CONTINUE without a preceding INITIATE.",
        "injection": "A CONTINUE request is submitted directly.",
        "parameters": "Action = CONTINUE; offset = 0; payload = 128 bytes.",
        "expected": "Mailbox return 0x02 (invalid sequence).",
        "location": "TRANSFER_FW request sequence handling.",
    },
    "duplicate_offset": {
        "description": "Tests the device's handling of repeated firmware-transfer offsets.",
        "injection": "A valid INITIATE is followed by CONTINUE using the same offset.",
        "parameters": "INITIATE offset = 0; CONTINUE offset = 0.",
        "expected": "Mailbox return 0x00 (accepted by current QEMU implementation).",
        "location": "TRANSFER_FW CONTINUE offset field.",
    },
    "skipped_offset": {
        "description": "Tests the device's handling of gaps between consecutive transfer offsets.",
        "injection": "A valid INITIATE is followed by CONTINUE at offset 2.",
        "parameters": "INITIATE offset = 0; CONTINUE offset = 2.",
        "expected": "Mailbox return 0x00 (accepted by current QEMU implementation).",
        "location": "TRANSFER_FW CONTINUE offset field.",
    },
    "out_of_range_offset": {
        "description": "Tests whether the device rejects a transfer extending beyond the 32 MiB firmware limit.",
        "injection": "An INITIATE request is submitted at the maximum firmware boundary with additional payload.",
        "parameters": "Offset = 32 MiB / 128; payload = 128 bytes.",
        "expected": "Mailbox return 0x02 (rejected).",
        "location": "TRANSFER_FW offset and payload range.",
    },
    "bit_flipped_data": {
        "description": "Tests whether the device detects a modified firmware payload.",
        "injection": "The first payload byte is modified by flipping one bit.",
        "parameters": "First payload byte XOR 0x01.",
        "expected": "Mailbox return 0x00 (accepted by current QEMU implementation).",
        "location": "TRANSFER_FW payload data.",
    },
    "truncated_payload": {
        "description": "Tests the handling of a shorter-than-normal firmware payload.",
        "injection": "An INITIATE request is submitted with 64 payload bytes instead of 128.",
        "parameters": "Payload length = 64 bytes.",
        "expected": "Mailbox return 0x00 (accepted by current QEMU implementation).",
        "location": "TRANSFER_FW payload length.",
    },
    "corrupted_reserved_fields": {
        "description": "Tests whether the device validates reserved request-header fields.",
        "injection": "Reserved header bytes 2 and 8 are assigned nonzero values.",
        "parameters": "Byte 2 = 0xA5; byte 8 = 0x5A.",
        "expected": "Mailbox return 0x00 (accepted by current QEMU implementation).",
        "location": "TRANSFER_FW request header.",
    },
}

FAULT_ORDER = list(FAULTS)

REQUIRED_COLUMNS = {
    "run_id",
    "timestamp",
    "test",
    "step",
    "expected_mailbox_return",
    "actual_mailbox_return",
    "ioctl_status",
    "verdict",
    "note",
}


def load_csv(path):
    if not path.is_file():
        raise FileNotFoundError(f"CSV file not found: {path}")

    with path.open("r", newline="", encoding="utf-8-sig") as file:
        reader = csv.DictReader(file)

        if not reader.fieldnames:
            raise ValueError("CSV file is empty or has no header.")

        missing = REQUIRED_COLUMNS - set(reader.fieldnames)

        if missing:
            raise ValueError(
                "CSV header is incompatible. Missing columns: "
                + ", ".join(sorted(missing))
                + ". Preserve the existing CSV and use a new CSV file "
                  "or migrate its old records before appending."
            )

        rows = list(reader)

    if not rows:
        raise ValueError("CSV contains a header but no execution records.")

    return rows


def select_run(rows, requested_run_id=None):
    run_ids = [
        row["run_id"]
        for row in rows
        if row.get("run_id")
    ]

    if not run_ids:
        raise ValueError("No run IDs found in the CSV.")

    if requested_run_id:
        selected_id = requested_run_id
    else:
        selected_id = run_ids[-1]

    selected_rows = [
        row for row in rows
        if row.get("run_id") == selected_id
    ]

    if not selected_rows:
        raise ValueError(f"Run ID not found: {selected_id}")

    return selected_rows, selected_id


def classify_events(events):
    verdicts = [
        (event.get("verdict") or "").strip().upper()
        for event in events
    ]

    if not verdicts:
        return "NOT RECORDED"

    if "ERROR" in verdicts:
        return "ERROR"

    if "FAIL" in verdicts:
        return "FAIL"

    if all(value == "PASS" for value in verdicts):
        return "PASS"

    return "INCOMPLETE"


def format_value(value):
    return str(value).strip() if value not in (None, "") else "Not recorded"


def markdown_table(headers, rows):
    output = [
        "| " + " | ".join(headers) + " |",
        "| " + " | ".join(["---"] * len(headers)) + " |",
    ]

    for row in rows:
        cleaned = [
            str(value).replace("|", "\\|").replace("\n", " ")
            for value in row
        ]
        output.append("| " + " | ".join(cleaned) + " |")

    return "\n".join(output)


def generate_fault_section(name, events):
    metadata = FAULTS[name]

    fault_events = [
        event for event in events
        if event.get("step") != "cleanup_abort"
    ]

    cleanup_events = [
        event for event in events
        if event.get("step") == "cleanup_abort"
    ]

    verdict = classify_events(fault_events)
    cleanup_verdict = classify_events(cleanup_events)

    lines = [
        f"## {name}",
        "",
        f"**Description:** {metadata['description']}",
        "",
        f"**Injection location:** {metadata['location']}",
        "",
        f"**Injection method:** {metadata['injection']}",
        "",
        f"**Injection parameters:** {metadata['parameters']}",
        "",
        f"**Expected behaviour:** {metadata['expected']}",
        "",
        f"**Fault-test verdict:** {verdict}",
        "",
        f"**Cleanup ABORT verdict:** {cleanup_verdict}",
        "",
        "### Recorded execution steps",
        "",
    ]

    table_rows = []

    for event in events:
        table_rows.append([
            format_value(event.get("step")),
            format_value(event.get("expected_mailbox_return")),
            format_value(event.get("actual_mailbox_return")),
            format_value(event.get("ioctl_status")),
            format_value(event.get("verdict")),
            format_value(event.get("note")),
        ])

    lines.append(markdown_table(
        [
            "Step",
            "Expected return",
            "Actual return",
            "ioctl status",
            "Verdict",
            "Notes",
        ],
        table_rows,
    ))

    lines.extend([
        "",
        "### Interpretation",
        "",
        (
            "The fault-test verdict indicates whether the recorded "
            "mailbox response matched the expected response."
        ),
        "",
        (
            "An accepted malformed request is not automatically a failure "
            "when acceptance is the expected behaviour for the current "
            "QEMU implementation."
        ),
        "",
    ])

    return "\n".join(lines), verdict, cleanup_verdict


def build_report(rows, run_id, csv_path):
    grouped = defaultdict(list)

    for row in rows:
        grouped[row["test"]].append(row)

    initial_cleanup = grouped.pop("initial_cleanup", [])

    recorded_faults = [
        name for name in FAULT_ORDER
        if name in grouped
    ]

    missing_faults = [
        name for name in FAULT_ORDER
        if name not in grouped
    ]

    unexpected_faults = [
        name for name in grouped
        if name not in FAULTS
    ]

    passed = 0
    failed = 0
    errors = 0

    fault_results = {}
    cleanup_results = {}

    sections = []

    for name in recorded_faults:
        section, verdict, cleanup_verdict = generate_fault_section(
            name, grouped[name]
        )

        sections.append(section)
        fault_results[name] = verdict
        cleanup_results[name] = cleanup_verdict

        if verdict == "PASS":
            passed += 1
        elif verdict == "ERROR":
            errors += 1
        else:
            failed += 1

    if errors:
        overall = "ERROR"
    elif failed:
        overall = "FAIL"
    elif missing_faults:
        overall = "INCOMPLETE"
    else:
        overall = "PASS"

    timestamps = [
        row["timestamp"]
        for row in rows
        if row.get("timestamp")
    ]

    first_timestamp = timestamps[0] if timestamps else "Not recorded"
    last_timestamp = timestamps[-1] if timestamps else "Not recorded"

    initial_verdict = classify_events(initial_cleanup)

    lines = [
        "# Protocol-Level Fault Injection Report",
        "",
        f"**Generated:** {datetime.now().astimezone().isoformat(timespec='seconds')}",
        "",
        "## 1. Execution Information",
        "",
        f"- **Run ID:** `{run_id}`",
        f"- **Source CSV:** `{csv_path}`",
        f"- **First recorded event:** {first_timestamp}",
        f"- **Last recorded event:** {last_timestamp}",
        f"- **Total CSV records:** {len(rows)}",
        "- **Execution environment:** QEMU-emulated CXL Type 3 device",
        "",
        "## 2. Overall Results",
        "",
        markdown_table(
            ["Metric", "Result"],
            [
                ["Fault cases recorded", f"{len(recorded_faults)}/9"],
                ["PASS", passed],
                ["FAIL", failed],
                ["ERROR", errors],
                ["Missing fault cases", len(missing_faults)],
                ["Overall protocol result", overall],
            ],
        ),
        "",
        "## 3. Fault Case Summary",
        "",
        markdown_table(
            ["Fault case", "Fault-test verdict", "Cleanup ABORT verdict"],
            [
                [
                    name,
                    fault_results[name],
                    cleanup_results[name],
                ]
                for name in recorded_faults
            ],
        ),
        "",
        "## 4. Initial Cleanup",
        "",
        f"**Initial ABORT verdict:** {initial_verdict}",
        "",
    ]

    if initial_cleanup:
        initial_rows = [
            [
                format_value(event.get("step")),
                format_value(event.get("expected_mailbox_return")),
                format_value(event.get("actual_mailbox_return")),
                format_value(event.get("ioctl_status")),
                format_value(event.get("verdict")),
                format_value(event.get("note")),
            ]
            for event in initial_cleanup
        ]

        lines.append(markdown_table(
            [
                "Step",
                "Expected return",
                "Actual return",
                "ioctl status",
                "Verdict",
                "Notes",
            ],
            initial_rows,
        ))
    else:
        lines.append("Initial cleanup was not recorded.")

    lines.extend([
        "",
        "## 5. Individual Fault Results",
        "",
    ])

    lines.extend(sections)

    lines.extend([
        "## 6. Missing and Unexpected Tests",
        "",
    ])

    if missing_faults:
        lines.append("The following registered fault cases were not recorded:")
        lines.extend([f"- `{name}`" for name in missing_faults])
    else:
        lines.append("All nine registered fault cases were recorded.")

    if unexpected_faults:
        lines.extend([
            "",
            "Unexpected test names found in the CSV:",
            "",
        ])
        lines.extend([f"- `{name}`" for name in unexpected_faults])

    lines.extend([
        "",
        "## 7. Pass/Fail Criteria",
        "",
        "A fault case is considered PASS when:",
        "",
        "1. All its recorded fault-request and baseline steps pass.",
        "2. The actual mailbox return matches the expected mailbox return.",
        "3. No recorded ioctl error or failed step exists.",
        "",
        "The overall protocol suite is PASS only when all nine registered "
        "fault cases are present and every case passes.",
        "",
        "## 8. Recovery Validation Status",
        "",
        "| Validation criterion | Status |",
        "|---|---|",
        "| Fault request mailbox response | Recorded |",
        "| ioctl execution status | Recorded |",
        "| ABORT cleanup response | Recorded separately |",
        "| GET_FW_INFO after each fault | NOT TESTED |",
        "| Device responsiveness after each fault | NOT TESTED |",
        "| Last-known-good firmware bootability | NOT TESTED |",
        "| Subsequent clean firmware transfer | NOT TESTED |",
        "| Firmware activation | NOT PERFORMED |",
        "",
        "**Important:** A protocol-test PASS does not prove firmware validity, "
        "device recovery, firmware-slot bootability, or successful subsequent "
        "firmware transfer. The current logger does not record those "
        "measurements. They remain part of the wider Goal 5 validation work.",
        "",
        "## 9. Source Log",
        "",
        f"`{csv_path}`",
        "",
        "---",
        "",
        "*End of protocol-level fault injection report.*",
        "",
    ])

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Generate a Markdown report from protocol fault CSV logs."
    )

    parser.add_argument(
        "--csv",
        default="logs/protocol-fault-injection.csv",
        help="Path to the protocol fault CSV file.",
    )

    parser.add_argument(
        "--run-id",
        help="Generate a report for a specific run ID.",
    )

    parser.add_argument(
        "--output-dir",
        default="reports/protocol",
        help="Directory for generated Markdown reports.",
    )

    args = parser.parse_args()

    csv_path = Path(args.csv)
    output_dir = Path(args.output_dir)

    try:
        rows = load_csv(csv_path)
        selected_rows, run_id = select_run(rows, args.run_id)

        report = build_report(selected_rows, run_id, csv_path)

        output_dir.mkdir(parents=True, exist_ok=True)

        timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")

        output_path = output_dir / (
            f"Protocol-level-fault-report_{timestamp}.md"
        )

        output_path.write_text(report, encoding="utf-8")

        print()
        print("Protocol report generated successfully.")
        print(f"Run ID: {run_id}")
        print(f"Records included: {len(selected_rows)}")
        print(f"Report: {output_path.resolve()}")

    except (OSError, ValueError, csv.Error) as error:
        print(f"Report generation failed: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())