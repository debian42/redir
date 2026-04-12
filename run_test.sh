#!/bin/bash
# run_test.sh - Bash-Port mit korrekter Numerierung und Log-Persistenz
set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
GRAY='\033[0;90m'
NC='\033[0m' # No Color

function Run-Validation-Pass() {
    local mode_name="$1"
    local enable_u16="$2"
    
    echo -e "\n${CYAN}>>> VALIDIERUNGS-PASS: $mode_name <<<${NC}"
    [ "$enable_u16" = "true" ] && export REDIR_ENABLE_U16TEXT=1 || unset REDIR_ENABLE_U16TEXT
    
    local report=()

    # Vorbereitung
    rm -f tester tester_org tester_conf.env test_err_*.txt test_in.txt 2>/dev/null
    cp redir tester && cp mock_org tester_org && chmod +x tester tester_org

    function finalize_test() {
        local test_num="$1"
        local title="$2"
        local is_success="$3"
        local err_file="$4"
        if [ "$is_success" = "true" ]; then
            [ -f "$err_file" ] && rm "$err_file"
            echo -e "${GREEN}[OK] $test_num. $title${NC}"
            return 0
        else
            echo -e "${YELLOW}  [!] Test $test_num fehlgeschlagen. Details in: $err_file${NC}"
            echo -e "${RED}[FAIL] $test_num. $title${NC}"
            return 1
        fi
    }

    # --- TEST 0: ARCHITECTURE GATE ---
    unset REDIR_ENABLE_REDIR
    echo "+ GATING_TEST=FAIL" > tester_conf.env
    gate_out=$(./tester --quiet 2>&1)
    ! echo "$gate_out" | grep -q "GATING_TEST=FAIL" && success="true" || success="false"
    report+=("$(finalize_test 0 "Architektur-Gate (Pass-Through)" "$success" "N/A")")
    export REDIR_ENABLE_REDIR=1

    # --- TEST 1: BASIS-FUNKTIONEN ---
    echo -e "+ AAA_FIRST=1\n- HTTP_PROXY\n+ ZZZ_LAST=99" > tester_conf.env
    export HTTP_PROXY="weg"
    echo "Daten aus der Pipe" > test_in.txt
    output=$(./tester Arg1 "Arg 2" < test_in.txt 2>&1)
    echo "$output" | grep -q "Daten aus der Pipe" && echo "$output" | grep -q "Arg 2" && ! echo "$output" | grep -q "HTTP_PROXY=weg" && success="true" || success="false"
    report+=("$(finalize_test 1 "Basis-Funktionalitaet" "$success" "N/A")")

    # --- TEST 2: SORTIERUNG ---
    echo "$output" | grep -E "AAA_FIRST.*ZZZ_LAST" -z >/dev/null && success="true" || success="false"
    report+=("$(finalize_test 2 "Alphabetische Sortierung im Speicher" "$success" "N/A")")

    # --- TEST 3: SYNTAX-FEHLER ---
    errF="test_err_3.txt"
    echo -e "+ INVALID_LINE\n* WRONG_PREFIX" > tester_conf.env
    ./tester --quiet >/dev/null 2> "$errF"
    grep -q "ERROR" "$errF" && grep -q "Praefix" "$errF" && success="true" || success="false"
    report+=("$(finalize_test 3 "Syntax-Fehler Erkennung" "$success" "$errF")")

    # --- TEST 4: NO-CONFIG ---
    errF="test_err_4.txt"
    rm -f tester_conf.env
    ./tester --quiet >/dev/null 2> "$errF"
    grep -q "WARN" "$errF" && grep -q "Konfigurationsdatei gefunden" "$errF" && success="true" || success="false"
    report+=("$(finalize_test 4 "No-Config Warnung" "$success" "$errF")")

    # --- TEST 5: HIDDEN ENVS ---
    report+=("${GREEN}[OK] 5. Hidden Envs (N/A on Linux)${NC}")

    # --- TEST 6: EXIT-CODES (FAILURE) ---
    mv tester_org tester_org.bak
    ./tester --quiet >/dev/null 2>/dev/null
    [ $? -eq 102 ] && success="true" || success="false"
    mv tester_org.bak tester_org
    report+=("$(finalize_test 6 "Exit-Code 102 (Missing Target)" "$success" "N/A")")

    # --- TEST 7: EXIT-CODES (SUCCESS) ---
    ./tester --quiet >/dev/null 2>/dev/null
    [ $? -eq 42 ] && success="true" || success="false"
    report+=("$(finalize_test 7 "Exit-Code 42 (Success Propagation)" "$success" "N/A")")

    # --- TEST 8: WHITESPACE ROBUSTNESS ---
    echo -e "\n  + WS_VAR = SUCCESS  \n" > tester_conf.env
    ./tester --quiet | grep -q "WS_VAR=SUCCESS" && success="true" || success="false"
    report+=("$(finalize_test 8 "Whitespace Robustheit" "$success" "N/A")")

    # --- TEST 9: UTF-8 BOM ---
    printf "\xEF\xBB\xBF+ BOM_VAR=TRUE\n" > tester_conf.env
    ./tester --quiet | grep -q "BOM_VAR=TRUE" && success="true" || success="false"
    report+=("$(finalize_test 9 "UTF-8 BOM Erkennung" "$success" "N/A")")

    # --- TEST 10: DUMP ---
    export REDIR_DUMP_ENV=1
    ./tester --quiet >/dev/null
    [ -f tester_*_env.txt ] && success="true" || success="false"
    rm -f tester_*_env.txt
    unset REDIR_DUMP_ENV
    report+=("$(finalize_test 10 "Diagnose-Dump" "$success" "N/A")")

    # --- TEST 11: FILTER SYNTAX ---
    echo "- REMOVE_ME=VAL" > tester_conf.env
    export REMOVE_ME=STAY
    ! ./tester --quiet | grep -q "REMOVE_ME" && success="true" || success="false"
    report+=("$(finalize_test 11 "Erweiterte Filter-Syntax" "$success" "N/A")")

    # --- TEST 12: EMPTY VALUES ---
    echo "+ EMPTY=" > tester_conf.env
    ./tester --quiet | grep -q "EMPTY=" && success="true" || success="false"
    report+=("$(finalize_test 12 "Leere Values" "$success" "N/A")")

    # --- TEST 13: EMPTY KEY PROTECTION ---
    errF="test_err_13.txt"
    echo -e "+ =INVALID" > tester_conf.env
    ./tester --quiet >/dev/null 2> "$errF"
    grep -q "ERROR" "$errF" && success="true" || success="false"
    report+=("$(finalize_test 13 "Leere Keys werden abgelehnt" "$success" "$errF")")

    # --- TEST 14: UMLAUTE ---
    echo "+ Ö_VAR=Ä_VAL" > tester_conf.env
    ./tester --quiet | grep -q "Ö_VAR=Ä_VAL" && success="true" || success="false"
    report+=("$(finalize_test 14 "Umlaut-Support" "$success" "N/A")")

    echo -e "\n  Zusammenfassung für $mode_name:"
    for line in "${report[@]}"; do echo -e "  $line"; done
    [[ "${report[*]}" == *"[FAIL]"* ]] && return 1 || return 0
}

echo -e "${CYAN}--- Starte ultimative Dual-Mode Validierung (Senior-Proof) ---${NC}"
f_failed=false
Run-Validation-Pass "Standard-Modus (UTF-8)" "false" || f_failed=true
Run-Validation-Pass "Unicode-Emulation" "true" || f_failed=true

echo -e "\n${CYAN}=== GESAMT-ZUSAMMENFASSUNG DER VALIDIERUNG ===${NC}"
$f_failed && { echo -e "${RED}VALIDIERUNG FEHLGESCHLAGEN!${NC}"; exit 1; } || { echo -e "${GREEN}PROJEKT-STATUS: EXZELLENT - BEIDE MODI GRUEN${NC}"; exit 0; }
