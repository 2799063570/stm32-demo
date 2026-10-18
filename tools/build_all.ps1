param([string]$UV4 = 'D:\MGK-ARM\UV4\UV4.exe')
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not (Test-Path -LiteralPath $UV4)) { throw 'Pass the path to Keil UV4.exe with -UV4.' }
$projects = Get-ChildItem -LiteralPath $repoRoot -Directory | Where-Object { $_.Name -match '^\d+-1 ' } | Sort-Object Name
foreach ($project in $projects) {
    $process = Start-Process -FilePath $UV4 -ArgumentList @('-r','Project.uvprojx','-j0','-o','Objects\Build.log') -WorkingDirectory $project.FullName -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(45000)) { throw "Build timed out: $($project.Name)" }
    $log = Join-Path $project.FullName 'Objects\Build.log'
    Write-Output $project.Name
    Get-Content -LiteralPath $log | Select-String 'Program Size|Error\(s\)|error:|Error:'
    if ($process.ExitCode -ne 0) { throw "Keil build failed (exit $($process.ExitCode)); see $log" }
    $fromelf = Join-Path (Split-Path (Split-Path $UV4 -Parent) -Parent) 'ARM\ARMCC\bin\fromelf.exe'
    if (Test-Path -LiteralPath $fromelf) {
        & $fromelf --bin --output (Join-Path $project.FullName 'Objects\Project.bin') (Join-Path $project.FullName 'Objects\Project.axf')
        if ($LASTEXITCODE -ne 0) { throw "BIN export failed: $($project.Name)" }
    }
}
