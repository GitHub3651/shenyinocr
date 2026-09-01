[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$QtBinDirectory,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Executable
)

$ErrorActionPreference = 'Stop'

$qtBinPath = (Resolve-Path -LiteralPath $QtBinDirectory).Path.TrimEnd('\\')
$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$destinationPath = Split-Path -Parent $executablePath
$winDeployQtPath = Join-Path $qtBinPath 'windeployqt.exe'

if (-not (Test-Path -LiteralPath $winDeployQtPath -PathType Leaf)) {
    throw "Qt deployment tool does not exist: $winDeployQtPath"
}

$originalPath = $env:PATH
try {
    $env:PATH = "$qtBinPath;$originalPath"
    & $winDeployQtPath --release --force --no-translations `
        --dir $destinationPath $executablePath
    $winDeployQtExitCode = $LASTEXITCODE
}
finally {
    $env:PATH = $originalPath
}

if ($winDeployQtExitCode -ne 0) {
    throw "Qt runtime deployment failed (windeployqt exit code $winDeployQtExitCode)."
}

$requiredFiles = @(
    'Qt5Core.dll',
    'Qt5Gui.dll',
    'Qt5Widgets.dll',
    'Qt5Network.dll',
    'platforms\\qwindows.dll'
)
$missing = @($requiredFiles | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $destinationPath $_) -PathType Leaf)
})
if ($missing.Count -gt 0) {
    throw "ResultReceiver deployment is incomplete: $($missing -join ', ')"
}

Write-Host "ResultReceiver runtime is ready in $destinationPath"
