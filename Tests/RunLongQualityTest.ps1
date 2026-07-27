param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [int64] $MaximumWorkingSetBytes = 805306368,
    [int] $MaximumHandleCount = 4096
)

$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $Executable
$startInfo.Arguments = "--stress"
$startInfo.UseShellExecute = $false
$process = [System.Diagnostics.Process]::Start($startInfo)
$peakWorkingSet = 0L
$peakHandles = 0

while (-not $process.HasExited) {
    $process.Refresh()
    $peakWorkingSet = [Math]::Max($peakWorkingSet, $process.WorkingSet64)
    $peakHandles = [Math]::Max($peakHandles, $process.HandleCount)
    Start-Sleep -Milliseconds 100
}

$process.WaitForExit()
$process.Refresh()
$exitCode = $process.ExitCode
if ($null -eq $exitCode -or $exitCode -ne 0) {
    throw "Stress executable failed with exit code $exitCode."
}
if ($peakWorkingSet -gt $MaximumWorkingSetBytes) {
    throw "Peak working set $peakWorkingSet exceeded $MaximumWorkingSetBytes bytes."
}
if ($peakHandles -gt $MaximumHandleCount) {
    throw "Peak handle count $peakHandles exceeded $MaximumHandleCount."
}

Write-Host "Long quality test passed. Peak working set: $peakWorkingSet bytes; peak handles: $peakHandles."
