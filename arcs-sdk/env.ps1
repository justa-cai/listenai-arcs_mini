param(
    [ValidateSet("", "setup", "check", "info", "help")]
    [string]$Command = "",
    [switch]$ForceSubst
)

$ErrorActionPreference = "Stop"

$ScriptSdkRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$SubstDrive = "R:"
$SubstRoot = "$SubstDrive\"
$ListenAiToolsVersion = "v0.1.0"
$ListenAiToolsZip = "listenai-tools-windows-v0.1.0.zip"
$ListenAiToolsUrl = "https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/dev-tools/windows-amd64/$ListenAiToolsVersion/$ListenAiToolsZip"
$ToolchainZip = "nuclei_riscv_newlibc_prebuilt_win64_2025.10_slim.zip"
$ToolchainUrl = "https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/tools/toolchain/windows-amd64/$ToolchainZip"

function Write-EnvOk($Message) {
    Write-Host "  [OK]   $Message"
}

function Write-EnvInfo($Message) {
    Write-Host "  [INFO] $Message"
}

function Write-EnvWarn($Message) {
    Write-Warning $Message
}

function Convert-ToCMakePath($Path) {
    return ([System.IO.Path]::GetFullPath($Path)).Replace("\", "/")
}

function Test-Toolchain($Path) {
    $gcc = Join-Path $Path "bin\riscv64-unknown-elf-gcc.exe"
    if (-not (Test-Path $gcc)) {
        return $false
    }

    & $gcc --version *> $null
    return ($LASTEXITCODE -eq 0)
}

function Test-ListenAiTools($Path) {
    $cmake = Join-Path $Path "cmake\bin\cmake.exe"
    $ninja = Join-Path $Path "ninja\ninja.exe"
    $kconfig = Join-Path $Path "kconfig\kconfig.py"
    $menuconfig = Join-Path $Path "menuconfig\menuconfig.py"
    $versionFile = Join-Path $Path "VERSION"
    if ((-not (Test-Path $cmake)) -or (-not (Test-Path $ninja)) -or
        (-not (Test-Path $kconfig)) -or (-not (Test-Path $menuconfig)) -or
        (-not (Test-Path $versionFile))) {
        return $false
    }

    if (((Get-Content -LiteralPath $versionFile -Raw).Trim()) -ne $ListenAiToolsVersion) {
        return $false
    }

    & $cmake --version *> $null
    if ($LASTEXITCODE -ne 0) {
        return $false
    }

    & $ninja --version *> $null
    return ($LASTEXITCODE -eq 0)
}

function Invoke-DownloadFile($Url, $OutputPath) {
    try {
        Invoke-WebRequest -Uri $Url -OutFile $OutputPath -UseBasicParsing
    } catch {
        if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
            & curl.exe -L --fail -o $OutputPath $Url
            if ($LASTEXITCODE -eq 0) {
                return
            }
        }
        throw
    }
}

function Install-ListenAiTools($InstallRoot, $Force) {
    if ((-not $Force) -and (Test-ListenAiTools $ToolsPath)) {
        Write-EnvOk "listenai-tools already installed"
        return
    }

    New-Item -ItemType Directory -Force $InstallRoot | Out-Null
    $zipPath = Join-Path ([System.IO.Path]::GetTempPath()) $ListenAiToolsZip

    Write-EnvInfo "Downloading listenai-tools: $ListenAiToolsUrl"
    Invoke-DownloadFile $ListenAiToolsUrl $zipPath

    Write-EnvInfo "Extracting listenai-tools to $InstallRoot"
    Expand-Archive -LiteralPath $zipPath -DestinationPath $InstallRoot -Force
    Remove-Item -LiteralPath $zipPath -Force -ErrorAction SilentlyContinue

    if (-not (Test-ListenAiTools $ToolsPath)) {
        throw "listenai-tools install failed: $ToolsPath"
    }

    Write-EnvOk "listenai-tools installed"
}

function Install-Toolchain($InstallRoot, $Force) {
    if ((-not $Force) -and (Test-Toolchain $ToolchainPath)) {
        Write-EnvOk "GCC toolchain already installed"
        return
    }

    New-Item -ItemType Directory -Force $InstallRoot | Out-Null
    $zipPath = Join-Path ([System.IO.Path]::GetTempPath()) $ToolchainZip

    Write-EnvInfo "Downloading GCC toolchain: $ToolchainUrl"
    Invoke-DownloadFile $ToolchainUrl $zipPath

    Write-EnvInfo "Extracting GCC toolchain to $InstallRoot"
    Expand-Archive -LiteralPath $zipPath -DestinationPath $InstallRoot -Force
    Remove-Item -LiteralPath $zipPath -Force -ErrorAction SilentlyContinue

    if (-not (Test-Toolchain $ToolchainPath)) {
        throw "GCC toolchain install failed: $ToolchainPath"
    }

    Write-EnvOk "GCC toolchain installed"
}

