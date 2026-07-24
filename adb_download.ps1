[CmdletBinding(PositionalBinding = $false)]
param(
    [Alias("S")]
    [string]$ResourceDirArg = "",
    [Alias("B")]
    [string]$BuildDirArg = "",
    [ValidateSet("default", "app", "boot")]
    [string]$Mode = "",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ScriptArgs
)

# adb_download.ps1 - ARCS firmware flashing through Windows ADB.

$ErrorActionPreference = "Stop"

$script:MaxDevices = if ($env:MAX_DEVICES) { [int]$env:MAX_DEVICES } else { 10 }
$script:RecoveryTimeout = if ($env:RECOVERY_TIMEOUT) { [int]$env:RECOVERY_TIMEOUT } else { 120 }
$script:PollInterval = if ($env:POLL_INTERVAL) { [int]$env:POLL_INTERVAL } else { 2 }
$script:BuildDir = if ($env:BUILD_DIR) { $env:BUILD_DIR } else { "build" }
$script:ResourceDir = if ($env:RES_DIR) { $env:RES_DIR } else { "res/arcs-mini" }

$script:FlashMode = "default"
if (-not [string]::IsNullOrWhiteSpace($ResourceDirArg)) {
    $script:ResourceDir = $ResourceDirArg
}
if (-not [string]::IsNullOrWhiteSpace($BuildDirArg)) {
    $script:BuildDir = $BuildDirArg
}
if (-not [string]::IsNullOrWhiteSpace($Mode)) {
    $script:FlashMode = $Mode
}
$script:RepoRoot = ""
$script:AdbCmd = ""
$script:TargetCount = 0
$script:MissingTidCount = 0

$script:LocalFiles = @()
$script:RemotePaths = @()

$script:NormalTransportIds = @()
$script:InitialBootDevices = @()
$script:TransportToUsb = @{}

$script:SelectedBootTargets = @()
$script:SelectedBootSet = @{}
$script:TidToBootTarget = @{}

$script:ScannedDevices = @()

$UpgradeEnterCmd = "root;listenai;upgrade enter"

function Write-Ok {
    param([string]$Message)
    Write-Host $Message -ForegroundColor Green
}

function Write-Warn {
    param([string]$Message)
    Write-Host $Message -ForegroundColor Yellow
}

function Write-Fail {
    param([string]$Message)
    Write-Host $Message -ForegroundColor Red
}

function Show-Usage {
    Write-Host "Usage: powershell -ExecutionPolicy Bypass -File adb_download.ps1 [-S <res-dir>] [-B <build-dir>] [-Mode default|app|boot]"
    Write-Host "       powershell -ExecutionPolicy Bypass -File adb_download.ps1 [-S <res-dir>] [app|boot]"
    Write-Host "  no mode/default: flash lpk-tagged images except boot"
    Write-Host "  app:             only flash the lpk-tagged app image"
    Write-Host "  boot:            flash all lpk-tagged images and run upgrade enter"
    Write-Host "  -S <res-dir>:    resource directory containing partition_table.json (default: res/arcs-mini)"
    Write-Host "  -B <build-dir>:  build output directory for `${BUILD_DIR} entries (default: build)"
}

function Find-RepoRoot {
    $scriptDir = Split-Path -Parent $PSCommandPath
    $searchDir = Resolve-Path -LiteralPath $scriptDir

    while ($searchDir) {
        $psScript = Join-Path $searchDir "adb_download.ps1"

        if (Test-Path -LiteralPath $psScript -PathType Leaf) {
            $script:RepoRoot = [string]$searchDir
            break
        }

        $parent = Split-Path -Parent $searchDir
        if ([string]::IsNullOrEmpty($parent) -or $parent -eq $searchDir) {
            break
        }
        $searchDir = $parent
    }

    if ([string]::IsNullOrEmpty($script:RepoRoot)) {
        Write-Fail "Error: unable to locate repository root"
        exit 1
    }

    Set-Location -LiteralPath $script:RepoRoot
}

