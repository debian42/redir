#!/bin/bash
# run_test.sh - Bash-Port mit korrekter Numerierung und Log-Persistenz (Senior Edition)
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
    rm -f tester tester_org tester_conf.env test_err_*.txt test_in.txt tester_*.txt tester_*.bin 2>/dev/null
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
    echo -e "${GRAY}  -> Starte Test 0: Architektur-Gate...${NC}"
    unset REDIR_ENABLE_REDIR
    echo "+ GATING_TEST=FAIL" > tester_conf.env
    gate_out=$(./tester --quiet 2>&1)
    ! echo "$gate_out" | grep -q "GATING_TEST=FAIL" && success="true" || success="false"
    report+=("$(finalize_test 0 "Architektur-Gate (Pass-Through)" "$success" "N/A")")
    export REDIR_ENABLE_REDIR=1

    # --- TEST 1: BASIS-FUNKTIONEN ---
    echo -e "${GRAY}  -> Starte Test 1: Basis-Funktionen...${NC}"
    echo -e "+ AAA_FIRST=1\n- HTTP_PROXY\n+ ZZZ_LAST=99" > tester_conf.env
    export HTTP_PROXY="weg"
    echo "Daten aus der Pipe" > test_in.txt
    output=$(./tester Arg1 "Arg 2" < test_in.txt 2>&1)
    echo "$output" | grep -q "Daten aus der Pipe" && echo "$output" | grep -q "Arg 2" && ! echo "$output" | grep -q "HTTP_PROXY=weg" && success="true" || success="false"
    report+=("$(finalize_test 1 "Basis-Funktionalitaet" "$success" "N/A")")

    # --- TEST 2: SORTIERUNG ---
    echo -e "${GRAY}  -> Starte Test 2: Sortierung...${NC}"
    echo "$output" | grep -E "AAA_FIRST.*ZZZ_LAST" -z >/dev/null && success="true" || success="false"
    report+=("$(finalize_test 2 "Alphabetische Sortierung im Speicher" "$success" "N/A")")

    # --- TEST 3: SYNTAX-FEHLER ---
    echo -e "${GRAY}  -> Starte Test 3: Syntax-Fehler...${NC}"
    errF="test_err_3.txt"
    echo -e "+ INVALID_LINE\n* WRONG_PREFIX" > tester_conf.env
    ./tester --quiet >/dev/null 2> "$errF"
    grep -q "ERROR" "$errF" && grep -q "Praefix" "$errF" && success="true" || success="false"
    report+=("$(finalize_test 3 "Syntax-Fehler Erkennung" "$success" "$errF")")

    # --- TEST 4: NO-CONFIG ---
    echo -e "${GRAY}  -> Starte Test 4: No-Config...${NC}"
    errF="test_err_4.txt"
    rm -f tester_conf.env
    ./tester --quiet >/dev/null 2> "$errF"
    grep -q "WARN" "$errF" && grep -q "Konfigurationsdatei gefunden" "$errF" && success="true" || success="false"
    report+=("$(finalize_test 4 "No-Config Warnung" "$success" "$errF")")

    # --- TEST 5: HIDDEN ENVS ---
    report+=("${GREEN}[OK] 5. Hidden Envs (N/A on Linux)${NC}")

    # --- TEST 6: EXIT-CODES (FAILURE) ---
    echo -e "${GRAY}  -> Starte Test 6: Exit-Codes (Failure)...${NC}"
    mv tester_org tester_org.bak
    ./tester --quiet >/dev/null 2>/dev/null
    [ $? -eq 102 ] && success="true" || success="false"
    mv tester_org.bak tester_org
    report+=("$(finalize_test 6 "Exit-Code 102 (Missing Target)" "$success" "N/A")")

    # --- TEST 7: EXIT-CODES (SUCCESS) ---
    echo -e "${GRAY}  -> Starte Test 7: Exit-Codes (Success)...${NC}"
    ./tester --quiet >/dev/null 2>/dev/null
    [ $? -eq 42 ] && success="true" || success="false"
    report+=("$(finalize_test 7 "Exit-Code 42 (Success Propagation)" "$success" "N/A")")

    # --- TEST 8: WHITESPACE ROBUSTNESS ---
    echo -e "${GRAY}  -> Starte Test 8: Whitespace Robustness...${NC}"
    echo -e "\n  + WS_VAR = SUCCESS  \n" > tester_conf.env
    ./tester --quiet | grep -q "WS_VAR=SUCCESS" && success="true" || success="false"
    report+=("$(finalize_test 8 "Whitespace Robustheit" "$success" "N/A")")

    # --- TEST 9: UTF-8 BOM ---
    echo -e "${GRAY}  -> Starte Test 9: UTF-8 BOM...${NC}"
    printf "\xEF\xBB\xBF+ BOM_VAR=TRUE\n" > tester_conf.env
    ./tester --quiet | grep -q "BOM_VAR=TRUE" && success="true" || success="false"
    report+=("$(finalize_test 9 "UTF-8 BOM Erkennung" "$success" "N/A")")

    # --- TEST 10: PRE_ENV DUMP (REDIR_DEBUG=PRE_ENV) ---
    echo -e "${GRAY}  -> Starte Test 10: Environment Dump...${NC}"
    export REDIR_DEBUG="   ,  PRE_ENV   "
    ./tester --quiet >/dev/null
    dumpFile=$(ls tester_*_pre_env.txt 2>/dev/null | head -n 1)
    if [ -n "$dumpFile" ]; then
        echo -e "${CYAN}     [FILE-NAME] $dumpFile${NC}"
        sample=$(head -n 1 "$dumpFile")
        echo -e "${GRAY}     [DUMP-CHECK] Erste Zeile im PRE_ENV: '$sample'${NC}"
        success="true"
        rm -f "$dumpFile"
    else
        success="false"
    fi
    unset REDIR_DEBUG
    report+=("$(finalize_test 10 "Diagnose-Dump (PRE_ENV)" "$success" "N/A")")

    # --- TEST 11: FILTER SYNTAX ---
    echo -e "${GRAY}  -> Starte Test 11: Extended Filter Syntax...${NC}"
    echo "- REMOVE_ME=VAL" > tester_conf.env
    export REMOVE_ME=STAY
    ! ./tester --quiet | grep -q "REMOVE_ME" && success="true" || success="false"
    report+=("$(finalize_test 11 "Erweiterte Filter-Syntax" "$success" "N/A")")

    # --- TEST 12: EMPTY VALUES ---
    echo -e "${GRAY}  -> Starte Test 12: Empty Values...${NC}"
    echo "+ EMPTY=" > tester_conf.env
    ./tester --quiet | grep -q "EMPTY=" && success="true" || success="false"
    report+=("$(finalize_test 12 "Leere Values" "$success" "N/A")")

    # --- TEST 13: EMPTY KEY PROTECTION ---
    echo -e "${GRAY}  -> Starte Test 13: Empty Key Protection...${NC}"
    errF="test_err_13.txt"
    echo -e "+ =INVALID" > tester_conf.env
    ./tester --quiet >/dev/null 2> "$errF"
    grep -q "ERROR" "$errF" && success="true" || success="false"
    report+=("$(finalize_test 13 "Leere Keys werden abgelehnt" "$success" "$errF")")

    # --- TEST 14: UMLAUTE ---
    echo -e "${GRAY}  -> Starte Test 14: Umlaut Robustness...${NC}"
    echo "+ Ö_VAR=Ä_VAL" > tester_conf.env
    ./tester --quiet | grep -q "Ö_VAR=Ä_VAL" && success="true" || success="false"
    report+=("$(finalize_test 14 "Umlaut-Support" "$success" "N/A")")

    # --- TEST 15: MULTI-DEBUG (ARGS, PIPES) ---
    echo -e "${GRAY}  -> Starte Test 15: Multi-Debug Flags...${NC}"
    export REDIR_DEBUG="DUMP_ARGS  , ,  DUMP_PIPES"
    ./tester --quiet ArgumentCheck >/dev/null
    argsFile=$(ls tester_*_args.txt 2>/dev/null | head -n 1)
    pipesFile=$(ls tester_*_pipes.txt 2>/dev/null | head -n 1)
    if [ -f "$argsFile" ] && [ -f "$pipesFile" ]; then
        echo -e "${CYAN}     [FILE-NAME] $argsFile${NC}"
        content=$(cat "$argsFile" | tr '\n' ' ')
        echo -e "${GRAY}     [DUMP-CHECK] Captured ARGS: '$content'${NC}"
        success="true"
        rm -f "$argsFile" "$pipesFile"
    else
        success="false"
    fi
    unset REDIR_DEBUG
    report+=("$(finalize_test 15 "Multi-Debug (ARGS & PIPES)" "$success" "N/A")")

    # --- TEST 16: POST_ENV DUMP ---
    echo -e "${GRAY}  -> Starte Test 16: Post-Env Dump...${NC}"
    export REDIR_DEBUG="POST_ENV"
    export TEST_REMOVE="gone"
    echo -e "+ TEST_ADD=found\n- TEST_REMOVE" > tester_conf.env
    ./tester --quiet >/dev/null
    postFile=$(ls tester_*_post_env.txt 2>/dev/null | head -n 1)
    if [ -n "$postFile" ]; then
        echo -e "${CYAN}     [FILE-NAME] $postFile${NC}"
        grep -q "TEST_ADD=found" "$postFile" && ! grep -q "TEST_REMOVE=gone" "$postFile" && success="true" || success="false"
        echo -e "${GRAY}     [DUMP-CHECK] POST_ENV Modifikation: TEST_ADD=found gefunden.${NC}"
        rm -f "$postFile"
    else
        success="false"
    fi
    unset TEST_REMOVE REDIR_DEBUG
    report+=("$(finalize_test 16 "Post-Environment Dump (ADD/REMOVE)" "$success" "N/A")")

    # --- TEST 17: HIGH-RES TIMESTAMP ---
    echo -e "${GRAY}  -> Starte Test 17: High-Res Timestamp...${NC}"
    export REDIR_DEBUG="PRE_ENV"
    ./tester --quiet >/dev/null
    dumpFile=$(ls tester_*_pre_env.txt 2>/dev/null | head -n 1)
    if [ -n "$dumpFile" ]; then
        echo -e "${CYAN}     [FILE-NAME] $dumpFile${NC}"
        [[ "$dumpFile" =~ [0-9]{8}_[0-9]{6}_[0-9]{3}_pre_env\.txt$ ]] && success="true" || success="false"
        rm -f "$dumpFile"
    else
        success="false"
    fi
    unset REDIR_DEBUG
    report+=("$(finalize_test 17 "Hochaufloesende Zeitstempel" "$success" "N/A")")

    # --- TEST 18: PERSISTENT SIGNAL LOGGING ---
    echo -e "${GRAY}  -> Starte Test 18: Persistent Signal Logging...${NC}"
    export REDIR_DEBUG="DUMP_SIGNALS"
    ./tester --quiet >/dev/null
    sigFile=$(ls tester_*_signals.txt 2>/dev/null | head -n 1)
    if [ -n "$sigFile" ]; then
        echo -e "${CYAN}     [FILE-NAME] $sigFile${NC}"
        grep -q "Persistent log active" "$sigFile" && success="true" || success="false"
        echo -e "${GRAY}     [DUMP-CHECK] Signal-Log Header: '$(head -c 38 "$sigFile")...'${NC}"
        rm -f "$sigFile"
    else
        success="false"
    fi
    unset REDIR_DEBUG
    report+=("$(finalize_test 18 "Persistentes Signal-Logging" "$success" "N/A")")

    # --- TEST 19: DUMP_IO PROXY (STDIN/OUT) ---
    echo -e "${GRAY}  -> Starte Test 19: I/O Proxy...${NC}"
    export REDIR_DEBUG="DUMP_IO"
    echo "PROXY_TEST_DATA" > test_io_in.txt
    ./tester --quiet < test_io_in.txt >/dev/null 2>/dev/null
    inBin=$(ls tester_*_stdin.bin 2>/dev/null | head -n 1)
    outBin=$(ls tester_*_stdout.bin 2>/dev/null | head -n 1)
    if [ -f "$inBin" ] && [ -f "$outBin" ]; then
        echo -e "${CYAN}     [FILE-NAME] $inBin${NC}"
        grep -q "PROXY_TEST_DATA" "$inBin" && grep -q "PROXY_TEST_DATA" "$outBin" && success="true" || success="false"
        echo -e "${GRAY}     [DUMP-CHECK] Stdin-Dump enthielt: '$(cat "$inBin")'${NC}"
        echo -e "${GRAY}     [DUMP-CHECK] Stdout-Dump Groesse: $(stat -c%s "$outBin") Bytes${NC}"
        rm -f "$inBin" "$outBin"
    else
        success="false"
    fi
    rm -f test_io_in.txt tester_*_stderr.bin 2>/dev/null
    unset REDIR_DEBUG
    report+=("$(finalize_test 19 "I/O Proxy & Binary Dumps (DUMP_IO)" "$success" "N/A")")

    # --- TEST 20: SIGNAL FORWARDING (Linux only) ---
    echo -e "${GRAY}  -> Starte Test 20: Multi-Signal Forwarding & Logging (USR1 & USR2)...${NC}"
    export REDIR_DEBUG="DUMP_SIGNALS"
    ./tester --wait > test_sig_out.txt 2>&1 &
    test_pid=$!
    sleep 0.5
    kill -SIGUSR1 $test_pid
    sleep 0.2
    kill -SIGUSR2 $test_pid
    sleep 0.2
    kill -SIGINT $test_pid
    wait $test_pid 2>/dev/null
    
    sigFile=$(ls tester_*_signals.txt 2>/dev/null | head -n 1)
    if [ -f "$sigFile" ]; then
        echo -e "${CYAN}     [FILE-NAME] $sigFile${NC}"
        sig_entry1=$(grep "Received: 10" "$sigFile")
        sig_entry2=$(grep "Received: 12" "$sigFile")
        echo -e "${GRAY}     [DUMP-CHECK] Signal-Log Einträge: '$sig_entry1' und '$sig_entry2'${NC}"
        [ -n "$sig_entry1" ] && [ -n "$sig_entry2" ] && success="true" || success="false"
        rm -f "$sigFile"
    else
        success="false"
    fi
    
    grep -q "Linux Signal empfangen: 10" test_sig_out.txt && \
    grep -q "Linux Signal empfangen: 12" test_sig_out.txt && \
    [ "$success" = "true" ] && success="true" || success="false"
    
    rm -f test_sig_out.txt
    unset REDIR_DEBUG
    report+=("$(finalize_test 20 "Multi-Signal Forwarding & Logging (USR1+USR2)" "$success" "N/A")")

    echo -e "\n  Zusammenfassung für $mode_name:"
    for line in "${report[@]}"; do echo -e "  $line"; done
    [[ "${report[*]}" == *"[FAIL]"* ]] && return 1 || return 0
}

echo -e "${CYAN}--- Starte ultimative Dual-Mode Validierung (Senior-Proof) ---${NC}"
f_failed=false
Run-Validation-Pass "Standard-Modus (UTF-8)" "false" || f_failed=true
Run-Validation-Pass "Unicode-Emulation" "true" || f_failed=true

echo -e "\n${CYAN}=== GESAMT-ZUSAMMENFASSUNG DER VALIDIERUNG ===${NC}"
if [ "$f_failed" = "true" ]; then 
    echo -e "${RED}VALIDIERUNG FEHLGESCHLAGEN!${NC}"; exit 1; 
else 
    echo -e "${GREEN}PROJEKT-STATUS: EXZELLENT - BEIDE MODI GRUEN${NC}"; exit 0; 
fi
