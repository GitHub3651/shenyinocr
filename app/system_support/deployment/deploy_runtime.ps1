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

if (-not (Test-Path -LiteralPath $windeployQtPath -PathType Leaf)) {
    throw "Qt deployment tool does not exist: $windeployQtPath"
}

if ($sourcePath.Equals($destinationPath, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to deploy runtime files onto the source package.'
}

New-Item -ItemType Directory -Force -Path $destinationPath | Out-Null

# Keep the executable produced by the current Qt Creator link. Copy only the
# version-independent runtime assets from dist; Qt libraries and plugins come
# from the active Qt kit below.
& robocopy $sourcePath $destinationPath /E /XO /FFT /R:1 /W:1 /NFL /NDL /NJH /NJS /XF ShengYin.exe manifest.sha256 README.txt 'Qt5*.dll' libEGL.dll libGLESv2.dll opengl32sw.dll D3Dcompiler_47.dll /XD myImage log platforms imageformats iconengines styles translations
$robocopyExitCode = $LASTEXITCODE
if ($robocopyExitCode -ge 8) {
    throw "Runtime deployment failed (robocopy exit code $robocopyExitCode)."
}

$executablePath = Join-Path $destinationPath 'ShengYin.exe'
if (-not (Test-Path -LiteralPath $executablePath -PathType Leaf)) {
    throw "Linked executable does not exist: $executablePath"
}

$originalPath = $env:PATH
try {
    $env:PATH = "$qtBinPath;$originalPath"
    & $windeployQtPath --release --force --no-translations --dir $destinationPath $executablePath
    $winDeployQtExitCode = $LASTEXITCODE
}
finally {
    $env:PATH = $originalPath
}

if ($winDeployQtExitCode -ne 0) {
    throw "Qt runtime deployment failed (windeployqt exit code $winDeployQtExitCode)."
}

$requiredFiles = @(
    'config1.txt',
    'en_dict.txt',
    'license.ini',
    'BarcodeDecoder.dll',
    'paddle_inference.dll',
    'mklml.dll',
    'mkldnn.dll',
    'libiomp5md.dll',
    'opencv_core341.dll',
    'opencv_imgproc341.dll',
    'opencv_highgui341.dll',
    'opencv_imgcodecs341.dll',
    'opencv_videoio341.dll',
    'opencv_world341.dll',
    'snap7.dll',
    'MvCameraControl.dll',
    'Qt5Core.dll',
    'Qt5Gui.dll',
    'Qt5Widgets.dll',
    'platforms\\qwindows.dll',
    'Model\\en_PP-OCRv3_det_infer\\inference.pdmodel',
    'Model\\en_PP-OCRv3_det_infer\\inference.pdiparams',
    'Model\\en_PP-OCRv3_rec_infer\\inference.pdmodel',
    'Model\\en_PP-OCRv3_rec_infer\\inference.pdiparams',
    'Model\\ch_ppocr_mobile_v2.0_cls_infer\\inference.pdmodel',
    'Model\\ch_ppocr_mobile_v2.0_cls_infer\\inference.pdiparams'
)

$missing = @($requiredFiles | Where-Object { -not (Test-Path -LiteralPath (Join-Path $destinationPath $_)) })
if ($missing.Count -gt 0) {
    throw "Runtime deployment is incomplete: $($missing -join ', ')"
}

$config = Get-Content -LiteralPath (Join-Path $destinationPath 'config1.txt') -Raw
if ($config -notmatch 'en_PP-OCRv3_det_infer' -or $config -match 'PP-OCRv5') {
    throw 'Runtime deployment did not produce the selected V3 OCR configuration.'
}

Write-Host "Qt and V3 runtime assets are ready in $destinationPath"
