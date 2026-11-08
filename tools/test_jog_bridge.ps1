param(
    [string]$BridgeRoot = 'D:\files\code\wheeltec_robot\wheeltec_robot\aubo\aubo_remote_bridge',
    [string]$VCVars = 'D:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat'
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$mcuRoot = Join-Path $repoRoot '11-1 AUBO笛卡尔串口遥控'
$outputRoot = Join-Path $repoRoot 'tests\build\jog_bridge'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$includeArgs = '/I"' + (Join-Path $BridgeRoot 'include') + '" /I"' + (Join-Path $mcuRoot 'System') + '" /I"' + (Join-Path $mcuRoot 'User') + '" '
$cArgs = '/nologo /utf-8 /std:c11 /W4 /WX /c ' + $includeArgs
foreach ($file in @((Join-Path $BridgeRoot 'src\RemoteProtocol.c'), (Join-Path $BridgeRoot 'src\RemoteJogProtocol.c'), (Join-Path $mcuRoot 'System\JogRemote.c'))) {
    $cArgs += '"' + $file + '" '
}
$cppArgs = '/nologo /utf-8 /std:c++14 /EHsc /W4 /WX ' + $includeArgs
foreach ($file in @((Join-Path $repoRoot 'tests\test_jog_bridge_cpp.cpp'), (Join-Path $BridgeRoot 'src\remote_core.cpp'), (Join-Path $BridgeRoot 'src\jog_remote_core.cpp'))) {
    $cppArgs += '"' + $file + '" '
}
$cppArgs += 'RemoteProtocol.obj RemoteJogProtocol.obj JogRemote.obj /Fe:test_jog_bridge.exe'
Push-Location -LiteralPath $outputRoot
try {
    & $env:ComSpec /d /c ('call "' + $VCVars + '" >nul && cl ' + $cArgs)
    if ($LASTEXITCODE -ne 0) { throw 'C protocol/MCU compilation failed.' }
    & $env:ComSpec /d /c ('call "' + $VCVars + '" >nul && cl ' + $cppArgs)
    if ($LASTEXITCODE -ne 0) { throw 'C++ bridge core compilation failed.' }
    & (Join-Path $outputRoot 'test_jog_bridge.exe')
    if ($LASTEXITCODE -ne 0) { throw 'MCU/bridge handshake regression failed.' }
} finally { Pop-Location }
