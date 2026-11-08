param([string]$SourcePath,[string]$PythonPath,[string]$Work)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2.0
$tokens=$null;$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($SourcePath,[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw ($parseErrors | Out-String)}
$definitions=@($ast.EndBlock.Statements | Where-Object { $_ -is [Management.Automation.Language.FunctionDefinitionAst] })
foreach($definition in $definitions){. ([scriptblock]::Create($definition.Extent.Text))}
$script:ToolboxRoot=$Work
$script:RuntimeCancelPath=[IO.Path]::Combine($Work,'not-cancelled.flag')
$script:RuntimeStatusPath=[IO.Path]::Combine($Work,'probe-progress.ini')
$payload="import subprocess,sys; subprocess.Popen([sys.executable,'-c','import time;time.sleep(3)']); print('RA2_PYTHON_OK',flush=True)"
$watch=[Diagnostics.Stopwatch]::StartNew()
$completed=$false;$exception='';$result=$null
try{$result=Invoke-PrivatePython $PythonPath @('-c',$payload) 1;$completed=$true}catch{$exception=$_.Exception.Message}
[pscustomobject]@{powershell=$PSVersionTable.PSVersion.ToString();parse_errors=$parseErrors.Count;functions=$definitions.Count;probe_timeout_seconds=1;elapsed_seconds=$watch.Elapsed.TotalSeconds;returned_successfully=$completed;error=$exception;result=$result} | ConvertTo-Json -Depth 5