function Parse-Args {
    param([string[]]$ArgsToParse)

    for ($i = 0; $i -lt $ArgsToParse.Count; $i++) {
        $arg = $ArgsToParse[$i]
        switch ($arg) {
            "" { }
            "-h" { Show-Usage; exit 0 }
            "--help" { Show-Usage; exit 0 }
            "help" { Show-Usage; exit 0 }
            "-S" {
                if ($i + 1 -ge $ArgsToParse.Count) {
                    Write-Fail "Error: -S requires a resource directory"
                    Show-Usage
                    exit 1
                }
                $script:ResourceDir = $ArgsToParse[++$i]
            }
            "-B" {
                if ($i + 1 -ge $ArgsToParse.Count) {
                    Write-Fail "Error: -B requires a build directory"
                    Show-Usage
                    exit 1
                }
                $script:BuildDir = $ArgsToParse[++$i]
            }
            "-Mode" {
                if ($i + 1 -ge $ArgsToParse.Count) {
                    Write-Fail "Error: -Mode requires default, app, or boot"
                    Show-Usage
                    exit 1
                }
                $script:FlashMode = $ArgsToParse[++$i]
            }
            default {
                if ($arg -in @("default", "app", "boot")) {
                    $script:FlashMode = $arg
                } else {
                    Write-Fail "Error: unsupported argument: $arg"
                    Show-Usage
                    exit 1
                }
            }
        }
    }

    if ($script:FlashMode -notin @("default", "app", "boot")) {
        Write-Fail "Error: unsupported mode: $script:FlashMode"
        Show-Usage
        exit 1
    }
}

function ConvertTo-RemotePath {
    param([string]$Address)

    $addr = $Address
    if ($addr.StartsWith("0x", [System.StringComparison]::OrdinalIgnoreCase)) {
        $addr = $addr.Substring(2)
    }

    $addr = $addr.TrimStart([char]"0")
    if ([string]::IsNullOrEmpty($addr)) {
        $addr = "0"
    }

    return "/RAW/NAND/$addr"
}

function Build-FlashTable {
    $resourceDir = $script:ResourceDir.TrimEnd("\", "/")
    $partitionFile = Join-Path $resourceDir "partition_table.json"
    if (-not (Test-Path -LiteralPath $partitionFile -PathType Leaf)) {
        Write-Fail "Error: partition table not found: $partitionFile"
        exit 1
    }

    switch ($script:FlashMode) {
        "app" { Write-Host "Current mode: app firmware only" }
        "boot" { Write-Host "Current mode: include boot flashing" }
        default { Write-Host "Current mode: flash static resources and app firmware, skip boot" }
    }

    Write-Host "Resource directory: $resourceDir"
    Write-Host "Partition table: $partitionFile"

    $partitionTable = Get-Content -LiteralPath $partitionFile -Raw -Encoding UTF8 | ConvertFrom-Json
    $baseDir = Split-Path -Parent $partitionFile

    foreach ($image in $partitionTable.images) {
        $name = [string]$image.name
        $rawFile = [string]$image.file
        $addr = [string]$image.addr
        if ([string]::IsNullOrEmpty($name) -or
            [string]::IsNullOrWhiteSpace($rawFile) -or
            [string]::IsNullOrWhiteSpace($addr)) {
            continue
        }
        if ($image.tags -notcontains "lpk") {
            continue
        }

        if ($script:FlashMode -eq "app" -and $name -ne "app") {
            continue
        }
        if ($name -eq "boot" -and $script:FlashMode -ne "boot") {
            continue
        }

        $usesBuildDir = $rawFile -match '\$\{BUILD_DIR\}'
        $localPath = $rawFile.Replace('${BUILD_DIR}', $script:BuildDir).Replace('${RES_DIR}', $resourceDir)
        if (-not [System.IO.Path]::IsPathRooted($localPath) -and -not $usesBuildDir) {
            $localPath = $localPath -replace '^[.][\\/]', ''
            $localPath = Join-Path $baseDir $localPath
        }

        $script:LocalFiles += $localPath
        $script:RemotePaths += (ConvertTo-RemotePath $addr)
    }

    if ($script:LocalFiles.Count -eq 0) {
        Write-Fail "Error: no files to flash, check $partitionFile and flash mode"
        exit 1
    }

    Write-Host "Files to flash:"
    for ($i = 0; $i -lt $script:LocalFiles.Count; $i++) {
        Write-Host "- $($script:LocalFiles[$i]) -> $($script:RemotePaths[$i])"
    }
}

function Verify-LocalFiles {
    Write-Host "Checking local files..."
    foreach ($localPath in $script:LocalFiles) {
        if (-not (Test-Path -LiteralPath $localPath -PathType Leaf)) {
            Write-Fail "Error: local file does not exist: $localPath"
            exit 1
        }
    }
    Write-Ok "File check complete"
}

function Add-AdbCandidate {
    param(
        [System.Collections.ArrayList]$Candidates,
        [string]$Candidate
    )

    if ([string]::IsNullOrWhiteSpace($Candidate)) {
        return
    }

    $candidatePath = $Candidate.Trim('"')
    if (-not $Candidates.Contains($candidatePath)) {
        [void]$Candidates.Add($candidatePath)
    }
}

function Count-AdbDevices {
    param([string]$Candidate)

    $count = 0
    $lines = @(& $Candidate devices -l 2>$null | ForEach-Object { ([string]$_) -replace "`r|`0", "" })
    foreach ($line in ($lines | Select-Object -Skip 1)) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }

        $parts = $line -split "\s+"
        if ($parts.Count -ge 2 -and $parts[1] -eq "device") {
            $count++
        }
    }

    return $count
}

