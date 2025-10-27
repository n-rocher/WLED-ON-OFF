param(
    [string]$InstallDir
)

# Ensure trailing backslash
if (-not $InstallDir.EndsWith("\")) {
    $InstallDir += "\"
}

$Target = "$InstallDir\WLED-ON-OFF_Helper.exe"
$Icon = "$InstallDir\WLEDSettings.exe"
$Shortcut = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\WLED-ON-OFF Helper.lnk"

$WScriptShell = New-Object -ComObject WScript.Shell
$ShortcutObj = $WScriptShell.CreateShortcut($Shortcut)
$ShortcutObj.TargetPath = $Target
$ShortcutObj.WorkingDirectory = $InstallDir
$ShortcutObj.Description = "WLED-ON-OFF Helper"
$ShortcutObj.IconLocation = "$Icon,0"
$ShortcutObj.Save()

Write-Host "Shortcut created at $Shortcut"