function Add-ToPath($PathItem) {
    if ((Test-Path $PathItem) -and (($env:Path -split ";") -notcontains $PathItem)) {
        $env:Path = "$PathItem;$env:Path"
    }
}

function Test-PathNeedsSubst($Path) {
    if ($Path -match '[^\x00-\x7F]') {
        return $true
    }
    if ($Path -match '\s') {
        return $true
    }
    return ($Path -match '(?i)(^|\\)OneDrive($|\\| - )')
}

function Show-Help() {
    Write-Host "ARCS SDK Windows environment setup"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  .\env.ps1          Check and set current PowerShell environment"
    Write-Host "  .\env.ps1 setup    Re-download tools and set environment"
    Write-Host "  .\env.ps1 check    Print environment status"
    Write-Host "  .\env.ps1 info     Print paths"
    Write-Host "  .\env.ps1 -ForceSubst  Map SDK root to R: before setting environment"
}

if ($Command -eq "help") {
    Show-Help
    return
}

$UseSubst = $ForceSubst -or (Test-PathNeedsSubst $ScriptSdkRoot)
$AsciiSdkRoot = Join-Path $env:USERPROFILE "arcs-sdk-win"
$SdkRoot = $ScriptSdkRoot
if ($UseSubst) {
    if (-not (Test-Path $SubstRoot)) {
        cmd /c subst $SubstDrive "$ScriptSdkRoot" | Out-Null
    }

    if (Test-Path (Join-Path $SubstRoot "cmake\listenai-cmake-config.cmake")) {
        $SdkRoot = $SubstRoot
        Write-EnvInfo "SDK path mapped to $SubstRoot"
    } elseif ((Test-Path (Join-Path $AsciiSdkRoot "cmake\listenai-cmake-config.cmake")) -and
        ((Get-Item $AsciiSdkRoot).LinkType -eq "Junction")) {
        $SdkRoot = $AsciiSdkRoot
        Write-EnvInfo "SDK path uses junction: $AsciiSdkRoot"
    } else {
        Write-EnvWarn "SDK path mapping failed; using original path: $ScriptSdkRoot"
    }
}

$ListenAiHome = if ($env:LISTENAI_HOME) {
    $env:LISTENAI_HOME
} else {
    Join-Path $env:USERPROFILE ".listenai"
}
$ToolsPath = Join-Path $ListenAiHome "listenai-tools"
$ToolchainPath = Join-Path $ListenAiHome "gcc"

$env:ARCS_BASE = Convert-ToCMakePath $SdkRoot
$env:LISTENAI_TOOLS_PATH = Convert-ToCMakePath $ToolsPath
$env:NUCLEI_TOOLCHAIN_PATH = Convert-ToCMakePath $ToolchainPath

if ($Command -eq "setup") {
    Install-Toolchain $ListenAiHome $true
    Install-ListenAiTools $ListenAiHome $true
} elseif ($Command -ne "check" -and $Command -ne "info") {
    Install-Toolchain $ListenAiHome $false
    Install-ListenAiTools $ListenAiHome $false
}

Add-ToPath (Join-Path $ToolchainPath "bin")
Add-ToPath (Join-Path $ToolsPath "cmake\bin")
Add-ToPath (Join-Path $ToolsPath "ninja")
Add-ToPath (Join-Path $ToolsPath "kconfig")
Add-ToPath (Join-Path $ToolsPath "menuconfig")

if ($Command -eq "check") {
    if (Test-Toolchain $ToolchainPath) {
        Write-EnvOk "GCC toolchain ready"
    } else {
        Write-EnvWarn "GCC toolchain not found: $ToolchainPath"
    }

    if (Test-ListenAiTools $ToolsPath) {
        Write-EnvOk "listenai-tools ready"
    } else {
        Write-EnvWarn "listenai-tools not found: $ToolsPath"
    }
}

if (($Command -eq "info") -or ($Command -eq "check") -or ($Command -eq "")) {
    Write-Host "ARCS_BASE=$env:ARCS_BASE"
    Write-Host "LISTENAI_TOOLS_PATH=$env:LISTENAI_TOOLS_PATH"
    Write-Host "NUCLEI_TOOLCHAIN_PATH=$env:NUCLEI_TOOLCHAIN_PATH"
}