function Detect-AdbTool {
    $candidates = New-Object System.Collections.ArrayList

    if ($env:ADB_CMD) {
        Add-AdbCandidate $candidates $env:ADB_CMD
    }

    $bundledAdb = Join-Path $script:RepoRoot "tools/adb/adb.exe"
    if (Test-Path -LiteralPath $bundledAdb -PathType Leaf) {
        Add-AdbCandidate $candidates $bundledAdb
    }

    $pathAdb = Get-Command adb -ErrorAction SilentlyContinue
    if ($pathAdb) {
        Add-AdbCandidate $candidates $pathAdb.Source
    }

    Add-AdbCandidate $candidates "adb"

    $bestAdb = ""
    $bestCount = -1

    foreach ($candidate in $candidates) {
        try {
            & $candidate version >$null 2>&1
            if ($LASTEXITCODE -ne 0) {
                continue
            }

            $candidateCount = Count-AdbDevices $candidate
            Write-Host "ADB candidate: $candidate (devices: $candidateCount)"
            if ($candidateCount -gt $bestCount) {
                $bestAdb = $candidate
                $bestCount = $candidateCount
            }
        } catch {
            continue
        }
    }

    if ([string]::IsNullOrEmpty($bestAdb)) {
        Write-Fail "Error: ADB is unavailable. Install Android SDK Platform-Tools and add adb to PATH."
        exit 1
    }

    $script:AdbCmd = $bestAdb
    Write-Host "Selected ADB: $script:AdbCmd (visible devices: $bestCount)"
    & $script:AdbCmd version
}

function Collect-DevicesRaw {
    @(& $script:AdbCmd devices -l 2>$null | ForEach-Object { ([string]$_) -replace "`r|`0", "" })
}

function Scan-ConnectedDevices {
    $devices = @()

    foreach ($line in Collect-DevicesRaw) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }
        if ($line -like "List of devices attached*") {
            continue
        }
        if ($line -notmatch "^(\S+)\s+(\S+)") {
            continue
        }

        $serial = $Matches[1]
        $state = $Matches[2]
        if ($state -ne "device") {
            continue
        }

        $usb = ""
        $tid = ""
        if ($line -match "usb:(\S+)") {
            $usb = $Matches[1]
        }
        if ($line -match "transport_id:(\S+)") {
            $tid = $Matches[1]
        }

        $devices += [pscustomobject]@{
            Serial = $serial
            Usb = $usb
            TransportId = $tid
        }
    }

    return $devices
}

function Get-AdbDeviceKey {
    param($Device)

    if (-not [string]::IsNullOrEmpty($Device.TransportId)) {
        return "tid:$($Device.TransportId)"
    }

    return "serial:$($Device.Serial)"
}

function ConvertTo-AdbTarget {
    param($Device)

    $key = Get-AdbDeviceKey $Device
    $display = $Device.Serial
    if (-not [string]::IsNullOrEmpty($Device.TransportId)) {
        $display = "$($Device.Serial) (transport_id:$($Device.TransportId))"
    }

    [pscustomobject]@{
        Serial = $Device.Serial
        Usb = $Device.Usb
        TransportId = $Device.TransportId
        Key = $key
        Display = $display
    }
}

function Get-AdbTargetSelectorArgs {
    param($Target)

    if (-not [string]::IsNullOrEmpty($Target.TransportId)) {
        return @("-t", $Target.TransportId)
    }

    return @("-s", $Target.Serial)
}

