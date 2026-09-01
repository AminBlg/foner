#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 AminBlg
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Re-measures the application and refuses a regression against
# health-baseline.json.
#
# The ratchet only tightens. When a metric improves, this rewrites the
# baseline, so the improvement is what the next commit has to beat.
#
# BASELINE=path       compare against a different file
# UPDATE_BASELINE=0   report improvements without recording them

set -uo pipefail

cd "$(dirname "$0")/.."
BASELINE=${BASELINE:-health-baseline.json}
UPDATE=${UPDATE_BASELINE:-1}

if [ ! -f "$BASELINE" ]; then
    echo "gate: no baseline at $BASELINE. Run scripts/health.sh and commit its output." >&2
    exit 2
fi

CURRENT=$(mktemp)
trap 'rm -f "$CURRENT"' EXIT

echo "Measuring..." >&2
./scripts/health.sh > "$CURRENT" 2>/dev/null || {
    echo "gate: health.sh failed" >&2
    exit 2
}

# Lower is better for all of these.
LOWER_BETTER="complexity_over_10 complexity_max gcc_warnings clang_tidy_warnings
              qml_warnings duplication_percent qml_files_over_200_lines
              untranslated_strings"
# And higher for this one: deleting tests is a regression too.
HIGHER_BETTER="test_assertions"

FAILED=0
IMPROVED=0

check() {
    local key=$1 direction=$2
    local was now
    was=$(jq -r ".$key" "$BASELINE")
    now=$(jq -r ".$key" "$CURRENT")

    # A tool that vanished reports null. Treat that as a failure rather than
    # silently passing a metric nobody is measuring any more.
    if [ "$now" = "null" ]; then
        if [ "$was" = "null" ]; then
            printf '  %-28s %s\n' "$key" "not measured (unchanged)"
        else
            printf '  %-28s FAIL  was %s, now unmeasurable - is the tool installed?\n' "$key" "$was"
            FAILED=1
        fi
        return
    fi
    if [ "$was" = "null" ]; then
        printf '  %-28s %s (new metric, recorded)\n' "$key" "$now"
        IMPROVED=1
        return
    fi

    local worse
    if [ "$direction" = lower ]; then
        worse=$(awk -v a="$now" -v b="$was" 'BEGIN{print (a>b)?1:0}')
    else
        worse=$(awk -v a="$now" -v b="$was" 'BEGIN{print (a<b)?1:0}')
    fi

    if [ "$worse" = 1 ]; then
        printf '  %-28s FAIL  was %s, now %s\n' "$key" "$was" "$now"
        FAILED=1
    elif [ "$now" != "$was" ]; then
        printf '  %-28s improved: %s -> %s\n' "$key" "$was" "$now"
        IMPROVED=1
    else
        printf '  %-28s %s\n' "$key" "$now"
    fi
}

echo
echo "Code health against $BASELINE:"
for k in $LOWER_BETTER; do check "$k" lower; done
for k in $HIGHER_BETTER; do check "$k" higher; done
echo

if [ "$FAILED" = 1 ]; then
    echo "gate: a metric regressed. Fix it, or if the change is deliberate," >&2
    echo "      re-run scripts/health.sh and commit the new baseline with a" >&2
    echo "      commit message saying why it moved." >&2
    exit 1
fi

if [ "$IMPROVED" = 1 ] && [ "$UPDATE" = 1 ]; then
    cp "$CURRENT" "$BASELINE"
    echo "gate: passed, and the baseline tightened. Commit $BASELINE."
else
    echo "gate: passed."
fi
