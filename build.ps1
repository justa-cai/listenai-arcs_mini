# Windows 原生构建脚本，行为与 build.sh 对齐
# 用法: .\build.ps1 -S .\apps\arcs-mini -DBOARD=arcs_mini

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$ProjectPath = $ScriptDir
$Output = "build"
$Target = ""
$Clean = $false
$WarningsAsErrors = $false
$Release = $false
$VerboseBuild = $false
$CMakeVars = New-Object System.Collections.Generic.List[string]

$ArcsBaseDirName = "arcs-sdk"
$DevToolsDirName = "listenai-dev-tools"
$DevToolToolchainDirName = "gcc"
$DevToolListenaiToolsDirName = "listenai-tools"

function Show-Usage {
    Write-Host "使用方式: build.ps1 [选项]"
    Write-Host "选项:"
    Write-Host "  -S, --Source <path>    指定项目源码路径 (默认为当前脚本所在目录)"
    Write-Host "  -t, --target <target>  指定构建目标 (如 menuconfig)"
    Write-Host "  -C, --Clean            清理构建目录"
    Write-Host "  -B, --build <path>     构建输出目录"
    Write-Host "  -h, --help             显示此帮助信息"
    Write-Host "  -r, --release          以 Release 模式构建 (移除 DEBUG_PATH 信息)"
    Write-Host "  -w, --warnings-as-errors 将警告视为错误"
    Write-Host "  -v, --verbose          显示详细的编译命令 (ninja -v)"
    Write-Host "  -D<var>=<value>        传递 CMake 变量 (可多次使用)"
    Write-Host ""
    Write-Host "示例:"
    Write-Host "  .\build.ps1 -S .\apps\arcs-mini -DBOARD=arcs_mini"
    Write-Host "  .\build.ps1 -C -S .\apps\arcs-mini -DBOARD=arcs_mini"
    Write-Host "  .\build.ps1 -S .\apps\arcs-mini -t menuconfig -DBOARD=arcs_mini"
    exit 1
}

function Get-RequiredArg {
    param([string]$Name, [ref]$Index, [string[]]$RawArgs)
    if (($Index.Value + 1) -ge $RawArgs.Count) {
        throw "$Name 需要一个参数值"
    }
    $Index.Value++
    return [string]$RawArgs[$Index.Value]
}

