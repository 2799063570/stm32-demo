param([string]$VCVars = 'D:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat')
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exampleRoot = Join-Path $repoRoot '11-1 AUBO笛卡尔串口遥控'
$outputRoot = Join-Path $repoRoot 'tests\build\jog'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
# 真实固件解析交付给串口助手的文件，并导出INPUT供Python逐字节交叉检查。
foreach ($name in @('01_poll_seq1','02_joints_seq2','03_joints_invalid_seq3','04_fault_seq4')) {
    Copy-Item -LiteralPath (Join-Path $exampleRoot "串口助手报文\$name.bin") -Destination $outputRoot
}
if (-not (Test-Path -LiteralPath $VCVars)) { throw 'Pass vcvars64.bat with -VCVars.' }
$argsText = '/nologo /utf-8 /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS '
foreach ($folder in @('System','User','Hardware')) { $argsText += '/I"' + (Join-Path $exampleRoot $folder) + '" ' }
foreach ($source in @((Join-Path $repoRoot 'tests\test_jog_remote.c'),
    (Join-Path $exampleRoot 'System\RemoteProtocol.c'), (Join-Path $exampleRoot 'System\RemoteJogProtocol.c'),
    (Join-Path $exampleRoot 'System\JogRemote.c'), (Join-Path $exampleRoot 'System\JogView.c'))) {
    $argsText += '"' + $source + '" '
}
Push-Location -LiteralPath $outputRoot
try {
    & $env:ComSpec /d /c ('call "' + $VCVars + '" >nul && cl ' + $argsText + ' /Fe:test_jog_remote.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Cartesian remote C compilation failed.' }
    & (Join-Path $outputRoot 'test_jog_remote.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Cartesian remote tests failed.' }
} finally { Pop-Location }
& python -m unittest discover -s (Join-Path $repoRoot 'tests') -p test_jog_assistant.py -v
if ($LASTEXITCODE -ne 0) { throw 'Cartesian serial assistant tests failed.' }
