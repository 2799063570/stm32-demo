param([string]$VCVars = 'D:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat')
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$motorRoot = (Get-ChildItem -LiteralPath $repoRoot -Directory | Where-Object { $_.Name -like '5-1 *' }).FullName
$oledRoot = (Get-ChildItem -LiteralPath $repoRoot -Directory | Where-Object { $_.Name -like '1-1 *' }).FullName
$testRoot = Join-Path $repoRoot 'tests'
$outputRoot = Join-Path $testRoot 'build'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
if (-not (Test-Path -LiteralPath $VCVars)) { throw 'Pass vcvars64.bat with -VCVars.' }
$argsText = '/nologo /utf-8 /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS '
foreach ($include in @((Join-Path $testRoot 'mock'), (Join-Path $oledRoot 'Hardware'),
    (Join-Path $oledRoot 'System'), (Join-Path $motorRoot 'System'), (Join-Path $motorRoot 'User'))) {
    $argsText += '/I"' + $include + '" '
}
foreach ($source in @((Join-Path $testRoot 'test_oled.c'), (Join-Path $oledRoot 'Hardware\OLED.c'),
    (Join-Path $motorRoot 'System\Dashboard.c'))) {
    $argsText += '"' + $source + '" '
}
$commandText = 'call "' + $VCVars + '" >nul && cl ' + $argsText + ' /Fe:test_oled.exe'
Push-Location -LiteralPath $outputRoot
try {
    & $env:ComSpec /d /c $commandText
    if ($LASTEXITCODE -ne 0) { throw 'OLED host compilation failed.' }
    & (Join-Path $outputRoot 'test_oled.exe')
    if ($LASTEXITCODE -ne 0) { throw 'OLED software tests failed.' }
} finally { Pop-Location }
