$Source = "samples/helloworld"
$Build = "build"
$Board = "arcs_mini"
$Jobs = 4
$Target = ""
$Config = "prj.conf"
$Clean = $false
$WarningsAsErrors = $false
$Release = $false
$VerboseBuild = $false
$DebugBuild = $false
$Generator = "Ninja"
$CMakeVars = New-Object System.Collections.Generic.List[string]

$ErrorActionPreference = "Stop"

function Show-Usage {
    Write-Host "Usage: .\build.ps1 [options]"
    Write-Host "Options:"
    Write-Host "  -S, --Source <path>       Project source path (default: samples/helloworld)"
    Write-Host "  -t, --target <target>     Build target (for example: menuconfig)"
    Write-Host "  -C, --Clean               Clean build directory"
    Write-Host "  -B, --build <path>        Build output directory (default: build)"
    Write-Host "  -j<N>, --jobs <N>         Parallel build jobs (default: 4)"
    Write-Host "  -h, --help                Show this help"
    Write-Host "  -r, --release             Release mode (remove DEBUG_PATH info)"
    Write-Host "  -w, --warnings-as-errors  Treat warnings as errors"
    Write-Host "  -c, --config <file>       Config file (default: prj.conf)"
    Write-Host "  -v, --verbose             Show verbose build commands"
    Write-Host "  -d, --debug               Enable Ninja debug explain output"
    Write-Host "  -G, --generator <type>    Build generator (Ninja or Makefile, default: Ninja)"
    Write-Host "  -D<var>=<value>           Pass a CMake variable (repeatable)"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\build.ps1 -S samples/helloworld -DBOARD=arcs_mini"
    Write-Host "  .\build.ps1 -C -S samples/helloworld -DBOARD=arcs_mini"
    Write-Host "  .\build.ps1 -S samples/helloworld -j1"
    Write-Host "  .\build.ps1 -S samples/helloworld -t menuconfig -DBOARD=arcs_evb"
}

function Get-RequiredArg {
    param(
        [string]$Name,
        [ref]$Index,
        [string[]]$RawArgs
    )

    if (($Index.Value + 1) -ge $RawArgs.Count) {
        throw "$Name requires a value"
    }

    $Index.Value++
    return [string]$RawArgs[$Index.Value]
}

function Set-Jobs {
    param([string]$Value)

    $parsed = 0
    if (-not [int]::TryParse($Value, [ref]$parsed) -or $parsed -le 0) {
        throw "-j/--jobs must be a positive integer, got: $Value"
    }

    $script:Jobs = $parsed
}

