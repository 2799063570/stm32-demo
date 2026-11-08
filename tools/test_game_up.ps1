param([string]$VCVars = 'D:\VisualStudio\VC\Auxiliary\Build\vcvars64.bat')
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$gameRoot = (Get-ChildItem -LiteralPath $repoRoot -Directory | Where-Object { $_.Name -like '10-1 *' }).FullName
$testRoot = Join-Path $repoRoot 'tests'
$outputRoot = Join-Path $testRoot 'build'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
if (-not (Test-Path -LiteralPath $VCVars)) { throw 'Pass vcvars64.bat with -VCVars.' }
$argsText = '/nologo /utf-8 /std:c11 /W4 /WX /D_CRT_SECURE_NO_WARNINGS '
foreach ($include in @((Join-Path $testRoot 'mock'), (Join-Path $gameRoot 'Hardware'),
    (Join-Path $gameRoot 'System'), (Join-Path $gameRoot 'User'))) {
    $argsText += '/I"' + $include + '" '
}
foreach ($source in @((Join-Path $testRoot 'test_game_up.c'), (Join-Path $gameRoot 'System\Game.c'),
    (Join-Path $gameRoot 'System\StickInput.c'), (Join-Path $gameRoot 'System\GameView.c'),
    (Join-Path $gameRoot 'Hardware\OLED.c'))) {
    $argsText += '"' + $source + '" '
}
$commandText = 'call "' + $VCVars + '" >nul && cl ' + $argsText + ' /Fe:test_game_up.exe'
Push-Location -LiteralPath $outputRoot
try {
    & $env:ComSpec /d /c $commandText
    if ($LASTEXITCODE -ne 0) { throw 'Game host compilation failed.' }
    & (Join-Path $outputRoot 'test_game_up.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Game software tests failed.' }
} finally { Pop-Location }
