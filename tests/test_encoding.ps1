$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
$exe = 'C:\Users\YuriVidal\Proyectos\PasteAsKeystrokes\PasteAsKeystrokes.exe'

Get-Process PasteAsKeystrokes,notepad -EA SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
$app = Start-Process $exe -PassThru
Start-Sleep -Milliseconds 1200

$wsh = New-Object -ComObject WScript.Shell
$fails = 0

function Test-Text($name, $text) {
    Start-Process notepad | Out-Null
    $np = $null
    for ($i=0; $i -lt 30; $i++) {
        $np = Get-Process notepad -EA SilentlyContinue | Where-Object { $_.MainWindowHandle -ne [IntPtr]::Zero } | Select-Object -First 1
        if ($np) { break }
        Start-Sleep -Milliseconds 200
    }
    $script:wsh.AppActivate($np.MainWindowTitle) | Out-Null
    Start-Sleep -Milliseconds 700

    Set-Clipboard -Value $text
    Start-Sleep -Milliseconds 200
    [System.Windows.Forms.SendKeys]::SendWait('^%v')
    $wait = [Math]::Max(2000, $text.Length * 40)
    Start-Sleep -Milliseconds $wait
    $script:wsh.AppActivate($np.MainWindowTitle) | Out-Null
    Start-Sleep -Milliseconds 300
    [System.Windows.Forms.SendKeys]::SendWait('^a'); Start-Sleep -Milliseconds 200
    [System.Windows.Forms.SendKeys]::SendWait('^c'); Start-Sleep -Milliseconds 400
    $got = Get-Clipboard -Raw
    if ($null -eq $got) { $got = '' }
    $got = $got.TrimEnd("`r","`n")

    Stop-Process -Id $np.Id -Force -EA SilentlyContinue
    Start-Sleep -Milliseconds 300

    if ($got -eq $text) {
        Write-Host ("[PASS] {0}" -f $name) -ForegroundColor Green
    } else {
        Write-Host ("[FAIL] {0}" -f $name) -ForegroundColor Red
        Write-Host ("    expected: {0}" -f $text)
        Write-Host ("    got:      {0}" -f $got)
        $script:fails++
    }
}

Test-Text "ASCII symbols (shift)" 'Hello! @#$%^&*()_+={}[]|:;"<>?,./~`'
Test-Text "mixed case + digits"   'AbCdEf 0123456789 XyZ'
Test-Text "latin accents"         ([char]0x00F1 + 'an' + [char]0x00E7 + 'on ' + [char]0x00E9 + [char]0x00FC + ' ' + [char]0x00BF + '?' + [char]0x00A1 + '!')
Test-Text "euro and symbols"      ('5' + [char]0x20AC + ' ' + [char]0x00A9 + [char]0x00AE + [char]0x2122)
Test-Text "CJK"                   ([char]0x6F22 + [char]0x5B57 + [char]0xD55C + [char]0xAE00)
Test-Text "emoji surrogate"       ([System.Char]::ConvertFromUtf32(0x1F511) + [System.Char]::ConvertFromUtf32(0x1F600))
Test-Text "real powershell cmd"   "Set-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\iaStorAC' -Name Start -Value 3"

Stop-Process -Id $app.Id -Force -EA SilentlyContinue
Write-Host ""
if ($fails -eq 0) { Write-Host "All encoding tests passed." -ForegroundColor Green; exit 0 }
else { Write-Host "$fails test(s) FAILED." -ForegroundColor Red; exit 1 }