function Convert-ToCMakePath($Path) {
    return ([System.IO.Path]::GetFullPath($Path)).Replace("\", "/")
}

function Normalize-FullPath($Path) {
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $root = [System.IO.Path]::GetPathRoot($fullPath)
    $trimChars = [char[]]@([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $trimmedPath = $fullPath.TrimEnd($trimChars)

    if ($trimmedPath.Length -lt $root.Length) {
        return $fullPath
    }

    return $trimmedPath
}

function Add-TrailingDirectorySeparator($Path) {
    $directorySeparator = [string][System.IO.Path]::DirectorySeparatorChar
    $altDirectorySeparator = [string][System.IO.Path]::AltDirectorySeparatorChar

    if ($Path.EndsWith($directorySeparator, [System.StringComparison]::Ordinal) -or
        $Path.EndsWith($altDirectorySeparator, [System.StringComparison]::Ordinal)) {
        return $Path
    }

    return "$Path$directorySeparator"
}

function Test-IsSubPath($Path, $Root) {
    $resolvedPath = Normalize-FullPath $Path
    $resolvedRoot = Normalize-FullPath $Root

    if ($resolvedPath.Length -le $resolvedRoot.Length) {
        return $false
    }

    $rootPrefix = Add-TrailingDirectorySeparator $resolvedRoot
    return $resolvedPath.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)
}

function Resolve-ConfigPath($SourcePath, $ConfigFile) {
    if ([System.IO.Path]::IsPathRooted($ConfigFile)) {
        return Convert-ToCMakePath $ConfigFile
    }

    return Convert-ToCMakePath (Join-Path $SourcePath $ConfigFile)
}

$RawArgs = [string[]]$args
for ($i = 0; $i -lt $RawArgs.Count; $i++) {
    $arg = [string]$RawArgs[$i]
    $indexRef = [ref]$i

    if (($arg -ceq "-S") -or ($arg -ceq "--Source") -or ($arg -ceq "-Source")) {
        $Source = Get-RequiredArg $arg $indexRef $RawArgs
        $i = $indexRef.Value
    } elseif (($arg -ceq "-B") -or ($arg -ceq "--build") -or ($arg -ceq "-Build")) {
        $Build = Get-RequiredArg $arg $indexRef $RawArgs
        $i = $indexRef.Value
    } elseif (($arg -ceq "-t") -or ($arg -ceq "--target") -or ($arg -ceq "-Target")) {
        $Target = Get-RequiredArg $arg $indexRef $RawArgs
        $i = $indexRef.Value
    } elseif (($arg -ceq "-j") -or ($arg -ceq "--jobs") -or ($arg -ceq "-Jobs")) {
        Set-Jobs (Get-RequiredArg $arg $indexRef $RawArgs)
        $i = $indexRef.Value
    } elseif (($arg.Length -gt 2) -and $arg.StartsWith("-j", [System.StringComparison]::Ordinal)) {
        Set-Jobs $arg.Substring(2)
    } elseif (($arg -ceq "-C") -or ($arg -ceq "--Clean") -or ($arg -ceq "-Clean")) {
        $Clean = $true
    } elseif (($arg -ceq "-w") -or ($arg -ceq "--warnings-as-errors") -or ($arg -ceq "-WarningsAsErrors")) {
        $WarningsAsErrors = $true
    } elseif (($arg -ceq "-v") -or ($arg -ceq "--verbose") -or ($arg -ceq "-Verbose")) {
        $VerboseBuild = $true
    } elseif (($arg -ceq "-d") -or ($arg -ceq "--debug") -or ($arg -ceq "-Debug")) {
        $DebugBuild = $true
        $VerboseBuild = $true
    } elseif (($arg -ceq "-G") -or ($arg -ceq "--generator") -or ($arg -ceq "-Generator")) {
        $Generator = Get-RequiredArg $arg $indexRef $RawArgs
        $i = $indexRef.Value
        if (($Generator -ceq "ninja") -or ($Generator -ceq "Ninja")) {
            $Generator = "Ninja"
        } elseif (($Generator -ceq "makefile") -or ($Generator -ceq "Makefile") -or ($Generator -ceq "Unix Makefiles")) {
            $Generator = "Makefile"
        } else {
            throw "Unsupported generator: $Generator (supported: Ninja, Makefile)"
        }
    } elseif (($arg -ceq "-h") -or ($arg -ceq "--help") -or ($arg -ceq "-Help") -or ($arg -ceq "/?")) {
        Show-Usage
        exit 0
    } elseif (($arg -ceq "-r") -or ($arg -ceq "--release") -or ($arg -ceq "-Release")) {
        $Release = $true
    } elseif (($arg -ceq "-c") -or ($arg -ceq "--config") -or ($arg -ceq "-Config")) {
        $Config = Get-RequiredArg $arg $indexRef $RawArgs
        $i = $indexRef.Value
    } elseif (($arg -ceq "-D") -or ($arg -ceq "--define")) {
        $CMakeVars.Add("-D$(Get-RequiredArg $arg $indexRef $RawArgs)")
        $i = $indexRef.Value
    } elseif (($arg.Length -gt 2) -and $arg.StartsWith("-D", [System.StringComparison]::Ordinal)) {
        $CMakeVars.Add($arg)
    } elseif (($arg -ceq "-Board") -or ($arg -ceq "--Board")) {
        $Board = Get-RequiredArg $arg $indexRef $RawArgs
        $i = $indexRef.Value
    } else {
        throw "Unknown argument: $arg"
    }
}

. "$PSScriptRoot\env.ps1"

$SdkRoot = $env:ARCS_BASE
$SourcePath = if ([System.IO.Path]::IsPathRooted($Source)) {
    Convert-ToCMakePath $Source
} else {
    Convert-ToCMakePath (Join-Path $SdkRoot $Source)
}
$BuildPath = if ([System.IO.Path]::IsPathRooted($Build)) {
    Convert-ToCMakePath $Build
} else {
    Convert-ToCMakePath (Join-Path $SdkRoot $Build)
}
$ConfigPath = Resolve-ConfigPath $SourcePath $Config

if (-not (Test-Path $SourcePath)) {
    throw "Source path not found: $SourcePath"
}
if (-not (Test-Path $ConfigPath)) {
    throw "Config file not found: $ConfigPath"
}

if ($Clean -and (Test-Path $BuildPath)) {
    $resolvedBuild = Normalize-FullPath $BuildPath
    if (-not (Test-IsSubPath $resolvedBuild $SdkRoot)) {
        throw "Refusing to clean path outside SDK root or SDK root itself: $BuildPath"
    }
    Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
}

$CMake = Join-Path $env:LISTENAI_TOOLS_PATH "cmake/bin/cmake.exe"
$Ninja = Join-Path $env:LISTENAI_TOOLS_PATH "ninja/ninja.exe"

if ($WarningsAsErrors) {
    $CMakeVars.Add("-DCMAKE_C_FLAGS=-Werror")
    $CMakeVars.Add("-DCMAKE_CXX_FLAGS=-Werror")
    Write-Host "Treating warnings as errors"
}
if ($Release) {
    $CMakeVars.Add("-DENABLE_DEBUG_PATH=OFF")
    Write-Host "Release mode enabled (-DENABLE_DEBUG_PATH=OFF)"
}

$CMakeVars.Add("-DCONFIG_DEFAULT=$ConfigPath")
if (-not ($CMakeVars | Where-Object { $_ -match '^-DBOARD(:[^=]+)?=' })) {
    $CMakeVars.Add("-DBOARD=$Board")
}

if ($Generator -eq "Makefile") {
    $CMakeGenerator = "Unix Makefiles"
    $BuildProgram = "make"
    Write-Host "Using Makefile generator"
} else {
    $CMakeGenerator = "Ninja"
    $BuildProgram = $Ninja
    Write-Host "Using Ninja generator"
}

if ($CMakeGenerator -eq "Ninja") {
    & $CMake -B $BuildPath -G $CMakeGenerator -S $SourcePath `
        -D CMAKE_MAKE_PROGRAM="$BuildProgram" `
        @CMakeVars
} else {
    & $CMake -B $BuildPath -G $CMakeGenerator -S $SourcePath `
        @CMakeVars
}

$buildArgs = @("--build", $BuildPath, "-j$Jobs")
if ($Target) {
    $buildArgs += @("--target", $Target)
}

$buildToolFlags = @()
if ($CMakeGenerator -eq "Ninja") {
    if ($DebugBuild) {
        $buildToolFlags += @("-d", "explain")
        Write-Host "Debug mode enabled (ninja -d explain)"
    }
    if ($VerboseBuild) {
        $buildToolFlags = @("-v") + $buildToolFlags
        Write-Host "Verbose mode enabled (ninja -v)"
    }
} else {
    if ($DebugBuild) {
        Write-Host "Note: -d/--debug explain is only available with Ninja; ignored for Makefile"
    }
    if ($VerboseBuild) {
        $buildToolFlags += "VERBOSE=1"
        Write-Host "Verbose mode enabled (make VERBOSE=1)"
    }
}

if ($buildToolFlags.Count -gt 0) {
    $buildArgs += "--"
    $buildArgs += $buildToolFlags
}

& $CMake @buildArgs
