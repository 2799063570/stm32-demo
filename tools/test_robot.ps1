param([string]$VCVars = 'D:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$robot = Join-Path $root '12-1 ROS移动机器人下层控制器'
$output = Join-Path $root 'tests/build'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$argsText = '/nologo /utf-8 /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS '
$argsText += '/I"' + $robot + '/System" /I"' + $robot + '/User" '
$argsText += '"' + $root + '/tests/test_robot.c" "' + $robot + '/System/RobotControl.c" "' + $robot + '/System/RobotProtocol.c" /Fe:test_robot.exe'
Push-Location -LiteralPath $output
try {
    & $env:ComSpec /d /c ('call "' + $VCVars + '" >nul && cl ' + $argsText)
    if ($LASTEXITCODE -ne 0) { throw 'Robot test compilation failed' }
    & ./test_robot.exe
    if ($LASTEXITCODE -ne 0) { throw 'Robot regression failed' }
    python (Join-Path $root 'tests/test_robot_protocol.py')
    if ($LASTEXITCODE -ne 0) { throw 'Robot Python interoperability failed' }
} finally { Pop-Location }
