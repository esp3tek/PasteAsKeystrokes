$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
$exe = 'C:\Users\YuriVidal\Proyectos\PasteAsKeystrokes\PasteAsKeystrokes.exe'

Get-Process PasteAsKeystrokes,notepad -EA SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
$app = Start-Process $exe -PassThru
Start-Sleep -Milliseconds 1200

function Test-Pattern($name, $text, $expectedLines) {
    Start-Process notepad | Out-Null
    $np = $null
    for ($i=0; $i -lt 30; $i++) {
        $np = Get-Process notepad -EA SilentlyContinue | Where-Object { $_.MainWindowHandle -ne [IntPtr]::Zero } | Select-Object -First 1
        if ($np) { break }
        Start-Sleep -Milliseconds 200
    }
    $wsh = New-Object -ComObject WScript.Shell
    $wsh.AppActivate($np.MainWindowTitle) | Out-Null
    Start-Sleep -Milliseconds 700

    Set-Clipboard -Value $text
    Start-Sleep -Milliseconds 200

    [System.Windows.Forms.SendKeys]::SendWait('^%v')
    Start-Sleep -Seconds 2

    $wsh.AppActivate($np.MainWindowTitle) | Out-Null
    Start-Sleep -Milliseconds 300
    [System.Windows.Forms.SendKeys]::SendWait('^a'); Start-Sleep -Milliseconds 200
    [System.Windows.Forms.SendKeys]::SendWait('^c'); Start-Sleep -Milliseconds 400
    $got = Get-Clipboard -Raw
    if ($null -eq $got) { $got = '' }

    Stop-Process -Id $np.Id -Force -EA SilentlyContinue
    Start-Sleep -Milliseconds 300

    # count newline characters (normalize CRLF/CR to LF)
    $gotNorm = ($got -replace "`r`n","`n" -replace "`r","`n").TrimEnd("`n")
    $expNorm = ($text -replace "`r`n","`n" -replace "`r","`n").TrimEnd("`n")
    $gotNL = ([regex]::Matches($gotNorm, "`n")).Count
    $expNL = ([regex]::Matches($expNorm, "`n")).Count

    $status = if ($gotNL -eq $expNL) { "PASS" } else { "FAIL" }
    $color  = if ($gotNL -eq $expNL) { "Green" } else { "Red" }
    Write-Host ("[{0}] {1}: expected {2} newlines, got {3}" -f $status, $name, $expNL, $gotNL) -ForegroundColor $color
    if ($gotNL -ne $expNL) {
        Write-Host ("    sent (escaped):  {0}" -f ($expNorm -replace "`n","\n"))
        Write-Host ("    got  (escaped):  {0}" -f ($gotNorm -replace "`n","\n"))
    }
}

# Patterns. "4 newlines" = 4 line breaks between content
Test-Pattern "2 consecutive blank lines"  "A`r`n`r`n`r`nB"          3
Test-Pattern "4 line breaks (5 lines)"    "L1`r`nL2`r`nL3`r`nL4`r`nL5"  4
Test-Pattern "4 consecutive newlines"     "A`r`n`r`n`r`n`r`n`r`nB"  5
Test-Pattern "trailing newlines x4"       "X`r`n`r`n`r`n`r`n"       4
Test-Pattern "LF only 4 breaks"           "a`nb`nc`nd`ne"          4
Test-Pattern "blank-heavy"                "1`n`n2`n`n3`n`n4"        6

Stop-Process -Id $app.Id -Force -EA SilentlyContinue
Write-Host "`nDone."
