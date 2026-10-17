param([string]$VCVars = 'D:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat')
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$motorRoot = (Get-ChildItem -LiteralPath $repoRoot -Directory | Where-Object { $_.Name -like '5-1 *' }).FullName
$testRoot = Join-Path $repoRoot 'tests'
$outputRoot = Join-Path $testRoot 'build'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
if (-not (Test-Path -LiteralPath $VCVars)) { throw 'Pass the Visual Studio vcvars64.bat path with -VCVars.' }
$argsText = '/nologo /utf-8 /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS '
$argsText += '/I"' + (Join-Path $testRoot 'mock') + '" '
$argsText += '/I"' + (Join-Path $motorRoot 'System') + '" '
$argsText += '/I"' + (Join-Path $motorRoot 'Hardware') + '" '
$argsText += '/I"' + (Join-Path $motorRoot 'User') + '" '
foreach ($source in @((Join-Path $testRoot 'test_logic.c'), (Join-Path $motorRoot 'System\PID.c'),
    (Join-Path $motorRoot 'System\Command.c'), (Join-Path $motorRoot 'System\Control.c'))) {
    $argsText += '"' + $source + '" '
}
$commandText = 'call "' + $VCVars + '" >nul && cl ' + $argsText + ' /Fe:test_logic.exe'
Push-Location -LiteralPath $outputRoot
try {
    & $env:ComSpec /d /c $commandText
    if ($LASTEXITCODE -ne 0) { throw 'Host test compilation failed.' }
    & (Join-Path $outputRoot 'test_logic.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Host logic tests failed.' }
} finally { Pop-Location }
