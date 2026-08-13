@echo off
REM Build script for VENUSA demos using CMake + Ninja

set TGT=smp_test_baremetal
set CHIP=venusa

REM Configuration variables
if not defined IC_BOARD set IC_BOARD=1
if not defined BUILD_TYPE set BUILD_TYPE=Debug
if not defined NUCLEI_TOOLCHAIN_PATH set NUCLEI_TOOLCHAIN_PATH=C:\nuclei

REM Build directory
set BUILD_DIR=build

REM Check for clean command
if "%1"=="clean" (
    echo ==========================================
    echo Cleaning all generated files...
    echo ==========================================
    
    cmake --build build --target clean_all
    REM Remove all build directories
    if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
    
    echo Clean completed successfully!
    exit /b 0
)

echo ==========================================
echo Building %TGT% with CMake + Ninja
echo ==========================================
echo CHIP: %CHIP%
echo IC_BOARD: %IC_BOARD%
echo BUILD_DIR: %BUILD_DIR%
echo BUILD_TYPE: %BUILD_TYPE%
echo TOOLCHAIN: %NUCLEI_TOOLCHAIN_PATH%
echo ==========================================

REM Create build directory
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
cd %BUILD_DIR%

REM Configure with CMake
cmake -G Ninja ^
    -DCHIP=%CHIP% ^
    -DTGT=%TGT% ^
    -DIC_BOARD=%IC_BOARD% ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DNUCLEI_TOOLCHAIN_PATH=%NUCLEI_TOOLCHAIN_PATH% ^
    -S ../ ^
    -B .

if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

REM Build
echo Building...
ninja -v

if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo ==========================================
echo Build completed successfully!
echo Binaries are located in: out/%CHIP%/
echo Libraries are located in: lib/%CHIP%-%TGT%/
echo ==========================================
