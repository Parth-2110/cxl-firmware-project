#!/usr/bin/env python3
"""Generate a Markdown report from QEMU CXL process-fault JSONL logs.

Run on the Arch Linux host, normally from ~/Projects/qemu.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path

ROOT = Path.home() / "Projects/qemu"
DEFAULT_LOG_DIR = ROOT / "process-fault-logs"
DEFAULT_OUTPUT_DIR = ROOT / "process-fault-reports"

TESTS = [
    {
        "label": "PF1",
        "checkpoint": "PF1",
        "continue_index": None,
        "title": "Before INITIATE",
        "description": (
            "Injects a QEMU process termination immediately before the "
            "firmware-transfer INITIATE request is submitted."
        ),
        "expected": (
            "The guest client reaches the PF1 checkpoint, QEMU is terminated "
            "with SIGKILL, and a fresh QEMU instance boots with SSH and "
            "/dev/cxl/mem0 available."
        ),
    },
    {
        "label": "PF2",
        "checkpoint": "PF2",
        "continue_index": None,
        "title": "After INITIATE",
        "description": (
            "Injects a QEMU process termination after the firmware-transfer "
            "INITIATE request has been submitted."
        ),
        "expected": (
            "The guest client reaches the PF2 checkpoint, QEMU is terminated "
            "with SIGKILL, and a fresh QEMU instance boots with SSH and "
            "/dev/cxl/mem0 available."
        ),
    },
    {
        "label": "PF3-1",
        "checkpoint": "PF3",
        "continue_index": 1,
        "title": "After CONTINUE 1",
        "description": (
            "Injects a QEMU process termination after the first CONTINUE "
            "request in the firmware-transfer sequence."
        ),
        "expected": (
            "The guest client reaches PF3 at CONTINUE index 1, QEMU is "
            "terminated with SIGKILL, and a fresh QEMU instance boots with "
            "SSH and /dev/cxl/mem0 available."
        ),
    },
    {
        "label": "PF3-2",
        "checkpoint": "PF3",
        "continue_index": 2,
        "title": "After CONTINUE 2",
        "description": (
            "Injects a QEMU process termination after the second CONTINUE "
            "request in the firmware-transfer sequence."
        ),
        "expected": (
            "The guest client reaches PF3 at CONTINUE index 2, QEMU is "
            "terminated with SIGKILL, and a fresh QEMU instance boots with "
            "SSH and /dev/cxl/mem0 available."
        ),
    },
    {
        "label": "PF4",
        "checkpoint": "PF4",
        "continue_index": None,
        "title": "After final CONTINUE, before END",
        "description": (
            "Injects a QEMU process termination after the final CONTINUE "
            "request and before the firmware-transfer END request."
        ),
        "expected": (
            "The guest client reaches the PF4 checkpoint, QEMU is terminated "
            "with SIGKILL, and a fresh QEMU instance boots with SSH and "
            "/dev/cxl/mem0 available."
        ),
    },
]
TEST_BY_LABEL = {test["label"]: test for test in TESTS}

REQUIRED_PASS_EVENTS = [
    ("checkpoint_received", None),
    ("qemu_sigkill", None),
    ("qemu_started", "after_fault"),
    ("ssh_service_ready", "after_fault"),
    ("ssh_authenticated", "after_fault"),
    ("guest_ready", "after_fault"),
    ("test_complete", None),
]


def read_logs(log_dir: Path) -> list[dict]:
    if not log_dir.is_dir():
        raise FileNotFoundError(f"Log directory not found: {log_dir}")

    records = []
    for path in sorted(log_dir.glob("*.jsonl")):
        try:
            with path.open("r", encoding="utf-8") as file:
                for line_number, line in enumerate(file, start=1):
                    if not line.strip():
                        continue
                    try:
                        record = json.loads(line)
                    except json.JSONDecodeError as exc:
                        raise ValueError(
                            f"Invalid JSON in {path}, line {line_number}: {exc}"
                        ) from exc
                    record["_source_file"] = str(path)
                    records.append(record)
        except OSError as exc:
            raise OSError(f"Cannot read {path}: {exc}") from exc

    if not records:
        raise ValueError(f"No JSONL records found in {log_dir}")
    return records


def select_suite(records: list[dict], requested_suite: str | None):
    grouped = defaultdict(list)
    for record in records:
        suite_id = record.get("suite_id")
        if suite_id:
            grouped[str(suite_id)].append(record)

    if requested_suite:
        if requested_suite not in grouped:
            raise ValueError(f"Suite ID not found in logs: {requested_suite}")
        suite_id = requested_suite
    else:
        if not grouped:
            raise ValueError(
                "No suite_id fields found. These logs may be from the old "
                "single-test controller; rerun with the updated controller."
            )
        suite_id = max(
            grouped,
            key=lambda key: max(
                str(row.get("time", "")) for row in grouped[key]
            ),
        )

    return grouped[suite_id], suite_id


def event_matches(record: dict, event_name: str, phase: str | None) -> bool:
    if record.get("event") != event_name:
        return False
    return phase is None or record.get("phase") == phase


def classify_test(records: list[dict]) -> tuple[str, list[str]]:
    events = [record for record in records if record.get("event")]
    errors = [
        record for record in events
        if record.get("event") == "controller_error"
        or record.get("event") == "guest_client_exited_before_checkpoint"
    ]
    if errors:
        return "ERROR", [str(item.get("error") or item.get("event")) for item in errors]

    missing = []
    for event_name, phase in REQUIRED_PASS_EVENTS:
        if not any(event_matches(record, event_name, phase) for record in events):
            missing.append(
                event_name if phase is None else f"{event_name} (phase={phase})"
            )
    if missing:
        return "INCOMPLETE", missing
    return "PASS", []


def fmt(value) -> str:
    if value is None or value == "":
        return "Not recorded"
    return str(value).replace("|", "\\|").replace("\n", " ")


def markdown_table(headers: list[str], rows: list[list[object]]) -> str:
    lines = [
        "| " + " | ".join(headers) + " |",
        "| " + " | ".join("---" for _ in headers) + " |",
    ]
    for row in rows:
        lines.append("| " + " | ".join(fmt(value) for value in row) + " |")
    return "\n".join(lines)


def first_event(records: list[dict], name: str, phase: str | None = None):
    for record in records:
        if event_matches(record, name, phase):
            return record
    return None


def build_test_section(test: dict, records: list[dict]) -> tuple[str, str]:
    verdict, missing_or_errors = classify_test(records)
    checkpoint = first_event(records, "checkpoint_received")
    kill = first_event(records, "qemu_sigkill")
    restarted = first_event(records, "qemu_started", "after_fault")
    ready = first_event(records, "guest_ready", "after_fault")
    complete = first_event(records, "test_complete")

    checkpoint_rows = []
    if checkpoint:
        checkpoint_rows.append([
            "Checkpoint", checkpoint.get("checkpoint"),
            "Action", checkpoint.get("action"),
        ])
        checkpoint_rows.append([
            "Transfer part", checkpoint.get("part"),
            "Offset", checkpoint.get("offset"),
        ])
        checkpoint_rows.append([
            "Completed flag", checkpoint.get("completed"),
            "Peer", checkpoint.get("peer"),
        ])

    timeline = []
    for record in records:
        timeline.append([
            record.get("time"),
            record.get("event"),
            record.get("phase"),
            record.get("result") or record.get("error") or record.get("cxl_device"),
        ])

    lines = [
        f"## {test['label']} — {test['title']}",
        "",
        f"**Description:** {test['description']}",
        "",
        f"**Expected behaviour:** {test['expected']}",
        "",
        f"**Fault-test verdict:** {verdict}",
        "",
        "### Checkpoint details",
        "",
    ]
    if checkpoint_rows:
        lines.append(markdown_table(
            ["Field", "Value", "Field", "Value"], checkpoint_rows
        ))
    else:
        lines.append("The expected checkpoint notification was not recorded.")

    lines.extend([
        "",
        "### Process interruption and restart",
        "",
        markdown_table(
            ["Validation", "Recorded value"],
            [
                ["Checkpoint received", "Yes" if checkpoint else "No"],
                ["QEMU SIGKILL", "Yes" if kill else "No"],
                ["Restarted QEMU PID", restarted.get("pid") if restarted else None],
                ["SSH restored after restart",
                 "Yes" if first_event(records, "ssh_service_ready", "after_fault") else "No"],
                ["SSH authentication after restart",
                 "Yes" if first_event(records, "ssh_authenticated", "after_fault") else "No"],
                ["/dev/cxl/mem0 after restart",
                 ready.get("cxl_device") if ready else "Not verified"],
                ["Test completion event", "Yes" if complete else "No"],
            ],
        ),
        "",
        "### Event timeline",
        "",
        markdown_table(
            ["Timestamp (UTC)", "Event", "Phase", "Details"],
            timeline,
        ),
        "",
    ])

    if verdict == "INCOMPLETE":
        lines.extend([
            "**Missing validation events:**",
            "",
            *[f"- `{item}`" for item in missing_or_errors],
            "",
        ])
    elif verdict == "ERROR":
        lines.extend([
            "**Controller errors:**",
            "",
            *[f"- {item}" for item in missing_or_errors],
            "",
        ])

    lines.extend([
        "### Interpretation",
        "",
        (
            "PASS means the configured process checkpoint was received, QEMU "
            "was terminated, a new QEMU process was started, and the guest's "
            "SSH service and `/dev/cxl/mem0` were verified after restart."
        ),
        "",
        (
            "This result does not establish firmware-transfer resumption, "
            "firmware-slot integrity, firmware persistence, device recovery "
            "at the firmware protocol level, or successful firmware activation."
        ),
        "",
    ])
    return "\n".join(lines), verdict


def build_report(records: list[dict], suite_id: str, log_dir: Path) -> str:
    grouped = defaultdict(list)
    for record in records:
        label = record.get("test")
        if label:
            grouped[str(label)].append(record)

    results = {}
    sections = []
    for test in TESTS:
        test_records = grouped.get(test["label"], [])
        if test_records:
            section, verdict = build_test_section(test, test_records)
        else:
            verdict = "NOT RECORDED"
            section = (
                f"## {test['label']} — {test['title']}\n\n"
                "**Fault-test verdict:** NOT RECORDED\n\n"
                "No log records were found for this test.\n"
            )
        results[test["label"]] = verdict
        sections.append(section)

    counts = {
        verdict: sum(1 for value in results.values() if value == verdict)
        for verdict in ("PASS", "FAIL", "ERROR", "INCOMPLETE", "NOT RECORDED")
    }
    if counts["ERROR"]:
        overall = "ERROR"
    elif counts["FAIL"]:
        overall = "FAIL"
    elif counts["INCOMPLETE"] or counts["NOT RECORDED"]:
        overall = "INCOMPLETE"
    else:
        overall = "PASS"

    times = [str(record.get("time")) for record in records if record.get("time")]
    test_ids = sorted({str(record.get("test_id")) for record in records
                       if record.get("test_id")})
    source_files = sorted({str(record.get("_source_file")) for record in records
                           if record.get("_source_file")})

    lines = [
        "# Process-Level Fault Injection Report",
        "",
        f"**Generated:** {datetime.now().astimezone().isoformat(timespec='seconds')}",
        "",
        "## 1. Execution Information",
        "",
        f"- **Suite ID:** `{suite_id}`",
        f"- **Source log directory:** `{log_dir}`",
        f"- **Test IDs:** {', '.join(f'`{item}`' for item in test_ids) or 'Not recorded'}",
        f"- **First recorded event:** {min(times) if times else 'Not recorded'}",
        f"- **Last recorded event:** {max(times) if times else 'Not recorded'}",
        f"- **Total JSONL records:** {len(records)}",
        "- **Execution environment:** QEMU-emulated CXL Type 3 device; Ubuntu guest controlled from Arch Linux host",
        "",
        "## 2. Overall Results",
        "",
        markdown_table(
            ["Metric", "Result"],
            [
                ["Process-fault cases recorded", f"{5 - counts['NOT RECORDED']}/5"],
                ["PASS", counts["PASS"]],
                ["FAIL", counts["FAIL"]],
                ["ERROR", counts["ERROR"]],
                ["INCOMPLETE", counts["INCOMPLETE"]],
                ["Not recorded", counts["NOT RECORDED"]],
                ["Overall process-level result", overall],
            ],
        ),
        "",
        "## 3. Fault Case Summary",
        "",
        markdown_table(
            ["Fault checkpoint", "Injection point", "Verdict"],
            [[test["label"], test["title"], results[test["label"]]]
             for test in TESTS],
        ),
        "",
        "## 4. Individual Fault Results",
        "",
        *sections,
        "## 5. Pass/Fail Criteria",
        "",
        "A process-level test is considered PASS only when all of the following are recorded:",
        "",
        "1. The guest client sends the expected checkpoint notification.",
        "2. The controller terminates the running QEMU process with SIGKILL.",
        "3. The controller starts QEMU again.",
        "4. The Ubuntu SSH service becomes available and SSH authentication succeeds.",
        "5. `/dev/cxl/mem0` is present in the restarted guest.",
        "6. The controller records `test_complete`.",
        "",
        "The overall suite is PASS only when all five checkpoints pass.",
        "",
        "## 6. Validation Scope and Limitations",
        "",
        "| Validation criterion | Status |",
        "|---|---|",
        "| Process checkpoint reached | Recorded per test |",
        "| QEMU process termination | Recorded per test |",
        "| QEMU restart | Recorded per test |",
        "| Guest SSH availability after restart | Recorded per test |",
        "| `/dev/cxl/mem0` availability after restart | Recorded per test |",
        "| Firmware transfer resumption | NOT TESTED |",
        "| Firmware image/slot integrity | NOT TESTED |",
        "| Firmware persistence across QEMU restart | NOT TESTED |",
        "| Firmware activation | NOT PERFORMED |",
        "",
        "**Important:** A process-level PASS demonstrates QEMU restart and basic guest/CXL-device availability only. It does not prove that an interrupted firmware transfer can resume or that firmware state is valid or persistent.",
        "",
        "## 7. Source Logs",
        "",
        *[f"- `{path}`" for path in source_files],
        "",
        "---",
        "",
        "*End of process-level fault injection report.*",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a Markdown report from process-fault JSONL logs."
    )
    parser.add_argument("--log-dir", type=Path, default=DEFAULT_LOG_DIR,
                        help="Directory containing controller JSONL logs.")
    parser.add_argument("--suite-id",
                        help="Report a specific suite; defaults to the latest suite.")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR,
                        help="Directory for generated Markdown reports.")
    args = parser.parse_args()

    try:
        records = read_logs(args.log_dir)
        selected, suite_id = select_suite(records, args.suite_id)
        report = build_report(selected, suite_id, args.log_dir)
        args.output_dir.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        output = args.output_dir / f"Process-level-fault-report_{stamp}.md"
        output.write_text(report, encoding="utf-8")
        print("\nProcess-level report generated successfully.")
        print(f"Suite ID: {suite_id}")
        print(f"Records included: {len(selected)}")
        print(f"Report: {output.resolve()}")
        return 0
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"Report generation failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
