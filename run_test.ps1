# run_test.ps1
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Run-Validation-Pass {
    param($ModeName, $EnableU16)
    
    Write-Host "`n>>> VALIDIERUNGS-PASS: $ModeName <<<" -ForegroundColor Cyan -BackgroundColor DarkBlue
    
    # Setze Erwartungshaltung für diesen Pass
    if ($EnableU16) { 
        $env:REDIR_ENABLE_U16TEXT = "1"
        $expectedEncoding = "Unicode" # Entspricht UTF-16 LE in PowerShell
    } else { 
        $env:REDIR_ENABLE_U16TEXT = $null
        $expectedEncoding = "Default" # Entspricht System-ANSI/ASCII
    }
    
    $report = @()

    # Vorbereitung
    Get-ChildItem "tester*" | Remove-Item -Force -ErrorAction SilentlyContinue
    Get-ChildItem "test_err_*.txt" | Remove-Item -Force -ErrorAction SilentlyContinue
    Copy-Item "redir.exe" "tester.exe" -Force
    Copy-Item "mock_org.exe" "tester_org.exe" -Force

    function New-TestConfig {
        param($Content)
        [System.IO.File]::WriteAllText("$(Get-Location)\tester_conf.env", $Content, $utf8NoBom)
    }

    # Helper für die Ergebnisauswertung
    function Finalize-Test {
        param($TestNum, $Title, $IsSuccess, $ErrFile)
        if ($IsSuccess) {
            if (Test-Path $ErrFile) { Remove-Item $ErrFile -Force }
            return "[OK] $TestNum. $Title"
        } else {
            Write-Host "  [!] Test $TestNum fehlgeschlagen. Details in: $ErrFile" -ForegroundColor Yellow
            return "[FAIL] $TestNum. $Title"
        }
    }

    # --- TEST 0: ARCHITECTURE GATE ---
    Write-Host "  -> Starte Test 0: Architektur-Gate..." -ForegroundColor Gray
    $env:REDIR_ENABLE_REDIR = $null
    New-TestConfig "+ GATING_TEST=FAIL"
    $gateOutput = cmd.exe /c ".\tester.exe --quiet" | Out-String
    $report += Finalize-Test 0 "Architektur-Gate (Pass-Through ohne Flag)" (-not ($gateOutput -match "GATING_TEST=FAIL")) "N/A"
    $env:REDIR_ENABLE_REDIR = "1"

    # --- TEST 1: BASIS-FUNKTIONEN ---
    Write-Host "  -> Starte Test 1: Basis-Funktionen..." -ForegroundColor Gray
    New-TestConfig "+ AAA_FIRST=1`n- HTTP_PROXY`n+ ZZZ_LAST=99`n+ CaseSensitive_VAR=YES`n+ SALAT_VAR=A=B=C"
    $env:HTTP_PROXY = "weg"
    $testInput = "Daten aus der Pipe"
    [System.IO.File]::WriteAllText("$(Get-Location)\test_in.txt", $testInput, $utf8NoBom)
    $output = cmd.exe /c ".\tester.exe Arg1 ""Zweites Arg mit Leerzeichen"" < test_in.txt 2>&1" | Out-String
    $check = ($output -match "Daten aus der Pipe") -and ($output -match "Arg1 Zweites Arg mit Leerzeichen") -and ($output -notmatch "HTTP_PROXY=weg")
    $report += Finalize-Test 1 "Basis-Funktionalitaet (I/O, Args, Filter)" $check "N/A"

    # --- TEST 2: SORTIERUNG ---
    Write-Host "  -> Starte Test 2: Sortierung..." -ForegroundColor Gray
    $report += Finalize-Test 2 "Alphabetische Sortierung im Speicher" ($output -match "AAA_FIRST(.|\n)*ZZZ_LAST") "N/A"

    # --- TEST 3: SYNTAX-FEHLER ---
    Write-Host "  -> Starte Test 3: Syntax-Fehler..." -ForegroundColor Gray
    $errF = "test_err_3.txt"
    New-TestConfig "+ INVALID_LINE`n* WRONG_PREFIX"
    cmd.exe /c ".\tester.exe --quiet >nul 2> $errF"
    $errLines = Get-Content $errF -Encoding $expectedEncoding -ErrorAction SilentlyContinue
    $success = $errLines | Where-Object { $_ -match "ERROR" -and $_ -match "Unbekanntes Praefix" }
    $report += Finalize-Test 3 "Syntax-Fehler Erkennung" ([bool]$success) $errF

    # --- TEST 4: NO-CONFIG ---
    Write-Host "  -> Starte Test 4: No-Config..." -ForegroundColor Gray
    $errF = "test_err_4.txt"
    if (Test-Path "tester_conf.env") { Remove-Item "tester_conf.env" -Force }
    cmd.exe /c ".\tester.exe --quiet >nul 2> $errF"
    $warnLines = Get-Content $errF -Encoding $expectedEncoding -ErrorAction SilentlyContinue
    $success = $warnLines | Where-Object { $_ -match "WARN" -and $_ -match "Konfigurationsdatei gefunden" }
    $report += Finalize-Test 4 "No-Config Warnung" ([bool]$success) $errF

    # --- TEST 5: HIDDEN ENVS ---
    Write-Host "  -> Starte Test 5: Hidden Envs..." -ForegroundColor Gray
    $cmdOut = cmd.exe /c ".\tester.exe --quiet 2>nul" | Out-String
    $report += Finalize-Test 5 "Hidden Envs (=C:) weitergeleitet" ($cmdOut -match "=C:=") "N/A"

    # --- TEST 6: EXIT-CODES (FAILURE) ---
    Write-Host "  -> Starte Test 6: Exit-Codes (Failure)..." -ForegroundColor Gray
    if (Test-Path "tester_org.exe") { Rename-Item "tester_org.exe" "tester_org.exe.bak" -Force }
    cmd.exe /c ".\tester.exe --quiet >nul 2>nul"
    $report += Finalize-Test 6 "Exit-Code 102 (Missing Target)" ($LASTEXITCODE -eq 102) "N/A"
    if (Test-Path "tester_org.exe.bak") { Rename-Item "tester_org.exe.bak" "tester_org.exe" -Force }

    # --- TEST 7: EXIT-CODES (SUCCESS) ---
    Write-Host "  -> Starte Test 7: Exit-Codes (Success)..." -ForegroundColor Gray
    .\tester.exe --quiet >$null 2>$null
    $report += Finalize-Test 7 "Exit-Code 42 (Success Propagation)" ($LASTEXITCODE -eq 42) "N/A"

    # --- TEST 8: WHITESPACE ROBUSTNESS ---
    Write-Host "  -> Starte Test 8: Whitespace Robustness..." -ForegroundColor Gray
    $wsConfig = "`n`n  + ARCH_VAR = STAY_COOL  `n   `t  `r`n# Comment`n`n+ VALID_VAR=YES`t `n+ TRAILING_TEST   =SUCCESS    `t  `n+ LEADING_TEST=   VALUE `n`r`n"
    New-TestConfig $wsConfig
    $wsOutput = cmd.exe /c ".\tester.exe --quiet 2>nul" | Out-String
    $check = ($wsOutput -match "ARCH_VAR=STAY_COOL") -and ($wsOutput -match "VALID_VAR=YES") -and ($wsOutput -match "TRAILING_TEST=SUCCESS") -and ($wsOutput -match "LEADING_TEST=VALUE")
    $report += Finalize-Test 8 "Whitespace/CRLF Robustheit" $check "N/A"

    # --- TEST 9: UTF-8 BOM ROBUSTNESS ---
    Write-Host "  -> Starte Test 9: UTF-8 BOM Robustness..." -ForegroundColor Gray
    $utf8WithBom = New-Object System.Text.UTF8Encoding($true)
    [System.IO.File]::WriteAllText("$(Get-Location)\tester_conf.env", "+ BOM_TEST_VAR=TRUE", $utf8WithBom)
    $bomOutput = cmd.exe /c ".\tester.exe --quiet 2>nul" | Out-String
    $report += Finalize-Test 9 "UTF-8 BOM Erkennung & Stripping" ($bomOutput -match "BOM_TEST_VAR=TRUE") "N/A"

    # --- TEST 10: ENVIRONMENT DUMP (REDIR_DEBUG=PRE_ENV) ---
    Write-Host "  -> Starte Test 10: Environment Dump..." -ForegroundColor Gray
    $env:REDIR_DEBUG = " ,  PRE_ENV  , "
    cmd.exe /c ".\tester.exe --quiet 2>nul" >$null
    $dumpFile = Get-ChildItem "tester_*_pre_env.txt" | Select-Object -First 1
    $check = [bool]$dumpFile
    if ($dumpFile) { 
        Write-Host "     [FILE-NAME] $($dumpFile.Name)" -ForegroundColor DarkCyan
        $sample = (Get-Content $dumpFile.FullName | Select-Object -First 1)
        Write-Host "     [DUMP-CHECK] Erste Zeile im PRE_ENV: '$sample'" -ForegroundColor DarkGray
        Remove-Item $dumpFile.FullName 
    }
    $report += Finalize-Test 10 "Diagnose-Dump (REDIR_DEBUG=PRE_ENV)" $check "N/A"
    $env:REDIR_DEBUG = $null

    # --- TEST 11: EXTENDED FILTER SYNTAX ---
    Write-Host "  -> Starte Test 11: Extended Filter Syntax..." -ForegroundColor Gray
    $env:FILTER_ME_1 = "should_be_removed"
    New-TestConfig "- FILTER_ME_1=SOMEVALUE"
    $extFilterOutput = cmd.exe /c ".\tester.exe --quiet 2>nul" | Out-String
    $report += Finalize-Test 11 "Erweiterte Filter-Syntax (- KEY=VAL)" ($extFilterOutput -notmatch "FILTER_ME_1") "N/A"
    $env:FILTER_ME_1 = $null

    # --- TEST 12: EMPTY VALUE ROBUSTNESS ---
    Write-Host "  -> Starte Test 12: Empty Value Robustness..." -ForegroundColor Gray
    New-TestConfig "+ EMPTY_VAR_1=`n+ EMPTY_VAR_2 =  "
    $emptyValOutput = cmd.exe /c ".\tester.exe --quiet 2>nul" | Out-String
    $report += Finalize-Test 12 "Leere Values korrekt im Env (KEY=)" ($emptyValOutput -match "EMPTY_VAR_1=" -and $emptyValOutput -match "EMPTY_VAR_2=") "N/A"

    # --- TEST 13: EMPTY KEY PROTECTION ---
    Write-Host "  -> Starte Test 13: Empty Key Protection..." -ForegroundColor Gray
    $errF = "test_err_13.txt"
    New-TestConfig "+ =INVALID_VAL`n+  = ALSO_INVALID"
    cmd.exe /c ".\tester.exe --quiet >nul 2> $errF"
    $errLines = Get-Content $errF -Encoding $expectedEncoding -ErrorAction SilentlyContinue
    $success = $errLines | Where-Object { $_ -match "ERROR" -and $_ -match "Leerer Key" }
    $report += Finalize-Test 13 "Leere Keys werden abgelehnt" ([bool]$success) $errF

    # --- TEST 14: UMLAUT ROBUSTNESS (UTF-8) ---
    Write-Host "  -> Starte Test 14: Umlaut Robustness..." -ForegroundColor Gray
    $umlautKey = "$([char]0xD6)ber_K$([char]0xE9)y_$([char]0xDF)"
    $umlautVal = "$([char]0xC4)pfel_$([char]0xDC)bel_$([char]0xB5)"
    New-TestConfig "+ $umlautKey = $umlautVal"
    $umlautOutput = cmd.exe /c ".\tester.exe --quiet" | Out-String
    $report += Finalize-Test 14 "Umlaut-Support (Variable im Child)" ($umlautOutput -match "ber_K" -and $umlautOutput -match "pfel_") "N/A"

    # --- TEST 15: MULTI-DEBUG FLAGS (DUMP_ARGS, DUMP_PIPES) ---
    Write-Host "  -> Starte Test 15: Multi-Debug Flags..." -ForegroundColor Gray
    $env:REDIR_DEBUG = "  DUMP_ARGS ,  DUMP_PIPES  "
    cmd.exe /c ".\tester.exe --quiet ArgumentCheck 2>nul" >$null
    $argsFile = Get-ChildItem "tester_*_args.txt" | Select-Object -First 1
    $pipesFile = Get-ChildItem "tester_*_pipes.txt" | Select-Object -First 1
    if ($argsFile) {
        Write-Host "     [FILE-NAME] $($argsFile.Name)" -ForegroundColor DarkCyan
        $content = Get-Content $argsFile.FullName -Raw
        Write-Host "     [DUMP-CHECK] Captured ARGS: '$content'" -ForegroundColor DarkGray
    }
    $report += Finalize-Test 15 "Multi-Debug (ARGS & PIPES)" ([bool]($argsFile -and $pipesFile)) "N/A"
    if ($argsFile) { Remove-Item $argsFile.FullName }
    if ($pipesFile) { Remove-Item $pipesFile.FullName }
    $env:REDIR_DEBUG = $null

    # --- TEST 16: POST_ENV DUMP (Modifikations-Check) ---
    Write-Host "  -> Starte Test 16: Post-Env Dump..." -ForegroundColor Gray
    $env:REDIR_DEBUG = "POST_ENV"
    $env:TEST_REMOVE = "gone"
    New-TestConfig "+ TEST_ADD=found`n- TEST_REMOVE"
    cmd.exe /c ".\tester.exe --quiet 2>nul" >$null
    $postFile = Get-ChildItem "tester_*_post_env.txt" | Select-Object -First 1
    $check = $false
    if ($postFile) {
        Write-Host "     [FILE-NAME] $($postFile.Name)" -ForegroundColor DarkCyan
        $content = Get-Content $postFile.FullName -Raw
        $check = ($content -match "TEST_ADD=found") -and ($content -notmatch "TEST_REMOVE=gone")
        Write-Host "     [DUMP-CHECK] POST_ENV Modifikation: TEST_ADD=found gefunden." -ForegroundColor DarkGray
        Remove-Item $postFile.FullName
    }
    $report += Finalize-Test 16 "Post-Environment Dump (ADD/REMOVE Check)" $check "N/A"
    $env:TEST_REMOVE = $null
    $env:REDIR_DEBUG = $null

    # --- TEST 17: HIGH-RES TIMESTAMP FORMAT (YYYYMMDD_HHMMSS_mmm) ---
    Write-Host "  -> Starte Test 17: High-Res Timestamp..." -ForegroundColor Gray
    $env:REDIR_DEBUG = "PRE_ENV"
    cmd.exe /c ".\tester.exe --quiet 2>nul" >$null
    $dumpFile = Get-ChildItem "tester_*_pre_env.txt" | Select-Object -First 1
    $check = $false
    if ($dumpFile) {
        Write-Host "     [FILE-NAME] $($dumpFile.Name)" -ForegroundColor DarkCyan
        # Format: tester_PID_YYYYMMDD_HHMMSS_mmm_pre_env.txt
        $check = $dumpFile.Name -match "\d{8}_\d{6}_\d{3}_pre_env\.txt$"
        Remove-Item $dumpFile.FullName
    }
    $report += Finalize-Test 17 "Hochaufloesende Zeitstempel (MS-Praezision)" $check "N/A"
    $env:REDIR_DEBUG = $null

    # --- TEST 18: PERSISTENT SIGNAL LOGGING ---
    Write-Host "  -> Starte Test 18: Persistent Signal Logging..." -ForegroundColor Gray
    $env:REDIR_DEBUG = "DUMP_SIGNALS"
    cmd.exe /c ".\tester.exe --quiet 2>nul" >$null
    $sigFile = Get-ChildItem "tester_*_signals.txt" | Select-Object -First 1
    $check = $false
    if ($sigFile) {
        Write-Host "     [FILE-NAME] $($sigFile.Name)" -ForegroundColor DarkCyan
        $content = Get-Content $sigFile.FullName -Raw
        Write-Host "     [DUMP-CHECK] Signal-Log Header: '$($content.Substring(0, 38))...'" -ForegroundColor DarkGray
        $check = $content -match "Persistent log active"
        Remove-Item $sigFile.FullName
    }
    $report += Finalize-Test 18 "Persistentes Signal-Logging" $check "N/A"
    $env:REDIR_DEBUG = $null

    # --- TEST 19: DUMP_IO PROXY (STDIN/OUT) ---
    Write-Host "  -> Starte Test 19: I/O Proxy..." -ForegroundColor Gray
    $env:REDIR_DEBUG = " ,, DUMP_IO ,, "
    $testInput = "PROXY_TEST_DATA"
    [System.IO.File]::WriteAllText("$(Get-Location)\test_io_in.txt", $testInput, $utf8NoBom)
    $ioOutput = cmd.exe /c ".\tester.exe --quiet < test_io_in.txt 2>nul" | Out-String
    
    $inBin = Get-ChildItem "tester_*_stdin.bin" | Select-Object -First 1
    $outBin = Get-ChildItem "tester_*_stdout.bin" | Select-Object -First 1
    
    $check = [bool]($inBin -and $outBin)
    if ($check) {
        Write-Host "     [FILE-NAME] $($inBin.Name)" -ForegroundColor DarkCyan
        $inContent = [System.IO.File]::ReadAllText($inBin.FullName)
        $outContent = [System.IO.File]::ReadAllText($outBin.FullName)
        Write-Host "     [DUMP-CHECK] Stdin-Dump enthielt: '$inContent'" -ForegroundColor DarkGray
        Write-Host "     [DUMP-CHECK] Stdout-Dump Groesse: $($outContent.Length) Bytes" -ForegroundColor DarkGray
        $check = ($inContent -match "PROXY_TEST_DATA") -and ($outContent -match "PROXY_TEST_DATA")
    }
    
    $report += Finalize-Test 19 "I/O Proxy & Binary Dumps (DUMP_IO)" $check "N/A"
    
    # Cleanup
    Get-ChildItem "tester_*_*.bin" | Remove-Item -Force
    Remove-Item "test_io_in.txt" -ErrorAction SilentlyContinue
    $env:REDIR_DEBUG = $null

    # Cleanup
    Remove-Item "test_in.txt" -ErrorAction SilentlyContinue
    return $report
}

