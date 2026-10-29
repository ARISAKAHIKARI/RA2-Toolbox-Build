# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [string]$InstallDirectory = $PSScriptRoot,
    [string]$SevenZipPath,
    [string]$ArchivePath
)

$ErrorActionPreference = 'Stop'
$archiveUrl = 'https://d1oqksizpb0e3d.cloudfront.net/rtx-remix-ngx_sdk_dlnr%402.7z'
$archiveBytes = 367851723L
$archiveHash = '4bb8651b3ac2180f23df1f39422c489810a13d445a94dc836c876c18c93ecefe'
$runtimeMember = 'lib/Windows_x86_64/rel/nvngx_dlssnr.dll'
$runtimeBytes = 266424944L
$runtimeHash = '2bc19bc09a7b89dd44f73e2ac0108fcc586e2154bdd0830f242f4f040e28789f'
$licenseBytes = 26727L
$licenseHash = '3027f23ca5a46dd9cb8183fbd522983a86f64d7daac5982912bf9f214671f294'
$temporary = $null
$stage = $null
$runtimeDirectory = $null
$createdRuntimeDirectory = $false
$updates = New-Object System.Collections.ArrayList
$committed = $false
$rollbackFailed = $false
$oldProgress = $ProgressPreference

function Assert-File([string]$Path, [long]$Bytes, [string]$Hash) {
    $item = Get-Item -LiteralPath $Path
    if ($item.PSIsContainer -or $item.Length -ne $Bytes) { throw "Unexpected file size: $Path" }
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ine $Hash) { throw "SHA-256 verification failed: $Path" }
}

function Assert-X64PE([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    $reader = New-Object System.IO.BinaryReader($stream)
    try {
        if ($reader.ReadUInt16() -ne 0x5a4d) { throw 'The NR runtime is not a Windows PE file.' }
        $stream.Position = 0x3c
        $offset = $reader.ReadUInt32()
        if ([long]$offset + 6 -gt $stream.Length) { throw 'Invalid PE header offset.' }
        $stream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x00004550 -or $reader.ReadUInt16() -ne 0x8664) {
            throw 'The NR runtime is not a Windows x64 binary.'
        }
    } finally { $reader.Dispose() }
}

function Expand-SelectedMembers([string]$Archive, [string]$Destination) {
    $extractors = New-Object System.Collections.ArrayList
    if ($SevenZipPath) {
        $resolved = Get-Item -LiteralPath $SevenZipPath
        if ($resolved.PSIsContainer) { throw 'SevenZipPath must name a 7-Zip executable.' }
        [void]$extractors.Add(@{ File = $resolved.FullName; Kind = '7z' })
    } else {
        $tar = Get-Command tar.exe -ErrorAction SilentlyContinue
        if ($tar) { [void]$extractors.Add(@{ File = $tar.Source; Kind = 'tar' }) }
        foreach ($name in @('7z.exe', '7zz.exe', '7za.exe')) {
            $command = Get-Command $name -ErrorAction SilentlyContinue
            if ($command) { [void]$extractors.Add(@{ File = $command.Source; Kind = '7z' }) }
        }
        foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
            if ($base) {
                $candidate = Join-Path $base '7-Zip\7z.exe'
                if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                    [void]$extractors.Add(@{ File = $candidate; Kind = '7z' })
                }
            }
        }
    }
    if (-not $extractors.Count) { throw 'No archive reader was found. Install 7-Zip or pass -SevenZipPath.' }
    $failures = New-Object System.Collections.ArrayList
    foreach ($extractor in $extractors) {
        if (Test-Path -LiteralPath $Destination) { Remove-Item -LiteralPath $Destination -Recurse -Force }
        [void][System.IO.Directory]::CreateDirectory($Destination)
        try {
            if ($extractor.Kind -eq 'tar') {
                $arguments = @('-xf', $Archive, '-C', $Destination, 'LICENSE.txt', $runtimeMember)
            } else {
                $arguments = @('x', '-y', ('-o' + $Destination), $Archive, 'LICENSE.txt', $runtimeMember)
            }
            $text = & $extractor.File @arguments 2>&1
            if ($LASTEXITCODE -ne 0) { throw ('Archive reader exited with code ' + $LASTEXITCODE) }
            $dll = Join-Path $Destination $runtimeMember
            $license = Join-Path $Destination 'LICENSE.txt'
            Assert-File $dll $runtimeBytes $runtimeHash
            Assert-File $license $licenseBytes $licenseHash
            return
        } catch { [void]$failures.Add(($extractor.File + ': ' + $_.Exception.Message)) }
    }
    throw ('Could not extract the two pinned SDK members. ' + ($failures -join '; '))
}

function Install-AtomicFile([string]$Source, [string]$Target) {
    $backup = Join-Path $stage ([System.Guid]::NewGuid().ToString('N') + '.backup')
    $existed = Test-Path -LiteralPath $Target
    $record = @{ Target = $Target; Backup = $backup; Existed = $existed; Promoted = $false }
    [void]$updates.Add($record)
    if ($existed) {
        [System.IO.File]::Replace($Source, $Target, $backup, $true)
    } else { [System.IO.File]::Move($Source, $Target) }
    $record.Promoted = $true
}

