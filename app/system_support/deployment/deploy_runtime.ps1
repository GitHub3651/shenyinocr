[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Source,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Destination,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$QtBinDirectory
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
    throw "Runtime source directory does not exist: $Source"
}

if (-not (Test-Path -LiteralPath $QtBinDirectory -PathType Container)) {
    throw "Qt bin directory does not exist: $QtBinDirectory"
}

$sourcePath = (Resolve-Path -LiteralPath $Source).Path.TrimEnd('\\')
$destinationPath = [System.IO.Path]::GetFullPath($Destination).TrimEnd('\\')
$qtBinPath = (Resolve-Path -LiteralPath $QtBinDirectory).Path.TrimEnd('\\')
$windeployQtPath = Join-Path $qtBinPath 'windeployqt.exe'
$qtChineseTranslationPath = Join-Path (
    Split-Path -Parent $qtBinPath) 'translations\qt_zh_CN.qm'

if (-not (Test-Path -LiteralPath $windeployQtPath -PathType Leaf)) {
    throw "Qt deployment tool does not exist: $windeployQtPath"
}
if (-not (Test-Path -LiteralPath $qtChineseTranslationPath -PathType Leaf)) {
    throw "Qt Chinese translation does not exist: $qtChineseTranslationPath"
}

if ($sourcePath.Equals($destinationPath, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to deploy runtime files onto the source package.'
}

New-Item -ItemType Directory -Force -Path $destinationPath | Out-Null

$obsoleteOcrPaths = @(
    'Model',
    'OCR',
    'config1.txt',
    'config_ocr.txt',
    'en_dict.txt',
    'paddle_inference.dll',
    'common.dll',
    'mklml.dll',
    'mkldnn.dll',
    'libiomp5md.dll'
)
foreach ($relativePath in $obsoleteOcrPaths) {
    $obsoletePath = Join-Path $destinationPath $relativePath
    if (Test-Path -LiteralPath $obsoletePath) {
        Remove-Item -LiteralPath $obsoletePath -Recurse -Force
    }
}

# Keep the executable produced by the current Qt Creator link. Copy only the
# version-independent runtime assets from dist; Qt libraries and plugins come
# from the active Qt kit below.
& robocopy $sourcePath $destinationPath /E /XO /FFT /R:1 /W:1 /NFL /NDL /NJH /NJS /XF ShengYin.exe '晟崟AI视觉检测软件.exe' README.txt 'Qt5*.dll' libEGL.dll libGLESv2.dll opengl32sw.dll D3Dcompiler_47.dll /XD myImage log platforms imageformats iconengines styles translations
$robocopyExitCode = $LASTEXITCODE
if ($robocopyExitCode -ge 8) {
    throw "Runtime deployment failed (robocopy exit code $robocopyExitCode)."
}

$linkedExecutablePath = Join-Path $destinationPath 'ShengYin.exe'
if (-not (Test-Path -LiteralPath $linkedExecutablePath -PathType Leaf)) {
    throw "Linked executable does not exist: $linkedExecutablePath"
}

$originalPath = $env:PATH
try {
    $env:PATH = "$qtBinPath;$originalPath"
    & $windeployQtPath --release --force --no-translations --dir $destinationPath $linkedExecutablePath
    $winDeployQtExitCode = $LASTEXITCODE
}
finally {
    $env:PATH = $originalPath
}

if ($winDeployQtExitCode -ne 0) {
    throw "Qt runtime deployment failed (windeployqt exit code $winDeployQtExitCode)."
}

$translationDestination = Join-Path $destinationPath 'translations'
New-Item -ItemType Directory -Force -Path $translationDestination | Out-Null
Copy-Item -LiteralPath $qtChineseTranslationPath `
    -Destination (Join-Path $translationDestination 'qt_zh_CN.qm') -Force

$requiredFiles = @(
    'config_ocr.txt',
    'license.ini',
    'BarcodeDecoder.dll',
    'paddle_inference.dll',
    'common.dll',
    'mklml.dll',
    'mkldnn.dll',
    'libiomp5md.dll',
    'opencv_world4140.dll',
    'snap7.dll',
    'MvCameraControl.dll',
    'Qt5Core.dll',
    'Qt5Gui.dll',
    'Qt5Widgets.dll',
    'platforms\\qwindows.dll',
    'translations\\qt_zh_CN.qm',
    'OCR\\PP-OCRv6_tiny\\det\\inference.json',
    'OCR\\PP-OCRv6_tiny\\det\\inference.pdiparams',
    'OCR\\PP-OCRv6_tiny\\rec\\inference.json',
    'OCR\\PP-OCRv6_tiny\\rec\\inference.pdiparams',
    'OCR\\PP-OCRv6_tiny\\ppocrv6_tiny_dict.txt'
)

$missing = @($requiredFiles | Where-Object { -not (Test-Path -LiteralPath (Join-Path $destinationPath $_)) })
if ($missing.Count -gt 0) {
    throw "Runtime deployment is incomplete: $($missing -join ', ')"
}

$config = Get-Content -LiteralPath (Join-Path $destinationPath 'config_ocr.txt') -Raw
if ($config -notmatch 'OCR/PP-OCRv6_tiny/det/' `
        -or $config -notmatch 'OCR/PP-OCRv6_tiny/rec/' `
        -or $config -notmatch 'ppocrv6_tiny_dict.txt') {
    throw 'Runtime deployment did not produce the PP-OCRv6 tiny configuration.'
}

$releaseExecutablePath = Join-Path $destinationPath '晟崟AI视觉检测软件.exe'
Copy-Item -LiteralPath $linkedExecutablePath -Destination $releaseExecutablePath -Force

Write-Host "Qt and PP-OCRv6 tiny runtime assets are ready in $destinationPath"
