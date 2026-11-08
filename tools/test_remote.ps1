param([string]$VCVars = 'D:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat',
      [string]$Python = 'C:\Users\zlab\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe')
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$remoteRoot = (Get-ChildItem -LiteralPath $repoRoot -Directory | Where-Object { $_.Name -like '9-1 *' }).FullName
$testRoot = Join-Path $repoRoot 'tests'
$outputRoot = Join-Path $testRoot 'build'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
# 测试直接读取交付给串口助手的二进制文件，避免文档示例与协议代码脱节。
$assistantRoot = Join-Path $remoteRoot '串口助手报文'
Copy-Item -LiteralPath (Join-Path $assistantRoot '01_读取摇杆.bin') -Destination (Join-Path $outputRoot 'assistant_poll.bin')
Copy-Item -LiteralPath (Join-Path $assistantRoot '02_模拟六关节反馈.bin') -Destination (Join-Path $outputRoot 'assistant_demo.bin')
Copy-Item -LiteralPath (Join-Path $assistantRoot '03_HOME请求1完成.bin') -Destination (Join-Path $outputRoot 'assistant_home_done.bin')
if (-not (Test-Path -LiteralPath $VCVars)) { throw 'Pass vcvars64.bat with -VCVars.' }
$argsText = '/nologo /utf-8 /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS '
foreach ($include in @((Join-Path $testRoot 'mock'), (Join-Path $remoteRoot 'Hardware'),
    (Join-Path $remoteRoot 'System'), (Join-Path $remoteRoot 'User'))) { $argsText += '/I"' + $include + '" ' }
foreach ($source in @((Join-Path $testRoot 'test_remote.c'), (Join-Path $remoteRoot 'System\RemoteProtocol.c'),
    (Join-Path $remoteRoot 'System\RemoteInput.c'), (Join-Path $remoteRoot 'System\RemoteLink.c'),
    (Join-Path $remoteRoot 'System\RemoteView.c'), (Join-Path $remoteRoot 'Hardware\OLED.c'))) {
    $argsText += '"' + $source + '" '
}
$commandText = 'call "' + $VCVars + '" >nul && cl ' + $argsText + ' /Fe:test_remote.exe'
Push-Location -LiteralPath $outputRoot
try {
    & $env:ComSpec /d /c $commandText
    if ($LASTEXITCODE -ne 0) { throw 'Remote host C compilation failed.' }
    & (Join-Path $outputRoot 'test_remote.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Remote C tests failed.' }
    $cppRoot = Join-Path $repoRoot 'ros1\aubo_remote_bridge'
    $cppArgs = '/nologo /utf-8 /std:c++14 /EHsc /W4 /WX /I"' + (Join-Path $cppRoot 'include') + '" '
    $cppArgs += '"' + (Join-Path $testRoot 'test_remote_cpp.cpp') + '" '
    $cppArgs += '"' + (Join-Path $cppRoot 'src\remote_core.cpp') + '" RemoteProtocol.obj /Fe:test_remote_cpp.exe'
    & $env:ComSpec /d /c ('call "' + $VCVars + '" >nul && cl ' + $cppArgs)
    if ($LASTEXITCODE -ne 0) { throw 'Remote C++ core compilation failed.' }
    & (Join-Path $outputRoot 'test_remote_cpp.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Remote C++ tests failed.' }
} finally { Pop-Location }
Push-Location -LiteralPath $repoRoot
try {
    & $Python -m unittest discover -s tests -p test_remote.py -v
    if ($LASTEXITCODE -ne 0) { throw 'Remote Python tests failed.' }
} finally { Pop-Location }