function Convert-ToCMakePath($Path) {
    return ([System.IO.Path]::GetFullPath($Path)).Replace("\", "/")
}

# 向上查找包含 arcs-sdk 的目录，设置 ARCS_BASE
function Find-ArcsBase {
    $dir = $ScriptDir
    while ($dir) {
        $candidate = Join-Path $dir $ArcsBaseDirName
        if (Test-Path $candidate -PathType Container) {
            $env:ARCS_BASE = Convert-ToCMakePath $candidate
            Write-Host "Found ARCS_BASE: $($env:ARCS_BASE)"
            return
        }
        $parent = Split-Path $dir -Parent
        if ($parent -eq $dir) { break }
        $dir = $parent
    }
    throw "未找到 ARCS_BASE（包含 $ArcsBaseDirName 的目录），请设置 ARCS_BASE 环境变量"
}

# 解析 LISTENAI_TOOLS_PATH / NUCLEI_TOOLCHAIN_PATH：优先环境变量，其次 %USERPROFILE%\.listenai，再向上找 listenai-dev-tools
function Find-DevTools {
    $roots = New-Object System.Collections.Generic.List[string]
    if ($env:USERPROFILE) { $roots.Add((Join-Path $env:USERPROFILE ".listenai")) }
    $dir = $ScriptDir
    while ($dir) {
        $roots.Add((Join-Path $dir $DevToolsDirName))
        $parent = Split-Path $dir -Parent
        if ($parent -eq $dir) { break }
        $dir = $parent
    }
    foreach ($root in $roots) {
        if (-not (Test-Path $root -PathType Container)) { continue }
        $tools = Join-Path $root $DevToolListenaiToolsDirName
        $gcc = Join-Path $root $DevToolToolchainDirName
        if ((-not $env:LISTENAI_TOOLS_PATH) -and (Test-Path $tools -PathType Container)) {
            $env:LISTENAI_TOOLS_PATH = $tools
            Write-Host "Found LISTENAI_TOOLS_PATH: $tools"
        }
        if ((-not $env:NUCLEI_TOOLCHAIN_PATH) -and (Test-Path $gcc -PathType Container)) {
            $env:NUCLEI_TOOLCHAIN_PATH = $gcc
            Write-Host "Found NUCLEI_TOOLCHAIN_PATH: $gcc"
        }
    }
}

$RawArgs = [string[]]$args
for ($i = 0; $i -lt $RawArgs.Count; $i++) {
    $arg = [string]$RawArgs[$i]
    $indexRef = [ref]$i
    if (($arg -ceq "-S") -or ($arg -ceq "--Source") -or ($arg -ceq "-Source")) {
        $ProjectPath = Get-RequiredArg $arg $indexRef $RawArgs; $i = $indexRef.Value
    } elseif (($arg -ceq "-B") -or ($arg -ceq "--build") -or ($arg -ceq "-Build")) {
        $Output = Get-RequiredArg $arg $indexRef $RawArgs; $i = $indexRef.Value
    } elseif (($arg -ceq "-t") -or ($arg -ceq "--target") -or ($arg -ceq "-Target")) {
        $Target = Get-RequiredArg $arg $indexRef $RawArgs; $i = $indexRef.Value
    } elseif (($arg -ceq "-C") -or ($arg -ceq "--Clean") -or ($arg -ceq "-Clean")) {
        $Clean = $true
    } elseif (($arg -ceq "-w") -or ($arg -ceq "--warnings-as-errors") -or ($arg -ceq "-WarningsAsErrors")) {
        $WarningsAsErrors = $true
    } elseif (($arg -ceq "-v") -or ($arg -ceq "--verbose") -or ($arg -ceq "-Verbose")) {
        $VerboseBuild = $true
    } elseif (($arg -ceq "-r") -or ($arg -ceq "--release") -or ($arg -ceq "-Release")) {
        $Release = $true
    } elseif (($arg -ceq "-h") -or ($arg -ceq "--help") -or ($arg -ceq "-Help") -or ($arg -ceq "/?")) {
        Show-Usage
    } elseif (($arg -ceq "-D") -or ($arg -ceq "--define")) {
        $CMakeVars.Add("-D$(Get-RequiredArg $arg $indexRef $RawArgs)"); $i = $indexRef.Value
    } elseif (($arg.Length -gt 2) -and $arg.StartsWith("-D", [System.StringComparison]::Ordinal)) {
        $CMakeVars.Add($arg)
    } else {
        Write-Host "未知参数: $arg"
        Show-Usage
    }
}

if (-not [System.IO.Path]::IsPathRooted($ProjectPath)) {
    $ProjectPath = Join-Path (Get-Location) $ProjectPath
}
$ProjectPath = Convert-ToCMakePath $ProjectPath

Write-Host "Source: $ProjectPath"
Write-Host "Target: $Target"
Write-Host "Clean : $Clean"

if ((-not $env:LISTENAI_TOOLS_PATH) -or (-not $env:NUCLEI_TOOLCHAIN_PATH)) {
    Find-DevTools
}
if (-not $env:LISTENAI_TOOLS_PATH) {
    throw "请设置 LISTENAI_TOOLS_PATH 环境变量（指向 Windows 版 listenai-tools）"
}
if (-not $env:NUCLEI_TOOLCHAIN_PATH) {
    throw "请设置 NUCLEI_TOOLCHAIN_PATH 环境变量（指向 RISC-V 工具链）"
}

$CMake = Join-Path $env:LISTENAI_TOOLS_PATH "cmake\bin\cmake.exe"
$Ninja = Join-Path $env:LISTENAI_TOOLS_PATH "ninja\ninja.exe"

# 并发任务数
$Jobs = if ($env:NUMBER_OF_PROCESSORS) { [int]$env:NUMBER_OF_PROCESSORS } else { 4 }

if (-not $env:ARCS_BASE) { Find-ArcsBase }

if ($Clean -and (Test-Path $Output)) {
    Remove-Item -LiteralPath $Output -Recurse -Force
}

if ($WarningsAsErrors) {
    $CMakeVars.Add("-DCMAKE_C_FLAGS=-Werror")
    $CMakeVars.Add("-DCMAKE_CXX_FLAGS=-Werror")
    Write-Host "Treating warnings as errors"
}
if ($Release) {
    $CMakeVars.Add("-DENABLE_DEBUG_PATH=OFF")
    Write-Host "Release mode enabled (-DENABLE_DEBUG_PATH=OFF)"
}

$CMakeVars.Add("-DBOARD_SEARCH_PATH=$(Convert-ToCMakePath (Join-Path $ScriptDir 'boards'))")

& $CMake -B $Output -G Ninja -S $ProjectPath `
    -D CMAKE_MAKE_PROGRAM="$Ninja" `
    @CMakeVars
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$buildArgs = @("--build", $Output, "-j$Jobs")
if ($Target) {
    $buildArgs += @("--target", $Target)
}
if ($VerboseBuild) {
    Write-Host "Verbose mode enabled (ninja -v)"
    $buildArgs += @("--", "-v")
}

& $CMake @buildArgs
exit $LASTEXITCODE
