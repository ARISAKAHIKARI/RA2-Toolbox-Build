# Windows PowerShell 5.1 entry point. The caller scopes its execution policy to
# this child process; this script never changes registry or execution policy.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][Alias('Profile')][string]$CursorProfile,
    [Parameter(Mandatory = $true)][string]$Status,
    [Parameter(Mandatory = $true)][string]$Cancel
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

function Test-ReparsePath([string]$TaskPath) {
    if (-not [IO.File]::Exists($TaskPath) -and -not [IO.Directory]::Exists($TaskPath)) { return $false }
    return ([IO.File]::GetAttributes($TaskPath) -band [IO.FileAttributes]::ReparsePoint) -ne 0
}

function New-DependencyDirectory([string]$TaskPath) {
    if (Test-ReparsePath $TaskPath) { throw '依赖目录不能是链接或重解析点。' }
    [void][IO.Directory]::CreateDirectory($TaskPath)
    if (Test-ReparsePath $TaskPath) { throw '依赖目录不能是链接或重解析点。' }
}

function Test-PrepareCancellation {
    if ([IO.File]::Exists($script:RuntimeCancelPath)) {
        throw [OperationCanceledException]::new('已取消依赖准备。')
    }
}

function Write-AtomicText([string]$TaskPath, [string]$TaskText, [Text.Encoding]$TaskEncoding) {
    $temporary = $TaskPath + '.tmp-' + [Guid]::NewGuid().ToString('N')
    try {
        [IO.File]::WriteAllText($temporary, $TaskText, $TaskEncoding)
        for ($attempt = 0; $attempt -lt 6; ++$attempt) {
            try {
                if ([IO.File]::Exists($TaskPath)) { [IO.File]::Replace($temporary, $TaskPath, [NullString]::Value) }
                else { [IO.File]::Move($temporary, $TaskPath) }
                break
            } catch [IO.IOException] {
                if ($attempt -eq 5) { throw }
                Start-Sleep -Milliseconds 50
            }
        }
    } finally {
        if ([IO.File]::Exists($temporary)) { [IO.File]::Delete($temporary) }
    }
}

function Write-PrepareStatus([string]$State, [string]$Message, [long]$Done = 0, [long]$Total = 0, [string]$ErrorKind = '') {
    $safeMessage = $Message.Replace("`r", ' ').Replace("`n", ' ')
    if ($safeMessage.Length -gt 800) { $safeMessage = $safeMessage.Substring(0, 800) }
    $text = "[Progress]`r`nDone=$Done`r`nTotal=$Total`r`nState=$State`r`nMessage=$safeMessage`r`nErrorKind=$ErrorKind`r`n"
    Write-AtomicText $script:RuntimeStatusPath $text ([Text.Encoding]::Unicode)
}

