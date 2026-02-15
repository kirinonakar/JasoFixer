@echo off
setlocal enabledelayedexpansion

:: Use user-specified Visual Studio path
set "VS_PATH=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

if "%VS_PATH%"=="" (
    echo [!] Visual Studio vcvars64.bat not found in default locations.
    echo Please run this script from the 'Developer Command Prompt' or 'Developer PowerShell'.
    pause
    exit /b 1
)

echo [+] Found Visual Studio: %VS_PATH%
call "%VS_PATH%"

echo [+] Compiling Resources...
rc JasoFixer.rc

echo [+] Compiling JasoFixer.cpp...
cl /O2 /GL /MT /GS /EHsc /utf-8 JasoFixer.cpp JasoFixer.res /link /LTCG /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA

if %ERRORLEVEL% equ 0 (
    echo [+] Compilation successful! JasoFixer.exe has been created.
    del JasoFixer.obj
) else (
    echo [!] Compilation failed.
)
