#!/bin/sh
set -e

case "$1" in
    manager|"")
        shift || true
        exec /opt/kvdb/kvdb_manager "$@"
        ;;

    unit-tests)
        shift
        exec /opt/kvdb/tests/unit/kvdb_unit_tests "$@"
        ;;

    scenarios)
        shift

        for scenario in /opt/kvdb/tests/scenarios/*; do
            if [ -x "$scenario" ]; then
                echo "Running scenario: $(basename "$scenario")"
                "$scenario" "$@"
            fi
        done
        ;;

    scenario)
        shift
    
        if [ -z "$1" ]; then
            echo "Usage: scenario <scenario_name>"
            echo "Available scenarios:"
            ls -1 /opt/kvdb/tests/scenarios
            exit 1
        fi
    
        scenario_name="$1"
        shift
    
        exec "/opt/kvdb/tests/scenarios/$scenario_name" "$@"
        ;;

    tests)
        shift

        /opt/kvdb/tests/unit/kvdb_unit_tests "$@"

        for scenario in /opt/kvdb/tests/scenarios/*; do
            if [ -x "$scenario" ]; then
                echo "Running scenario: $(basename "$scenario")"
                "$scenario"
            fi
        done
        ;;

    *)
        echo "Unknown command: $1"
        echo "Available commands:"
        echo "  manager"
        echo "  tests"
        echo "  unit-tests"
        echo "  scenarios"
        echo "  scenario <name>"
        exit 1
        ;;
esac