function Categorize-ConnectedDevices {
    $seenTid = @{}
    $seenBoot = @{}
    $script:MissingTidCount = 0

    foreach ($device in $script:ScannedDevices) {
        if ([string]::IsNullOrEmpty($device.Serial)) {
            continue
        }

        if ($device.Serial -like "BOOT-*") {
            $bootTarget = ConvertTo-AdbTarget $device
            if (-not $seenBoot.ContainsKey($bootTarget.Key)) {
                $script:InitialBootDevices += $bootTarget
                $seenBoot[$bootTarget.Key] = $true
            }
            continue
        }

        if ([string]::IsNullOrEmpty($device.TransportId)) {
            $script:MissingTidCount++
            continue
        }

        if (-not $seenTid.ContainsKey($device.TransportId)) {
            $script:NormalTransportIds += $device.TransportId
            $script:TransportToUsb[$device.TransportId] = $device.Usb
            $seenTid[$device.TransportId] = $true
        }
    }

    $script:TargetCount = $script:NormalTransportIds.Count + $script:InitialBootDevices.Count

    if ($script:TargetCount -eq 0) {
        Write-Fail "Error: no usable devices detected (state must be device)"
        Write-Host "Current `"$script:AdbCmd devices -l`" output:"
        & $script:AdbCmd devices -l
        Write-Host "Parsed device rows (serial | usb | transport_id):"
        if ($script:ScannedDevices.Count -eq 0) {
            Write-Host "(empty)"
        } else {
            foreach ($device in $script:ScannedDevices) {
                Write-Host "$($device.Serial) | $($device.Usb) | $($device.TransportId)"
            }
        }
        exit 1
    }

    if ($script:TargetCount -gt $script:MaxDevices) {
        Write-Fail "Error: detected $script:TargetCount devices, greater than MAX_DEVICES=$script:MaxDevices"
        Write-Host "Set a larger value, for example: `$env:MAX_DEVICES=$script:TargetCount; powershell -ExecutionPolicy Bypass -File adb_download.ps1"
        exit 1
    }

    Write-Host "Detected devices: $script:TargetCount"
    Write-Host "- normal mode devices: $($script:NormalTransportIds.Count)"
    Write-Host "- recovery BOOT-* devices: $($script:InitialBootDevices.Count)"
    if ($script:MissingTidCount -gt 0) {
        Write-Warn "Warning: ignored $script:MissingTidCount normal device(s) without transport_id"
    }
}

function Send-RecoveryCommands {
    if ($script:NormalTransportIds.Count -eq 0) {
        return
    }

    Write-Host "Sending recovery commands..."
    $failures = 0

    foreach ($tid in $script:NormalTransportIds) {
        & $script:AdbCmd -t $tid reboot recovery >$null 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[transport_id:$tid] sent: reboot recovery"
            continue
        }

        & $script:AdbCmd -t $tid shell recovery >$null 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[transport_id:$tid] sent: shell recovery"
            continue
        }

        Write-Fail "[transport_id:$tid] failed to enter recovery"
        $failures++
    }

    if ($failures -gt 0) {
        Write-Fail "Error: $failures device(s) failed to enter recovery"
        exit 1
    }
}

function Add-SelectedBootTarget {
    param($Target)

    if (-not $script:SelectedBootSet.ContainsKey($Target.Key)) {
        $script:SelectedBootTargets += $Target
        $script:SelectedBootSet[$Target.Key] = $true
    }
}