try {
    if (-not [Environment]::Is64BitOperatingSystem) { throw 'A 64-bit Windows installation is required.' }
    $destination = [System.IO.Path]::GetFullPath($InstallDirectory)
    [void][System.IO.Directory]::CreateDirectory($destination)
    $runtimeDirectory = Join-Path $destination 'neural-runtime'
    $temporary = Join-Path ([System.IO.Path]::GetTempPath()) ('RA2-DLSS5-' + [System.Guid]::NewGuid().ToString('N'))
    [void][System.IO.Directory]::CreateDirectory($temporary)
    $archive = Join-Path $temporary 'rtx-remix-ngx_sdk_dlnr-2.7z'
    if ($ArchivePath) {
        $archive = (Get-Item -LiteralPath $ArchivePath).FullName
    } else {
        $ProgressPreference = 'SilentlyContinue'
        Write-Host 'Downloading the official NVIDIA Neural Rendering package (about 351 MiB)...'
        Invoke-WebRequest -UseBasicParsing -Uri $archiveUrl -OutFile $archive -TimeoutSec 300
    }
    Assert-File $archive $archiveBytes $archiveHash
    $extracted = Join-Path $temporary 'selected-members'
    Expand-SelectedMembers $archive $extracted
    $sourceRuntime = Join-Path $extracted $runtimeMember
    Assert-X64PE $sourceRuntime
    $signature = Get-AuthenticodeSignature -LiteralPath $sourceRuntime
    if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or
        -not $signature.SignerCertificate -or
        $signature.SignerCertificate.Subject -notmatch '(?:^|,\s*)CN=NVIDIA Corporation(?:,|$)') {
        throw ('The pinned NR DLL did not pass NVIDIA publisher verification: ' + $signature.Status)
    }
    # Stage beside the installation so each final file replacement is on the same volume.
    $stage = Join-Path $destination ('.neural-runtime-stage-' + [System.Guid]::NewGuid().ToString('N'))
    [void][System.IO.Directory]::CreateDirectory($stage)
    $stagedRuntime = Join-Path $stage 'nvngx_dlssnr.dll'
    $stagedLicense = Join-Path $stage 'LICENSE.txt'
    [System.IO.File]::Copy($sourceRuntime, $stagedRuntime)
    [System.IO.File]::Copy((Join-Path $extracted 'LICENSE.txt'), $stagedLicense)
    Assert-File $stagedRuntime $runtimeBytes $runtimeHash
    Assert-File $stagedLicense $licenseBytes $licenseHash
    $createdRuntimeDirectory = -not (Test-Path -LiteralPath $runtimeDirectory)
    [void][System.IO.Directory]::CreateDirectory($runtimeDirectory)
    # Install the accompanying terms first; the executable payload is promoted last.
    Install-AtomicFile $stagedLicense (Join-Path $runtimeDirectory 'LICENSE.txt')
    Install-AtomicFile $stagedRuntime (Join-Path $runtimeDirectory 'nvngx_dlssnr.dll')
    $committed = $true
    Write-Host ('Installed the verified NVIDIA NR runtime in ' + $runtimeDirectory)
    Write-Host 'This installs a dependency. DLSS 5 becomes active only after a successful rendering evaluation on supported hardware.'
} catch {
    $originalError = $_
    for ($index = $updates.Count - 1; $index -ge 0; --$index) {
        $record = $updates[$index]
        try {
            if (Test-Path -LiteralPath $record.Backup -PathType Leaf) {
                if (Test-Path -LiteralPath $record.Target -PathType Leaf) {
                    [System.IO.File]::Replace($record.Backup, $record.Target, $null, $true)
                } else { [System.IO.File]::Move($record.Backup, $record.Target) }
            } elseif ($record.Promoted -and -not $record.Existed) {
                Remove-Item -LiteralPath $record.Target -Force
            }
        } catch { $rollbackFailed = $true; Write-Warning ('Could not restore ' + $record.Target + '; retained backup directory: ' + $stage) }
    }
    throw $originalError
} finally {
    $ProgressPreference = $oldProgress
    if ($temporary -and (Test-Path -LiteralPath $temporary)) { Remove-Item -LiteralPath $temporary -Recurse -Force -ErrorAction SilentlyContinue }
    if ($stage -and -not $rollbackFailed -and (Test-Path -LiteralPath $stage)) { Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue }
    if (-not $committed -and $createdRuntimeDirectory -and $runtimeDirectory -and (Test-Path -LiteralPath $runtimeDirectory)) {
        $remaining = @(Get-ChildItem -LiteralPath $runtimeDirectory -Force)
        if (-not $remaining.Count) { Remove-Item -LiteralPath $runtimeDirectory -Force -ErrorAction SilentlyContinue }
    }
}
