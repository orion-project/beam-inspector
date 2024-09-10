# Enable scripts execution: Set-ExecutionPolicy Bypass
# Restore restricted policy: Set-ExecutionPolicy Default

$SubjectApp = "beam-inspector"
$TargetFile = "track_mem_$(Get-Date -Format "yyyy-MM-ddTHH-mm-ss").csv"
$IntervalSecs = 300

function Format-Mem {
    param( $Mem )
    "$([math]::Round($Mem/1024/1024, 2))"
}

for ($true) {
    $Proc = Get-Process $SubjectApp
    if ($null -eq $Proc) {
        Exit(1)
    }

    $Row = [PSCustomObject]@{
        Time = Get-Date -Format "yyyy-MM-ddTHH:mm:ss"
        WorkingSet = Format-Mem $Proc.WorkingSet64
        PeakWorkingSet = Format-Mem $Proc.PeakWorkingSet64
        PagedMemory = Format-Mem $Proc.PagedMemorySize64
        PeakPagedMemory = Format-Mem $Proc.PeakPagedMemorySize64
        #PagedSysMemory = Format-Mem $Proc.PagedSystemMemorySize64
        #PrivateMemory = Format-Mem $Proc.PrivateMemorySize64
        #VirtualMemory = Format-Mem $Proc.VirtualMemorySize64
        #PeakVirtualMemory = Format-Mem $Proc.PeakVirtualMemorySize64
    }
    $Row | Export-Csv -Path $TargetFile -Append -NoTypeInformation

    #stdout
    Write-Output "$([PSCustomObject]@{
        Time = $Row.Time
        WorkingSet = $Row.WorkingSet
        PagedMemory = $Row.PagedMemory
    })"

    Start-Sleep -Seconds $IntervalSecs
}
