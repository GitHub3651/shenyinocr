[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$SourceDll,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$QtCoreDll,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$QtTestDll,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$TargetExecutable,

    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$Destination
)

$ErrorActionPreference = 'Stop'

function Get-PeMachine
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$ImagePath
    )

    if (-not (Test-Path -LiteralPath $ImagePath -PathType Leaf)) {
        throw "PE image does not exist: $ImagePath"
    }

    $resolvedPath = (Resolve-Path -LiteralPath $ImagePath).Path
    $stream = [System.IO.File]::OpenRead($resolvedPath)
    try {
        if ($stream.Length -lt 64) {
            throw "PE image is too small: $resolvedPath"
        }

        $reader = New-Object System.IO.BinaryReader($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) {
            throw "PE image has no MZ signature: $resolvedPath"
        }

        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0 -or $peOffset + 6 -gt $stream.Length) {
            throw "PE image has an invalid header offset: $resolvedPath"
        }

        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "PE image has no PE signature: $resolvedPath"
        }

        $machine = $reader.ReadUInt16()
        switch ($machine) {
            0x014C { return 'x86' }
            0x8664 { return 'x64' }
            0xAA64 { return 'ARM64' }
            default { return ('0x{0:X4}' -f $machine) }
        }
    }
    finally {
        $stream.Dispose()
    }
}

function Copy-VerifiedRuntimeDll
{
    param(
        [Parameter(Mandatory = $true)]
        [string]$RuntimeDll,

        [Parameter(Mandatory = $true)]
        [string]$DestinationDirectory,

        [Parameter(Mandatory = $true)]
        [string]$ExpectedMachine
    )

    if (-not (Test-Path -LiteralPath $RuntimeDll -PathType Leaf)) {
        throw "Test runtime DLL does not exist: $RuntimeDll"
    }

    $sourcePath = (Resolve-Path -LiteralPath $RuntimeDll).Path
    $runtimeMachine = Get-PeMachine -ImagePath $sourcePath
    if ($runtimeMachine -ne $ExpectedMachine) {
        throw "Test runtime architecture mismatch: target=$ExpectedMachine, runtime=$runtimeMachine, file=$sourcePath"
    }

    $destinationFile = Join-Path $DestinationDirectory ([System.IO.Path]::GetFileName($sourcePath))

    Copy-Item -LiteralPath $sourcePath -Destination $destinationFile -Force

    if (-not (Test-Path -LiteralPath $destinationFile -PathType Leaf)) {
        throw "Test runtime deployment did not create: $destinationFile"
    }

    $sourceHash = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
    $destinationHash = (Get-FileHash -LiteralPath $destinationFile -Algorithm SHA256).Hash
    if ($sourceHash -ne $destinationHash) {
        throw "Test runtime deployment hash mismatch: $destinationFile"
    }

    Write-Host "Test runtime DLL is ready: $destinationFile ($runtimeMachine)"
}

$destinationPath = [System.IO.Path]::GetFullPath($Destination)
New-Item -ItemType Directory -Force -Path $destinationPath | Out-Null
$targetMachine = Get-PeMachine -ImagePath $TargetExecutable

foreach ($runtimeDll in @($SourceDll, $QtCoreDll, $QtTestDll)) {
    Copy-VerifiedRuntimeDll -RuntimeDll $runtimeDll -DestinationDirectory $destinationPath -ExpectedMachine $targetMachine
}
