param(
    [Parameter(Mandatory = $true)][string]$ArchiveDirectory,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [switch]$Network
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$script:Checks = 0
function Assert-Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw ('CHECK FAILED: ' + $Message) }
    $script:Checks++
}
function Expect-Failure([scriptblock]$Action, [string]$Message) {
    $caught = $false
    try { & $Action } catch { $caught = $true }
    Assert-Check $caught $Message
}

$source = [IO.Path]::GetFullPath([IO.Path]::Combine($PSScriptRoot, '..', 'engine', 'prepare_runtime.ps1'))
$tokens = $null; $parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($source, [ref]$tokens, [ref]$parseErrors)
Assert-Check ($parseErrors.Count -eq 0) 'production PowerShell AST parses'
$definitions = @($ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]}, $false))
foreach ($definition in $definitions) { Invoke-Expression $definition.Extent.Text }
$sourceBytes = [IO.File]::ReadAllBytes($source)
Assert-Check ($sourceBytes[0] -eq 0xEF -and $sourceBytes[1] -eq 0xBB -and $sourceBytes[2] -eq 0xBF) 'UTF8 BOM supports Windows PowerShell 5.1 literals'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$taskRoot = [IO.Path]::Combine([IO.Path]::GetTempPath(), 'ra2-bootstrap-中文 空格-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($taskRoot)
[void][IO.Directory]::CreateDirectory($OutputDirectory)
$script:ToolboxRoot = $taskRoot
$script:RuntimeStatusPath = [IO.Path]::Combine($taskRoot, 'progress.ini')
$script:RuntimeCancelPath = [IO.Path]::Combine($taskRoot, 'cancel.flag')
$manifest = [IO.File]::ReadAllText([IO.Path]::Combine([IO.Path]::GetDirectoryName($source), 'runtime-dependencies.json')) | ConvertFrom-Json
$record = $manifest.python
$archivePath = [IO.Path]::Combine($ArchiveDirectory, $record.filename)
$networkVerified = $false

try {
    Test-PythonManifest $record
    Write-PrepareStatus 'downloading' "正在下载`n请稍候" 1 12 'dependency'
    $statusBytes = [IO.File]::ReadAllBytes($script:RuntimeStatusPath)
    Assert-Check ($statusBytes[0] -eq 0xFF -and $statusBytes[1] -eq 0xFE) 'UTF16LE status BOM'
    $statusText = [IO.File]::ReadAllText($script:RuntimeStatusPath, [Text.Encoding]::Unicode)
    Assert-Check ($statusText.Contains('Message=正在下载 请稍候')) 'newlines sanitized and Chinese status preserved'
    Assert-Check ($statusText.Contains('ErrorKind=dependency')) 'dependency retry classification preserved'

    $verified = Get-VerifiedArchiveBytes $archivePath $record
    Assert-Check ($verified -is [byte[]] -and $verified.Length -eq [long]$record.bytes) 'actual official Python bytes and SHA256'
    $stage = [IO.Path]::Combine($taskRoot, 'official-python')
    New-DependencyDirectory $stage
    Expand-PythonArchive $archivePath $record $stage
    Assert-Check ([IO.Directory]::GetFiles($stage).Count -eq $record.files.Count) 'all official Python members extracted'
    $archive = [IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        foreach ($entry in $archive.Entries) {
            $target = [IO.Path]::Combine($stage, $entry.FullName)
            if ($entry.FullName -eq 'python312._pth') {
                Assert-Check ([IO.File]::ReadAllText($target) -ceq "python312.zip`r`n.`r`n") 'embedded interpreter stays isolated without site hooks'
                continue
            }
            $stream = $entry.Open(); $memory = [IO.MemoryStream]::new()
            try { $stream.CopyTo($memory); $expected = $memory.ToArray() }
            finally { $stream.Dispose(); $memory.Dispose() }
            $actual = [IO.File]::ReadAllBytes($target)
            $sha = [Security.Cryptography.SHA256]::Create()
            try {
                Assert-Check ([BitConverter]::ToString($sha.ComputeHash($actual)) -ceq [BitConverter]::ToString($sha.ComputeHash($expected))) ('official Python member preserved: ' + $entry.FullName)
            } finally { $sha.Dispose() }
        }
    } finally { $archive.Dispose() }

    $bad = $record | ConvertTo-Json -Depth 5 | ConvertFrom-Json
    $bad.sha256 = '0' * 64
    Expect-Failure { [void](Get-VerifiedArchiveBytes $archivePath $bad) } 'mismatched SHA rejected before installation'
    $bad = $record | ConvertTo-Json -Depth 5 | ConvertFrom-Json
    $bad.bytes = [long]$record.bytes - 1
    Expect-Failure { [void](Get-VerifiedArchiveBytes $archivePath $bad) } 'truncated/unexpected size rejected'
    $bad = $record | ConvertTo-Json -Depth 5 | ConvertFrom-Json
    $bad.url = 'http://www.python.org/other.zip'
    Expect-Failure { Test-PythonManifest $bad } 'unfixed/non-HTTPS endpoint rejected'

    # Exercise the real extractor against authenticated synthetic malformed ZIPs.
    $maliciousNames = @('../escape', '/absolute', 'C:/escape', 'python.exe/', 'python.exe:stream', 'CON.txt', 'trailing.', 'a\b')
    foreach ($name in $maliciousNames) {
        $fixture = [IO.Path]::Combine($taskRoot, [Guid]::NewGuid().ToString('N') + '.zip')
        $file = [IO.File]::Open($fixture, [IO.FileMode]::CreateNew)
        $zip = [IO.Compression.ZipArchive]::new($file, [IO.Compression.ZipArchiveMode]::Create)
        try { $entry = $zip.CreateEntry($name); $writer = $entry.Open(); $writer.WriteByte(1); $writer.Dispose() }
        finally { $zip.Dispose(); $file.Dispose() }
        $fixtureBytes = [IO.File]::ReadAllBytes($fixture)
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $digest = [BitConverter]::ToString($sha.ComputeHash($fixtureBytes)).Replace('-', '').ToLowerInvariant() }
        finally { $sha.Dispose() }
        $fixtureRecord = $record | ConvertTo-Json -Depth 5 | ConvertFrom-Json
        $fixtureRecord.bytes = $fixtureBytes.Length; $fixtureRecord.sha256 = $digest
        $fixtureRecord.unpacked_bytes = 1; $fixtureRecord.files = @($name)
        $fixtureStage = [IO.Path]::Combine($taskRoot, 'stage-' + [Guid]::NewGuid().ToString('N'))
        New-DependencyDirectory $fixtureStage
        Expect-Failure { Expand-PythonArchive $fixture $fixtureRecord $fixtureStage } ('unsafe Python member rejected: ' + $name)
        Assert-Check ([IO.Directory]::GetFiles($fixtureStage).Count -eq 0) 'unsafe archive never writes any member'
    }

    $cache = [IO.Path]::Combine($taskRoot, 'cache')
    New-DependencyDirectory $cache
    $cached = [IO.Path]::Combine($cache, $record.sha256 + '-' + $record.filename)
    [IO.File]::Copy($archivePath, $cached)
    Assert-Check ((Get-PythonArchive $record $cache) -ceq $cached) 'verified archive cache reused'
    [IO.File]::WriteAllText($script:RuntimeCancelPath, '')
    Expect-Failure { [void](Get-PythonArchive $record $cache) } 'cancelled cache read exits'
    Assert-Check ([IO.File]::Exists($cached)) 'cancellation keeps completed verified archive'
    [IO.File]::Delete($script:RuntimeCancelPath)

    # Windows executable probing is explicitly stubbed for these portable
    # transaction checks. Extraction, locking, promotion and JSON are real.
    $originalProbe = (Get-Item Function:Test-PythonCandidate).ScriptBlock
    $script:ProbeMode = 'good'
    function Test-PythonCandidate([string]$Candidate, [bool]$Pinned = $false) {
        Test-PrepareCancellation
        if (-not [IO.File]::Exists($Candidate)) { return $false }
        if ($Candidate.Contains('.python-stage-')) {
            if ($script:ProbeMode -eq 'cancel-stage') {
                [IO.File]::WriteAllText($script:RuntimeCancelPath, '')
                Test-PrepareCancellation
            }
            return $script:ProbeMode -ne 'reject-all'
        }
        if (-not $Pinned) { return $script:ProbeMode -eq 'legacy-good' }
        return $script:ProbeMode -eq 'good'
    }
    $runtime = [IO.Path]::Combine($taskRoot, 'runtime')
    $managed = [IO.Path]::Combine($runtime, 'downloaded')
    New-DependencyDirectory $runtime; New-DependencyDirectory $managed
    $first = Get-PrivatePython $record $runtime $managed $cache
    Assert-Check ([IO.File]::Exists($first)) 'staged interpreter published after child-probe success'
    $indexPath = [IO.Path]::Combine($managed, 'python.json')
    $firstIndex = [IO.File]::ReadAllText($indexPath)
    Assert-Check ((Get-PrivatePython $record $runtime $managed $cache) -ceq $first) 'healthy managed interpreter reused'
    [IO.File]::WriteAllText([IO.Path]::Combine($runtime, 'python.exe'), 'incomplete legacy install')
    Assert-Check ((Get-PrivatePython $record $runtime $managed $cache) -ceq $first) 'exe existence alone does not admit broken legacy runtime'
    $script:ProbeMode = 'legacy-good'
    Assert-Check ((Get-PrivatePython $record $runtime $managed $cache) -ceq [IO.Path]::Combine($runtime, 'python.exe')) 'healthy legacy interpreter reused without download'
    $script:ProbeMode = 'reject-managed'
    $replacement = Get-PrivatePython $record $runtime $managed $cache
    Assert-Check ($replacement -cne $first -and [IO.File]::Exists($first)) 'repair uses a new immutable directory and retains old interpreter'
    $beforeCancel = [IO.File]::ReadAllText($indexPath)
    $script:ProbeMode = 'cancel-stage'
    Expect-Failure { [void](Get-PrivatePython $record $runtime $managed $cache) } 'cancellation during staged validation aborts publication'
    Assert-Check ([IO.File]::ReadAllText($indexPath) -ceq $beforeCancel) 'cancelled repair retains previous pointer'
    Assert-Check ([IO.Directory]::GetDirectories($managed, '.python-stage-*').Count -eq 0) 'cancelled private stage cleaned'
    [IO.File]::Delete($script:RuntimeCancelPath)
    Set-Item Function:Test-PythonCandidate $originalProbe

    if ($Network) {
        $networkCache = [IO.Path]::Combine($taskRoot, 'network-cache')
        New-DependencyDirectory $networkCache
        $downloaded = Get-PythonArchive $record $networkCache
        [void](Get-VerifiedArchiveBytes $downloaded $record)
        Assert-Check ([IO.File]::Exists($downloaded)) 'production async HTTP download matches official SHA256'
        $networkVerified = $true
    }
    $result = [ordered]@{
        checks = $script:Checks
        powershell_version = $PSVersionTable.PSVersion.ToString()
        host_platform = [Environment]::OSVersion.Platform.ToString()
        production_functions_executed = $true
        actual_official_python_zip_extracted = $true
        actual_official_https_download_verified = $networkVerified
        interpreter_probe_transaction_checks = 'controlled stub; no Windows binary execution'
        windows_powershell_51_executed = $false
        windows_python_executed = $false
    }
    $result | ConvertTo-Json | Set-Content -LiteralPath ([IO.Path]::Combine($OutputDirectory, 'powershell-worker-result.json')) -Encoding utf8
    $result | ConvertTo-Json
} finally {
    if ([IO.Directory]::Exists($taskRoot)) { [IO.Directory]::Delete($taskRoot, $true) }
}
