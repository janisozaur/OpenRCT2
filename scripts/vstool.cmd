@echo off

rem Invokes a tool within a Visual Studio prompt
rem Uses %PLATFORM% to set architecture of prompt

set "vspath="

rem Try to use vswhere to find the latest Visual Studio
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere%" set "vswhere=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%vswhere%" (
    for /f "usebackq tokens=*" %%i in (`"%vswhere%" -latest -prerelease -property installationPath`) do (
        set "vspath=%%i"
    )
)

if not defined vspath (
    for /f "usebackq tokens=*" %%i in (`vswhere -latest -prerelease -property installationPath 2^>nul`) do (
        set "vspath=%%i"
    )
)

if defined vspath if exist "%vspath%" goto found

rem Fallback to searching common paths
if exist "%ProgramFiles%\Microsoft Visual Studio\2026\Enterprise" (
    set "vspath=%ProgramFiles%\Microsoft Visual Studio\2026\Enterprise"
    goto found
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2026\Community" (
    set "vspath=%ProgramFiles%\Microsoft Visual Studio\2026\Community"
    goto found
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2026\Professional" (
    set "vspath=%ProgramFiles%\Microsoft Visual Studio\2026\Professional"
    goto found
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2026\Preview" (
    set "vspath=%ProgramFiles%\Microsoft Visual Studio\2026\Preview"
    goto found
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise" (
    set "vspath=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
    goto found
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community" (
    set "vspath=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
    goto found
)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise" (
    set "vspath=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise"
    goto found
)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community" (
    set "vspath=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community"
    goto found
)

echo Visual Studio directory not found
exit /b 1

:found
set "vsarch=%platform%"
if "%vsarch%"=="win32" set "vsarch=x86"

if not "%vsarch%"=="" (
    call "%vspath%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=%vsarch%
) else (
    call "%vspath%\Common7\Tools\VsDevCmd.bat" -no_logo
)

%*
exit /b %errorlevel%
