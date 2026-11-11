#!/bin/bash

set -o pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR" || exit 1

PYTHON=".venv/bin/python"
REPORT_DIR="reports/goal5"

if [ ! -x "$PYTHON" ]; then
    echo "ERROR: Python virtual environment not found."
    exit 1
fi

mkdir -p "$REPORT_DIR"

MODE="${1:-}"

case "$MODE" in

    quick)
        echo "======================================"
        echo "          CXL QUICK DEMO"
        echo "======================================"

        sudo "$PYTHON" -m pytest -m quick \
            --junitxml="$REPORT_DIR/quick-results.xml" \
            | tee "$REPORT_DIR/quick-results.txt"
        ;;

    full)
        echo "======================================"
        echo "       CXL FULL REGRESSION"
        echo "======================================"

        sudo "$PYTHON" -m pytest -m full \
            --junitxml="$REPORT_DIR/full-results.xml" \
            | tee "$REPORT_DIR/full-results.txt"
        ;;

    *)
        echo "Usage: ./run-regression.sh {quick|full}"
        echo
        echo "Available modes:"
        echo "  quick  - Run nine fault-response tests"
        echo "  full   - Run nine fault and recovery tests"
        exit 2
        ;;
esac

TEST_EXIT=${PIPESTATUS[0]}
sudo chown -R "$(id -u):$(id -g)" "$REPORT_DIR"

if [ "$TEST_EXIT" -ne 0 ]; then
    echo "Regression execution FAILED."
else
    echo "Regression execution PASSED."
fi

exit "$TEST_EXIT"
