[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Source,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Destination
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
    throw "Runtime source directory does not exist: $Source"
}

$sourcePath = (Resolve-Path -LiteralPath $Source).Path.TrimEnd('\\')
$destinationPath = [System.IO.Path]::GetFullPath($Destination).TrimEnd('\\')

if ($sourcePath.Equals($destinationPath, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to deploy runtime files onto the source package.'
}

New-Item -ItemType Directory -Force -Path $destinationPath | Out-Null

# Keep the executable produced by the current Qt Creator link. Runtime logs and
# generated images stay local to each run and must not be copied from dist.
& robocopy $sourcePath $destinationPath /E /XO /FFT /R:1 /W:1 /NFL /NDL /NJH /NJS /XF ShengYin.exe manifest.sha256 README.txt /XD myImage log
$robocopyExitCode = $LASTEXITCODE
if ($robocopyExitCode -ge 8) {
    throw "Runtime deployment failed (robocopy exit code $robocopyExitCode)."
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

Write-Host "V3 runtime assets are ready in $destinationPath"