function ConvertTo-ProcessArgument([string]$Value) {
    if ($Value.IndexOf([char]0) -ge 0) { throw '进程参数包含无效字符。' }
    # Windows CommandLineToArgv/CRT quoting, including trailing backslashes.
    $quoted = [Text.RegularExpressions.Regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $quoted = [Text.RegularExpressions.Regex]::Replace($quoted, '(\\+)$', '$1$1')
    return '"' + $quoted + '"'
}

function Invoke-PrivatePython([string]$PythonPath, [string[]]$Arguments, [int]$TimeoutSeconds = 0) {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $PythonPath
    $info.Arguments = (($Arguments | ForEach-Object { ConvertTo-ProcessArgument $_ }) -join ' ')
    $info.WorkingDirectory = $script:ToolboxRoot
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [Text.Encoding]::UTF8
    $info.StandardErrorEncoding = [Text.Encoding]::UTF8
    $child = [Diagnostics.Process]::new()
    $child.StartInfo = $info
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $cancelWatch = $null
    try {
        Test-PrepareCancellation
        [void]$child.Start()
        $outputTask = $child.StandardOutput.ReadToEndAsync()
        $errorTask = $child.StandardError.ReadToEndAsync()
        while (-not $child.WaitForExit(100)) {
            if ([IO.File]::Exists($script:RuntimeCancelPath)) {
                if ($null -eq $cancelWatch) { $cancelWatch = [Diagnostics.Stopwatch]::StartNew() }
                # Probes have no useful partial work. The real worker gets time
                # to close its network stream and remove its private stage.
                if ($TimeoutSeconds -gt 0 -or $cancelWatch.Elapsed.TotalSeconds -ge 20) {
                    $child.Kill()
                    [void]$child.WaitForExit(5000)
                    throw [OperationCanceledException]::new('已取消预处理。')
                }
            }
            if ($TimeoutSeconds -gt 0 -and $watch.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
                $child.Kill()
                [void]$child.WaitForExit(5000)
                throw 'Python 运行环境检查超时。'
            }
        }
        $drainWatch = [Diagnostics.Stopwatch]::StartNew()
        while (-not $outputTask.IsCompleted -or -not $errorTask.IsCompleted) {
            Test-PrepareCancellation
            if ($drainWatch.Elapsed.TotalSeconds -ge 2 -or
                ($TimeoutSeconds -gt 0 -and $watch.Elapsed.TotalSeconds -ge $TimeoutSeconds)) {
                throw 'Python 运行环境输出未正常关闭。'
            }
            Start-Sleep -Milliseconds 50
        }
        return [PSCustomObject]@{ Code = $child.ExitCode; Output = $outputTask.Result + $errorTask.Result }
    } finally {
        try { if ($null -ne $child -and -not $child.HasExited) { $child.Kill(); [void]$child.WaitForExit(5000) } } catch { }
        try { $child.StandardOutput.Close(); $child.StandardError.Close() } catch { }
        $child.Dispose()
    }
}

function Test-PythonCandidate([string]$Candidate, [bool]$Pinned = $false) {
    if (-not [IO.File]::Exists($Candidate) -or (Test-ReparsePath $Candidate)) { return $false }
    $probe = 'import sys,struct,ssl,hashlib,zipfile,ctypes,json,urllib.request; assert sys.version_info[:2]==(3,12) and struct.calcsize("P")==8; print("RA2_PYTHON_OK")'
    if ($Pinned) { $probe += '; assert sys.version_info[:3]==(3,12,10)' }
    try {
        $result = Invoke-PrivatePython $Candidate @('-I', '-S', '-c', $probe) 30
        return $result.Code -eq 0 -and $result.Output.Contains('RA2_PYTHON_OK')
    } catch [OperationCanceledException] { throw }
    catch { return $false }
}

function Get-DependencyLock([string]$TaskPath) {
    if (Test-ReparsePath $TaskPath) { throw '依赖锁文件不能是链接。' }
    $watch = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        Test-PrepareCancellation
        try { return [IO.File]::Open($TaskPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
        catch [IO.IOException] {
            if ($watch.Elapsed.TotalMinutes -ge 10) { throw '另一个工具箱仍在准备 Python，请稍后重试。' }
            Write-PrepareStatus 'dependency_wait' '正在等待另一个工具箱完成 Python 准备…'
            Start-Sleep -Milliseconds 200
        }
    }
}

function Test-PythonManifest($Record) {
    if ($Record.version -cne '3.12.10' -or $Record.abi -cne 'cp312' -or
        $Record.filename -cne 'python-3.12.10-embed-amd64.zip' -or
        $Record.url -cne 'https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip' -or
        $Record.sha256 -notmatch '^[a-f0-9]{64}$' -or
        [long]$Record.bytes -le 0 -or [long]$Record.bytes -gt 20000000 -or
        [long]$Record.unpacked_bytes -le 0 -or [long]$Record.unpacked_bytes -gt 64000000 -or
        $Record.files.Count -lt 1 -or $Record.files.Count -gt 128) {
        throw 'Python 依赖清单无效。'
    }
}

function Get-VerifiedArchiveBytes([string]$TaskPath, $Record) {
    Test-PrepareCancellation
    if (Test-ReparsePath $TaskPath) { throw '下载缓存不能是链接。' }
    $stream = [IO.File]::Open($TaskPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        if ($stream.Length -ne [long]$Record.bytes) { throw 'Python 下载大小不符。' }
        $bytes = [byte[]]::new([int]$Record.bytes)
        $offset = 0
        while ($offset -lt $bytes.Length) {
            Test-PrepareCancellation
            $read = $stream.Read($bytes, $offset, [Math]::Min(65536, $bytes.Length - $offset))
            if ($read -le 0) { throw 'Python 下载文件被截断。' }
            $offset += $read
        }
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $digest = [BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-', '').ToLowerInvariant() }
        finally { $sha.Dispose() }
        if ($digest -cne $Record.sha256) { throw 'Python 下载 SHA256 校验失败，未安装。' }
        # Do not enumerate a byte[] into PowerShell's pipeline.
        return ,$bytes
    } finally { $stream.Dispose() }
}

function Wait-NetworkOperation($Operation, $Request, [int]$TimeoutSeconds) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    try {
        while (-not $Operation.AsyncWaitHandle.WaitOne(150)) {
            Test-PrepareCancellation
            if ($watch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw '下载超时。' }
        }
        Test-PrepareCancellation
    } catch { $Request.Abort(); throw }
}

function Get-PythonArchive($Record, [string]$CacheDirectory) {
    Test-PythonManifest $Record
    $target = [IO.Path]::Combine($CacheDirectory, $Record.sha256 + '-' + $Record.filename)
    if ([IO.File]::Exists($target)) {
        try { [void](Get-VerifiedArchiveBytes $target $Record); return $target }
        catch [OperationCanceledException] { throw }
        catch { if (Test-ReparsePath $target) { throw }; [IO.File]::Delete($target) }
    }
    for ($attempt = 0; $attempt -lt 2; ++$attempt) {
        Test-PrepareCancellation
        $temporary = $target + '.part-' + [Guid]::NewGuid().ToString('N')
        $request = $null; $response = $null; $source = $null; $destination = $null
        try {
            Write-PrepareStatus 'downloading' '正在从 python.org 下载 Python 预处理运行环境…' 0 ([long]$Record.bytes)
            $request = [Net.HttpWebRequest]::Create([Uri]$Record.url)
            $request.AllowAutoRedirect = $false
            $request.UseDefaultCredentials = $false
            $request.UserAgent = 'RA2Toolbox-DependencySetup/1'
            $request.Timeout = 15000
            $request.ReadWriteTimeout = 30000
            $operation = $request.BeginGetResponse($null, $null)
            try {
                Wait-NetworkOperation $operation $request 15
                $response = $request.EndGetResponse($operation)
            } finally { $operation.AsyncWaitHandle.Close() }
            if ([int]$response.StatusCode -ne 200 -or $response.ResponseUri.AbsoluteUri -cne $Record.url -or
                ($response.ContentLength -ge 0 -and $response.ContentLength -ne [long]$Record.bytes)) {
                throw 'Python 下载响应与固定清单不符。'
            }
            $source = $response.GetResponseStream()
            $destination = [IO.File]::Open($temporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
            $buffer = [byte[]]::new(65536)
            [long]$received = 0
            $deadline = [Diagnostics.Stopwatch]::StartNew()
            $lastProgress = [Diagnostics.Stopwatch]::StartNew()
            while ($true) {
                Test-PrepareCancellation
                if ($deadline.Elapsed.TotalMinutes -gt 10) { throw 'Python 下载超过时限。' }
                $operation = $source.BeginRead($buffer, 0, $buffer.Length, $null, $null)
                try {
                    Wait-NetworkOperation $operation $request 30
                    $read = $source.EndRead($operation)
                } finally { $operation.AsyncWaitHandle.Close() }
                if ($read -eq 0) { break }
                $received += $read
                if ($received -gt [long]$Record.bytes) { throw 'Python 下载大小超过清单。' }
                $destination.Write($buffer, 0, $read)
                if ($lastProgress.ElapsedMilliseconds -ge 250) {
                    $message = '正在下载 Python · {0:N1} / {1:N1} MiB' -f ($received / 1MB), ([long]$Record.bytes / 1MB)
                    Write-PrepareStatus 'downloading' $message $received ([long]$Record.bytes)
                    $lastProgress.Restart()
                }
            }
            $destination.Flush($true)
            $destination.Dispose(); $destination = $null
            [void](Get-VerifiedArchiveBytes $temporary $Record)
            Test-PrepareCancellation
            [IO.File]::Move($temporary, $target)
            return $target
        } catch [OperationCanceledException] { throw }
        catch {
            if ($attempt -eq 1) { throw ('Python 下载失败：' + $_.Exception.Message + ' 检查网络后再次预处理即可重试。') }
            Write-PrepareStatus 'downloading' 'Python 下载中断，正在重试…'
        } finally {
            if ($null -ne $destination) { $destination.Dispose() }
            if ($null -ne $source) { $source.Dispose() }
            if ($null -ne $response) { $response.Dispose() }
            if ($null -ne $request) { $request.Abort() }
            if ([IO.File]::Exists($temporary)) { [IO.File]::Delete($temporary) }
        }
    }
    throw 'Python 下载未完成。'
}

function Expand-PythonArchive([string]$ArchivePath, $Record, [string]$StageDirectory) {
    $bytes = Get-VerifiedArchiveBytes $ArchivePath $Record
    $memory = [IO.MemoryStream]::new($bytes, $false)
    $archive = [IO.Compression.ZipArchive]::new($memory, [IO.Compression.ZipArchiveMode]::Read, $false)
    try {
        if ($archive.Entries.Count -ne $Record.files.Count) { throw 'Python 压缩包成员数不符。' }
        $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        [long]$unpacked = 0
        foreach ($entry in $archive.Entries) {
            Test-PrepareCancellation
            $name = $entry.FullName
            # The pinned embeddable distribution is flat: no path traversal or
            # directory/symlink extraction is necessary for any valid member.
            if ($name -notmatch '^[A-Za-z0-9_.-]+$' -or $name -in @('.', '..') -or
                $name.EndsWith('.') -or $name -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)' -or
                -not $names.Add($name) -or $Record.files -cnotcontains $name) { throw 'Python 压缩包包含不安全或未声明的成员。' }
            # ExternalAttributes is absent before .NET Framework 4.7.2. The
            # authenticated archive and exact flat whitelist still apply, and
            # all members are created as regular files rather than links.
            $attributesProperty = $entry.PSObject.Properties['ExternalAttributes']
            if ($null -ne $attributesProperty) {
                [long]$attributes = $attributesProperty.Value
                if ((($attributes -shr 16) -band 0xF000) -eq 0xA000 -or
                    ($attributes -band 0x400) -ne 0) { throw 'Python 压缩包包含链接或重解析点。' }
            }
            if ($entry.Length -lt 0 -or $entry.Length -gt [long]$Record.unpacked_bytes) { throw 'Python 成员大小无效。' }
            $unpacked += $entry.Length
        }
        if ($unpacked -ne [long]$Record.unpacked_bytes) { throw 'Python 解压大小不符。' }
        foreach ($entry in $archive.Entries) {
            Test-PrepareCancellation
            $destinationPath = [IO.Path]::Combine($StageDirectory, $entry.FullName)
            $source = $entry.Open()
            $destination = [IO.File]::Open($destinationPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
            try {
                $buffer = [byte[]]::new(65536)
                [long]$written = 0
                while (($read = $source.Read($buffer, 0, $buffer.Length)) -gt 0) {
                    Test-PrepareCancellation
                    $written += $read
                    if ($written -gt $entry.Length) { throw 'Python 解压超出成员大小。' }
                    $destination.Write($buffer, 0, $read)
                }
                if ($written -ne $entry.Length) { throw 'Python 成员被截断。' }
            } finally { $destination.Dispose(); $source.Dispose() }
        }
        # Isolation is retained; site-packages is supplied explicitly by the
        # verified worker. No global site, pip, .pth hooks or PATH changes.
        [IO.File]::WriteAllText([IO.Path]::Combine($StageDirectory, 'python312._pth'), "python312.zip`r`n.`r`n", [Text.Encoding]::ASCII)
    } finally { $archive.Dispose(); $memory.Dispose() }
}

function Get-PrivatePython($Record, [string]$RuntimeDirectory, [string]$ManagedDirectory, [string]$CacheDirectory) {
    Write-PrepareStatus 'dependency_check' '正在检查本机已有的预处理运行环境…'
    $legacy = [IO.Path]::Combine($RuntimeDirectory, 'python.exe')
    if (Test-PythonCandidate $legacy) { return $legacy }
    $lock = Get-DependencyLock ([IO.Path]::Combine($ManagedDirectory, 'python.lock'))
    try {
        $indexPath = [IO.Path]::Combine($ManagedDirectory, 'python.json')
        if ([IO.File]::Exists($indexPath) -and -not (Test-ReparsePath $indexPath) -and ([IO.FileInfo]::new($indexPath)).Length -lt 4096) {
            try {
                $index = [IO.File]::ReadAllText($indexPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
                $pattern = '^cpython-' + $Record.sha256.Substring(0, 12) + '-[a-f0-9]{32}$'
                if ($index.sha256 -ceq $Record.sha256 -and $index.directory -cmatch $pattern) {
                    $folder = [IO.Path]::Combine($ManagedDirectory, $index.directory)
                    $candidate = [IO.Path]::Combine($folder, 'python.exe')
                    if (-not (Test-ReparsePath $folder) -and (Test-PythonCandidate $candidate $true)) { return $candidate }
                }
            } catch [OperationCanceledException] { throw }
            catch { }
        }
        $archivePath = Get-PythonArchive $Record $CacheDirectory
        Write-PrepareStatus 'installing' '正在校验并安装 Python；原有安装保持不变…'
        $stage = [IO.Path]::Combine($ManagedDirectory, '.python-stage-' + [Guid]::NewGuid().ToString('N'))
        $finalName = 'cpython-' + $Record.sha256.Substring(0, 12) + '-' + [Guid]::NewGuid().ToString('N')
        $final = [IO.Path]::Combine($ManagedDirectory, $finalName)
        $published = $false
        New-DependencyDirectory $stage
        try {
            Expand-PythonArchive $archivePath $Record $stage
            if (-not (Test-PythonCandidate ([IO.Path]::Combine($stage, 'python.exe')) $true)) { throw '下载的 Python 环境无法加载；已保留缓存，可重试。' }
            Test-PrepareCancellation
            # The probe has exited and closed its handles before the directory
            # moves. Never rename the interpreter running the installer.
            [IO.Directory]::Move($stage, $final)
            $indexText = @{ directory = $finalName; version = $Record.version; sha256 = $Record.sha256 } | ConvertTo-Json -Compress
            Write-AtomicText $indexPath $indexText ([Text.UTF8Encoding]::new($false))
            $published = $true
            return [IO.Path]::Combine($final, 'python.exe')
        } finally {
            if ([IO.Directory]::Exists($stage)) { [IO.Directory]::Delete($stage, $true) }
            if (-not $published -and [IO.Directory]::Exists($final)) { [IO.Directory]::Delete($final, $true) }
        }
    } finally { $lock.Dispose() }
}

$script:ToolboxRoot = [IO.Path]::GetFullPath([IO.Path]::Combine($PSScriptRoot, '..'))
$script:RuntimeStatusPath = [IO.Path]::GetFullPath($Status)
$script:RuntimeCancelPath = [IO.Path]::GetFullPath($Cancel)
$profilePath = [IO.Path]::GetFullPath($CursorProfile)

try {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) { throw '预处理依赖安装仅用于 64 位 Windows。' }
    if (-not [IO.Directory]::Exists($profilePath)) { throw '光标包目录不存在。' }
    if ([IO.Path]::GetFileName($script:RuntimeStatusPath) -cne 'progress.ini' -or
        [IO.Path]::GetFileName($script:RuntimeCancelPath) -cne 'cancel.flag' -or
        [IO.Path]::GetDirectoryName($script:RuntimeStatusPath) -cne [IO.Path]::GetDirectoryName($script:RuntimeCancelPath)) { throw '预处理状态路径无效。' }
    $taskDirectory = [IO.Path]::GetDirectoryName($script:RuntimeStatusPath)
    $taskParent = [IO.Path]::GetDirectoryName($taskDirectory)
    $expectedParent = [IO.Path]::Combine($profilePath, '.preprocess')
    if (-not [string]::Equals($taskParent, $expectedParent, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Directory]::Exists($taskDirectory) -or (Test-ReparsePath $taskDirectory) -or (Test-ReparsePath $taskParent)) { throw '预处理任务目录无效。' }
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    # Enable the protocol supported by the official endpoints, preserving the
    # process defaults and all normal certificate-chain/hostname verification.
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $manifestPath = [IO.Path]::Combine($PSScriptRoot, 'runtime-dependencies.json')
    if (([IO.FileInfo]::new($manifestPath)).Length -gt 65536) { throw '依赖清单大小无效。' }
    $manifest = [IO.File]::ReadAllText($manifestPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
    if ($manifest.schema_version -ne 1 -or $manifest.platform -cne 'windows-x86_64') { throw '不支持的依赖清单。' }
    Test-PythonManifest $manifest.python
    Test-PrepareCancellation
    $runtimeDirectory = [IO.Path]::Combine($script:ToolboxRoot, 'runtime')
    New-DependencyDirectory $runtimeDirectory
    $managedDirectory = [IO.Path]::Combine($runtimeDirectory, 'downloaded')
    New-DependencyDirectory $managedDirectory
    $cacheDirectory = [IO.Path]::Combine($runtimeDirectory, 'download-cache')
    New-DependencyDirectory $cacheDirectory
    $python = Get-PrivatePython $manifest.python $runtimeDirectory $managedDirectory $cacheDirectory
    Test-PrepareCancellation
    $helper = [IO.Path]::Combine($PSScriptRoot, 'prepare_runtime.py')
    $result = Invoke-PrivatePython $python @('-I', '-S', $helper, '--profile', $profilePath, '--status', $script:RuntimeStatusPath, '--cancel', $script:RuntimeCancelPath)
    if ($result.Code -ne 0 -and -not [IO.File]::Exists($script:RuntimeStatusPath + '.error.txt')) {
        [IO.File]::WriteAllText($script:RuntimeStatusPath + '.error.txt', $result.Output, [Text.Encoding]::UTF8)
        Write-PrepareStatus 'error' '依赖准备或预处理引擎退出；详情见当前任务日志，可再次预处理重试。' 0 0 'dependency'
    }
    exit $result.Code
} catch [OperationCanceledException] {
    Write-PrepareStatus 'cancelled' '已取消预处理准备；已验证的下载缓存保留。'
    exit 0
} catch {
    $detail = $_.Exception.Message
    try {
        [IO.File]::WriteAllText($script:RuntimeStatusPath + '.error.txt', ($_ | Out-String), [Text.Encoding]::UTF8)
        Write-PrepareStatus 'error' ($detail + ' 修正网络或目录权限后，再次预处理可重试。') 0 0 'dependency'
    } catch { [Console]::Error.WriteLine($detail) }
    exit 1
}