function Wait-ForBootDevices {
    foreach ($bootTarget in $script:InitialBootDevices) {
        Add-SelectedBootTarget $bootTarget
    }

    Write-Host "Waiting for devices to enter recovery and expose BOOT serials..."
    $deadline = (Get-Date).AddSeconds($script:RecoveryTimeout)

    while ((Get-Date) -lt $deadline) {
        $scanLines = @(Scan-ConnectedDevices)
        $scanBootTargets = @()
        $scanBootByUsb = @{}

        foreach ($device in $scanLines) {
            if ($device.Serial -like "BOOT-*") {
                $bootTarget = ConvertTo-AdbTarget $device
                $scanBootTargets += $bootTarget
                if (-not [string]::IsNullOrEmpty($device.Usb)) {
                    $scanBootByUsb[$device.Usb] = $bootTarget
                }
            }
        }

        foreach ($tid in $script:NormalTransportIds) {
            if ($script:TidToBootTarget.ContainsKey($tid)) {
                continue
            }

            $devUsb = $script:TransportToUsb[$tid]
            if (-not [string]::IsNullOrEmpty($devUsb) -and $scanBootByUsb.ContainsKey($devUsb)) {
                $matchedBoot = $scanBootByUsb[$devUsb]
                $script:TidToBootTarget[$tid] = $matchedBoot
                Add-SelectedBootTarget $matchedBoot
            }
        }

        foreach ($tid in $script:NormalTransportIds) {
            if ($script:TidToBootTarget.ContainsKey($tid)) {
                continue
            }

            foreach ($bootTarget in $scanBootTargets) {
                if (-not $script:SelectedBootSet.ContainsKey($bootTarget.Key)) {
                    $script:TidToBootTarget[$tid] = $bootTarget
                    Add-SelectedBootTarget $bootTarget
                    break
                }
            }
        }

        if ($script:SelectedBootTargets.Count -ge $script:TargetCount) {
            break
        }

        $remaining = $script:TargetCount - $script:SelectedBootTargets.Count
        Write-Host "Still waiting for $remaining device(s) to enter recovery..."
        Start-Sleep -Seconds $script:PollInterval
    }

    if ($script:SelectedBootTargets.Count -lt $script:TargetCount) {
        Write-Fail "Error: recovery device count is insufficient, expected $script:TargetCount, got $($script:SelectedBootTargets.Count)"
        Write-Host "Recognized BOOT devices:"
        foreach ($bootTarget in $script:SelectedBootTargets) {
            Write-Host "- $($bootTarget.Display)"
        }
        exit 1
    }

    Write-Host "Target recovery devices:"
    foreach ($bootTarget in $script:SelectedBootTargets) {
        Write-Host "- $($bootTarget.Display)"
    }
}

function Flash-OneDevice {
    param($Target)

    $display = $Target.Display
    $selectorArgs = @(Get-AdbTargetSelectorArgs $Target)

    if ($script:FlashMode -eq "boot") {
        Write-Host "[$display] running upgrade preparation command"
        & $script:AdbCmd @selectorArgs shell $UpgradeEnterCmd >$null
        if ($LASTEXITCODE -ne 0) {
            Write-Fail "[$display] upgrade preparation failed"
            return 1
        }
    }

    for ($i = 0; $i -lt $script:LocalFiles.Count; $i++) {
        $localPath = $script:LocalFiles[$i]
        $remotePath = $script:RemotePaths[$i]

        Write-Host "[$display] push $localPath -> $remotePath"
        & $script:AdbCmd @selectorArgs push $localPath $remotePath >$null
        if ($LASTEXITCODE -ne 0) {
            Write-Fail "[$display] push failed: $localPath"
            return 1
        }
    }

    & $script:AdbCmd @selectorArgs shell reboot hard >$null 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[$display] flashing complete, device is rebooting"
        return 0
    }

    & $script:AdbCmd @selectorArgs reboot >$null 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[$display] flashing complete, adb reboot sent"
        return 0
    }

    Write-Fail "[$display] files pushed, but reboot command failed"
    return 2
}

function Flash-AllDevices {
    Write-Host "Start flashing..."
    $successSerials = @()
    $failedSerials = @()

    foreach ($target in $script:SelectedBootTargets) {
        $result = Flash-OneDevice $target
        if ($result -eq 0) {
            $successSerials += $target.Display
        } else {
            $failedSerials += $target.Display
        }
    }

    Write-Host ""
    Write-Host "Flash summary:"
    Write-Host "- success: $($successSerials.Count)"
    foreach ($serial in $successSerials) {
        Write-Host "  [OK] $serial"
    }

    Write-Host "- failed: $($failedSerials.Count)"
    foreach ($serial in $failedSerials) {
        Write-Host "  [FAIL] $serial"
    }

    if ($failedSerials.Count -gt 0) {
        Write-Fail "Some devices failed. Check USB connection, authorization prompts, and device logs."
        exit 1
    }

    Write-Ok "All devices flashed successfully"
}

function Main {
    Find-RepoRoot
    Parse-Args $ScriptArgs
    Build-FlashTable
    Verify-LocalFiles
    Detect-AdbTool

    $script:ScannedDevices = @(Scan-ConnectedDevices)
    Categorize-ConnectedDevices
    Send-RecoveryCommands
    Wait-ForBootDevices
    Flash-AllDevices
}

Main
