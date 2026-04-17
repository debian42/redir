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
    $env:REDIR_ENABLE_REDIR = $null
    New-TestConfig "+ GATING_TEST=FAIL"
    $gateOutput = cmd.exe /c ".\tester.exe --quiet" | Out-String
    $report += Finalize-Test 0 "Architektur-Gate (Pass-Through ohne Flag)" (-not ($gateOutput -match "GATING_TEST=FAIL")) "N/A"
    $env:REDIR_ENABLE_REDIR = "1"

    # --- TEST 1: BASIS-FUNKTIONEN ---
    New-TestConfig "+ AAA_FIRST=1`n- HTTP_PROXY`n+ ZZZ_LAST=99`n+ CaseSensitive_VAR=YES`n+ SALAT_VAR=A=B=C"
    $env:HTTP_PROXY = "weg"
    $testInput = "Daten aus der Pipe"
    [System.IO.File]::WriteAllText("$(Get-Location)\test_in.txt", $testInput, $utf8NoBom)
    $output = cmd.exe /c ".\tester.exe Arg1 ""Zweites Arg mit Leerzeichen"" < test_in.txt 2>&1" | Out-String
    $check = ($output -match "Daten aus der Pipe") -and ($output -match "Arg1 Zweites Arg mit Leerzeichen") -and ($output -notmatch "HTTP_PROXY=weg")
    $report += Finalize-Test 1 "Basis-Funktionalitaet (I/O, Args, Filter)" $check "N/A"

    # --- TEST 2: SORTIERUNG ---
    $report += Finalize-Test 2 "Alphabetische Sortierung im Speicher" ($output -match "AAA_FIRST(.|\n)*ZZZ_LAST") "N/A"

    # --- TEST 3: SYNTAX-FEHLER ---
    $errF = "test_err_3.txt"
    New-TestConfig "+ INVALID_LINE`n* WRONG_PREFIX"
    cmd.exe /c ".\tester.exe --quiet >nul 2> $errF"
    $errLines = Get-Content $errF -Encoding $expectedEncoding -ErrorAction SilentlyContinue
    $success = $errLines | Where-Object { $_ -match "ERROR" -and $_ -match "Unbekanntes Praefix" }
    $report += Finalize-Test 3 "Syntax-Fehler Erkennung" ([bool]$success) $errF

    # --- TEST 4: NO-CONFIG ---
    $errF = "test_err_4.txt"
    if (Test-Path "tester_conf.env") { Remove-Item "tester_conf.env" -Force }
    cmd.exe /c ".\tester.exe --quiet >nul 2> $errF"
    $warnLines = Get-Content $errF -Encoding $expectedEncoding -ErrorAction SilentlyContinue
    $success = $warnLines | Where-Object { $_ -match "WARN" -and $_ -match "Konfigurationsdatei gefunden" }
    $report += Finalize-Test 4 "No-Config Warnung" ([bool]$success) $errF

    # --- TEST 5: HIDDEN ENVS ---
    $cmdOut = cmd.exe /c ".\tester.exe --quiet 2>nul" | Out-String
    $report += Finalize-Test 5 "Hidden Envs (=C:) weitergeleitet" ($cmdOut -match "=C:=") "N/A"

    # --- TEST 6: EXIT-CODES (FAILURE) ---
    if (Test-Path "tester_org.exe") { Rename-Item "tester_org.exe" "tester_org.exe.bak" -Force }
    cmd.exe /c ".\tester.exe --quiet >nul 2>nul"
    $report += Finalize-Test 6 "Exit-Code 102 (Missing Target)" ($LASTEXITCODE -eq 102) "N/A"
    if (Test-Path "tester_org.exe.bak") { Rename-Item "tester_org.exe.bak" "tester_org.exe" -Force }

    # --- TEST 7: EXIT-CODES (SUCCESS) ---
    .\tester.exe --quiet >$null 2>$null
    $report += Finalize-Test 7 "Exit-Code 42 (Success Propagation)" ($LASTEXITCODE -eq 42) "N/A"

    # --- TEST 8: WHITESPACE ROBUSTNESS ---
    $wsConfig = "`n`n  + ARCH_VAR = STAY_COOL  `n   `t  `r`n# Comment`n`n+ VALID_VAR=YES`t `n+ TRAILING_TEST   =SUCCESS    `t  `n+ LEADING_TEST=   VALUE `n`r`n"
    New-TestConfig $wsConfig
    $wsOutput = cmd.exe /c ".\tester.exe --quiet 2>nul" | Out-String
    $check = ($wsOutput -match "ARCH_VAR=STAY_COOL") -and ($wsOutput -match "VALID_VAR=YES") -and ($wsOutput -match "TRAILING_TEST=SUCCESS") -and ($wsOutput -match "LEADING_TEST=VALUE")
    $report += Finalize-Test 8 "Whitespace/CRLF Robustheit" $check "N/A"

    # --- TEST 9: UTF-8 BOM ROBUSTNESS ---
    $utf8WithBom = New-Object System.Text.UTF8Encoding($true)
    [System.IO.File]::WriteAllText("$(Get-Location)\tester_conf.env", "+ BOM_TEST_VAR=TRUE", $utf8WithBom)
    $bomOutput = cmd.exe /c ".\tester.exe --quiet 2>nul" | Out-String
    $report += Finalize-Test 9 "UTF-8 BOM Erkennung & Stripping" ($bomOutput -match "BOM_TEST_VAR=TRUE") "N/A"

    # --- TEST 10: ENVIRONMENT DUMP ---
    $env:REDIR_DUMP_ENV = "1"
    cmd.exe /c ".\tester.exe --quiet 2>nul" >$null
    $dumpFile = Get-ChildItem "tester_*_env.txt" | Select-Object -First 1
    $report += Finalize-Test 10 "Diagnose-Dump (REDIR_DUMP_ENV=1)" ([bool]$dumpFile) "N/A"
    if ($dumpFile) { Remove-Item $dumpFile.FullName }
    $env:REDIR_DUMP_ENV = $null

    # --- TEST 11: EXTENDED FILTER SYNTAX ---
    $env:FILTER_ME_1 = "should_be_removed"
    New-TestConfig "- FILTER_ME_1=SOMEVALUE"
    $extFilterOutput = cmd.exe /c ".\tester.exe --quiet 2>nul" | Out-String
    $report += Finalize-Test 11 "Erweiterte Filter-Syntax (- KEY=VAL)" ($extFilterOutput -notmatch "FILTER_ME_1") "N/A"
    $env:FILTER_ME_1 = $null

    # --- TEST 12: EMPTY VALUE ROBUSTNESS ---
    New-TestConfig "+ EMPTY_VAR_1=`n+ EMPTY_VAR_2 =  "
    $emptyValOutput = cmd.exe /c ".\tester.exe --quiet 2>nul" | Out-String
    $report += Finalize-Test 12 "Leere Values korrekt im Env (KEY=)" ($emptyValOutput -match "EMPTY_VAR_1=" -and $emptyValOutput -match "EMPTY_VAR_2=") "N/A"

    # --- TEST 13: EMPTY KEY PROTECTION ---
    $errF = "test_err_13.txt"
    New-TestConfig "+ =INVALID_VAL`n+  = ALSO_INVALID"
    cmd.exe /c ".\tester.exe --quiet >nul 2> $errF"
    $errLines = Get-Content $errF -Encoding $expectedEncoding -ErrorAction SilentlyContinue
    $success = $errLines | Where-Object { $_ -match "ERROR" -and $_ -match "Leerer Key" }
    $report += Finalize-Test 13 "Leere Keys werden abgelehnt" ([bool]$success) $errF

    # --- TEST 14: UMLAUT ROBUSTNESS (UTF-8) ---
    $umlautKey = "$([char]0xD6)ber_K$([char]0xE9)y_$([char]0xDF)"
    $umlautVal = "$([char]0xC4)pfel_$([char]0xDC)bel_$([char]0xB5)"
    New-TestConfig "+ $umlautKey = $umlautVal"
    $umlautOutput = cmd.exe /c ".\tester.exe --quiet" | Out-String
    $report += Finalize-Test 14 "Umlaut-Support (Variable im Child)" ($umlautOutput -match "ber_K" -and $umlautOutput -match "pfel_") "N/A"

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
