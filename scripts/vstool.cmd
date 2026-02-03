@echo off

rem Invokes a tool within a Visual Studio prompt
rem Uses %PLATFORM% to set architecture of prompt

set "vspath=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
if exist "%vspath%" goto found
set "vspath=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
if exist "%vspath%" goto found
set "vspath=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise"
if exist "%vspath%" goto found
set "vspath=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community"
if exist "%vspath%" goto found

:notfound
echo Visual Studio directory not found
exit /b 1

:found
rem Set host architecture environment variable for MSVC if specified
if "%OPENRCT2_HOST_ARCH_DISPLAY%"=="ARM64" (
    set "VCToolsHostArchitecture=ARM64"
) else if "%OPENRCT2_HOST_ARCH_DISPLAY%"=="x64" (
    set "VCToolsHostArchitecture=x64"
) else if "%OPENRCT2_HOST_ARCH_DISPLAY%"=="x86" (
    set "VCToolsHostArchitecture=x86"
)

if "%platform%"=="x64" (
    call "%vspath%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64
) else if "%platform%"=="win32" (
    call "%vspath%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x86
) else (
    call "%vspath%\Common7\Tools\VsDevCmd.bat" -no_logo
)

%*
exit /b %errorlevel%
