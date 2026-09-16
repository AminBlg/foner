#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 AminBlg
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Measures the health of the Kirigami application and prints one JSON object.
#
# Scope is src/ and tests/ only. The other implementations in sibling
# directories are deliberately not measured.
#
# Every metric is a count where lower is better, so gate.sh can compare two
# runs without knowing what any of them mean. A metric whose tool is missing
# reports null rather than 0: a missing tool must not look like a clean result.

set -uo pipefail

cd "$(dirname "$0")/.."
ROOT=$(pwd)
BUILD=${BUILD_DIR:-build}
export PATH="$HOME/.local/bin:$PATH"

# Generated sources are build output and would swamp every count.
EXCLUDES=(-x "*/foner_autogen/*" -x "*/fonerprobe_autogen/*" -x "*/.qt/*" -x "*/build/*")

json_null_or() { [ -n "$1" ] && echo "$1" || echo null; }

# ---- complexity -----------------------------------------------------------
CCN_OVER=null
CCN_MAX=null
if command -v lizard >/dev/null; then
    CSV=$(lizard src tests -l cpp "${EXCLUDES[@]}" --csv 2>/dev/null)
    # Numeric compare, not lexical: sort -rn on this column reports 24 above
    # 17 because it is comparing strings.
    CCN_OVER=$(printf '%s\n' "$CSV" | awk -F, '$2+0>10' | wc -l)
    CCN_MAX=$(printf '%s\n' "$CSV" | awk -F, '$2+0>m{m=$2+0} END{print m+0}')
fi

# ---- compiler warnings ----------------------------------------------------
# A fresh tree, because an incremental build reports warnings only for the
# files it happened to recompile.
GCC_WARNINGS=null
TIDY_DIR=$(mktemp -d)
CC_BUILD=$(mktemp -d)
if cmake -S . -B "$CC_BUILD" -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_FLAGS="-Wall -Wextra" >/dev/null 2>&1; then
    GCC_WARNINGS=$(cmake --build "$CC_BUILD" -j 2>&1 \
        | grep 'warning:' | grep -v autogen | wc -l)
fi

# ---- clang-tidy -----------------------------------------------------------
TIDY_WARNINGS=null
if command -v clang-tidy >/dev/null && [ -f "$CC_BUILD/compile_commands.json" ]; then
    # gcc passes -mno-direct-extern-access, which clang rejects outright. Left
    # in, clang-tidy reports a compiler error and analyses nothing, so the
    # count would read 0 and look like success.
    jq 'map(.command |= gsub("-mno-direct-extern-access"; ""))' \
        "$CC_BUILD/compile_commands.json" > "$TIDY_DIR/compile_commands.json" 2>/dev/null
    if [ -s "$TIDY_DIR/compile_commands.json" ]; then
        TIDY_WARNINGS=$(git ls-files 'src/*.cpp' | while read -r f; do
            clang-tidy -p "$TIDY_DIR" --quiet \
                --checks='-*,bugprone-*,performance-*,readability-misleading-indentation,readability-redundant-*' \
                "$f" 2>/dev/null
        done | grep -c 'warning:')
    fi
fi

# ---- qmllint --------------------------------------------------------------
# Unqualified access is not a defect here: every context property the app
# injects from C++ trips it, and there are hundreds.
QML_WARNINGS=null
QMLLINT=/usr/lib/qt6/bin/qmllint
if [ -x "$QMLLINT" ]; then
    QML_WARNINGS=$("$QMLLINT" src/app/qml/*.qml -I /usr/lib/qt6/qml \
        --unqualified disable 2>&1 | grep -c 'Warning:')
fi

# ---- duplication ----------------------------------------------------------
DUP_PERCENT=null
DUP_DIR=$(mktemp -d)
if command -v npx >/dev/null; then
    # Pinned exactly. jscpd 5.2.1 reports five more duplicated lines than
    # 5.2.0 on the same files, which failed CI on a README-only commit.
    # Bump the pin and the baseline together, in one commit.
    npx --yes jscpd@5.2.0 src --reporters json --output "$DUP_DIR" --silent \
        --min-lines 5 \
        --ignore "**/foner_autogen/**,**/fonerprobe_autogen/**,**/.qt/**" \
        >/dev/null 2>&1
    if [ -f "$DUP_DIR/jscpd-report.json" ]; then
        DUP_PERCENT=$(jq '.statistics.total.percentage' "$DUP_DIR/jscpd-report.json" 2>/dev/null)
    fi
fi

# ---- tests ----------------------------------------------------------------
# Assertions rather than test functions: a test that asserts nothing passes.
# This is the one metric where MORE is better, and gate.sh knows it.
ASSERTIONS=$(grep -hcE 'QCOMPARE|QVERIFY' tests/*.cpp 2>/dev/null | awk '{s+=$1} END{print s+0}')

# ---- QML file size --------------------------------------------------------
QML_OVER_200=$(git ls-files '*.qml' | while read -r f; do
    [ "$(wc -l < "$f")" -gt 200 ] && echo "$f"
done | wc -l)

# ---- untranslated strings -------------------------------------------------
# User-facing properties assigned a bare literal instead of going through
# i18n(). Deliberately narrow: only the properties a user actually reads.
UNTRANSLATED=$(git ls-files '*.qml' | xargs grep -nE \
    '^\s*(text|placeholderText|title|description|tooltip|Accessible\.name|Accessible\.description):\s*"[^"]' 2>/dev/null \
    | grep -vc 'i18n')

rm -r "$TIDY_DIR" "$DUP_DIR" "$CC_BUILD" 2>/dev/null

cat <<JSON
{
  "generated": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "commit": "$(git rev-parse --short HEAD)",
  "complexity_over_10": $(json_null_or "$CCN_OVER"),
  "complexity_max": $(json_null_or "$CCN_MAX"),
  "gcc_warnings": $(json_null_or "$GCC_WARNINGS"),
  "clang_tidy_warnings": $(json_null_or "$TIDY_WARNINGS"),
  "qml_warnings": $(json_null_or "$QML_WARNINGS"),
  "duplication_percent": $(json_null_or "$DUP_PERCENT"),
  "test_assertions": $ASSERTIONS,
  "qml_files_over_200_lines": $QML_OVER_200,
  "untranslated_strings": $UNTRANSLATED
}
JSON