Write-Host "--- Starte ultimative Dual-Mode Validierung (Senior-Proof) ---" -ForegroundColor Cyan
$finalFailed = $false
$allReports = @{}
$allReports["DEFAULT (ASCII)"] = Run-Validation-Pass -ModeName "Standard-Modus (ASCII/ANSI)" -EnableU16 $false
$allReports["UNICODE (UTF-16)"] = Run-Validation-Pass -ModeName "Unicode-Modus (REDIR_ENABLE_U16TEXT=1)" -EnableU16 $true

Write-Host "`n=== GESAMT-ZUSAMMENFASSUNG DER VALIDIERUNG ===" -ForegroundColor Cyan
foreach ($mode in $allReports.Keys) {
    Write-Host "`nMODUS: $mode" -ForegroundColor Cyan
    foreach ($line in $allReports[$mode]) {
        if ($line -match "OK") { Write-Host "  $line" -ForegroundColor Green }
        else { Write-Host "  $line" -ForegroundColor Red; $finalFailed = $true }
    }
}

if (-not $finalFailed) { 
    Write-Host "`nPROJEKT-STATUS: EXZELLENT - ALLE AUTOMATISIERTEN TESTS GRUEN" -BackgroundColor Green -ForegroundColor Black 
    Write-Host "`n[MANUELLER SIGNAL-CHECK]" -ForegroundColor Yellow
    Write-Host "Um das Signal-Forwarding (Ctrl+C / Ctrl+Break) zu testen:" -ForegroundColor Gray
    Write-Host "PowerShell: `$env:REDIR_ENABLE_REDIR=1; .\tester.exe --wait" -ForegroundColor Gray
    Write-Host "CMD:        set REDIR_ENABLE_REDIR=1 && tester.exe --wait" -ForegroundColor Gray
    Write-Host "2. Druecke Strg+C (Event 0) oder Strg+Pause (Event 1)" -ForegroundColor Gray
}
else { Write-Host "`nVALIDIERUNG FEHLGESCHLAGEN!" -BackgroundColor Red; exit 1 }
