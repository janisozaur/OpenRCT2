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
set "vsdevcmd_args=-no_logo"

rem List available host compilers
for /d %%D in ("%vspath%\VC\Tools\MSVC\*\bin\Host*") do (
    echo Available compiler: %%~nxD
)

rem Pass host_arch parameter to VsDevCmd.bat based on OPENRCT2_HOST_ARCH_DISPLAY
if "%OPENRCT2_HOST_ARCH_DISPLAY%"=="ARM64" (
    set "vsdevcmd_args=%vsdevcmd_args% -host_arch=arm64"
)

if "%platform%"=="x64" (
    call "%vspath%\Common7\Tools\VsDevCmd.bat" %vsdevcmd_args% -arch=x64
) else if "%platform%"=="win32" (
    call "%vspath%\Common7\Tools\VsDevCmd.bat" %vsdevcmd_args% -arch=x86
) else if "%platform%"=="arm64" (
    call "%vspath%\Common7\Tools\VsDevCmd.bat" %vsdevcmd_args% -arch=arm64
) else (
    call "%vspath%\Common7\Tools\VsDevCmd.bat" %vsdevcmd_args%
)

%*
exit /b %errorlevel%
