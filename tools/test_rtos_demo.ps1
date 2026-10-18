param([string]$VCVars = 'D:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat')
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$demoRoot = (Get-ChildItem -LiteralPath $repoRoot -Directory | Where-Object { $_.Name -like '7-1 *' }).FullName
$testRoot = Join-Path $repoRoot 'tests'
$outputRoot = Join-Path $testRoot 'build'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
if (-not (Test-Path -LiteralPath $VCVars)) { throw 'Pass vcvars64.bat with -VCVars.' }
$argsText = '/nologo /utf-8 /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS '
foreach ($include in @((Join-Path $testRoot 'rtos_mock'), (Join-Path $demoRoot 'Hardware'), (Join-Path $demoRoot 'System'))) {
    $argsText += '/I"' + $include + '" '
}
foreach ($source in @((Join-Path $testRoot 'test_rtos_demo.c'), (Join-Path $demoRoot 'System\DemoCommand.c'), (Join-Path $demoRoot 'Hardware\OLED_SPI.c'))) {
    $argsText += '"' + $source + '" '
}
$commandText = 'call "' + $VCVars + '" >nul && cl ' + $argsText + ' /Fe:test_rtos_demo.exe'
Push-Location -LiteralPath $outputRoot
try {
    & $env:ComSpec /d /c $commandText
    if ($LASTEXITCODE -ne 0) { throw 'FreeRTOS demo host compilation failed.' }
    & (Join-Path $outputRoot 'test_rtos_demo.exe')
    if ($LASTEXITCODE -ne 0) { throw 'FreeRTOS demo software tests failed.' }
} finally { Pop-Location }
