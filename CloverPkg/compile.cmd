@echo off

rem # https://github.com/cecekpawon/Clover
rem # 12/20/2016 4:55:03 PM

rem # setup EDK2

set CYGWIN_HOME=c:\cygwin
set NASM_PREFIX=%CYGWIN_HOME%\bin\

set "CURRENTDIR=%CD%"
for %%i in ("%CURRENTDIR%") do set "CLOVER_BASEPATH=%%~nxi"

pushd .
cd ..
rem svn update
set "WORKSPACE=%CD%"
call %WORKSPACE%\edksetup.bat
popd

rem # local vars

rem set MYTOOLCHAIN=GCC49
set MYTOOLCHAIN=VS2015x86
set "DEFINED_OPT=-D EMBED_APTIOFIX -D EMBED_FSINJECT"
rem set DEFINED_OPT=

set "CLOVER_PATH=%WORKSPACE%\%CLOVER_BASEPATH%"
set "CONF_PATH=%CLOVER_PATH%\Conf"
set "CLOVER_BUILD_PATH=%WORKSPACE%\Build\Clover\RELEASE_%MYTOOLCHAIN%"
set "F_VER_H=%CLOVER_PATH%\Version.h"
set "F_VER_TXT=rev.txt"
set "CLOVER_VERSION=2.3k"
set "CLOVER_REVISION_SUFFIX=X64"
set "CLOVER_DSC=%CLOVER_PATH%\%CLOVER_BASEPATH%.dsc"
set "CLOVER_LOG=%CLOVER_PATH%\%CLOVER_BASEPATH%.log"
set "QEMU_EFI_PATH=G:\QVM\DISK\EFI"

rem # get revision

cd %WORKSPACE%
for /f "tokens=2" %%i in ('svn info^|find "Revision"') do @echo %%i>%F_VER_TXT%
set /P EDK2_REVISION=<%F_VER_TXT%

cd %CURRENTDIR%
for /f "tokens=2" %%i in ('svn info^|find "Revision"') do @echo %%i>%F_VER_TXT%
set /P CLOVER_REVISION=<%F_VER_TXT%

rem # setup command

set BUILDDATE=%date:~10,4%-%date:~4,2%-%date:~7,2% %time:~0,-3%
set "CLOVER_BUILD_CMD=build -s -p __CLOVER_DSC__ %DEFINED_OPT% -a X64 -t %MYTOOLCHAIN% -b RELEASE -n %NUMBER_OF_PROCESSORS% -j __CLOVER_LOG__"
set CLOVER_BUILDINFOS_STR=%CLOVER_BUILD_CMD%

rem # setup path

FOR %%i IN ("%CLOVER_DSC%") DO set CLOVER_DSC_FN=%%~ni%%~xi
FOR %%i IN ("%CLOVER_LOG%") DO set CLOVER_LOG_FN=%%~ni%%~xi

call set CLOVER_BUILDINFOS_STR=%%CLOVER_BUILD_CMD:__CLOVER_DSC__=%CLOVER_DSC_FN%%%
call set CLOVER_BUILDINFOS_STR=%%CLOVER_BUILDINFOS_STR:__CLOVER_LOG__=%CLOVER_LOG_FN%%%

call set CLOVER_BUILD_CMD=%%CLOVER_BUILD_CMD:__CLOVER_DSC__="%CLOVER_DSC%"%%
call set CLOVER_BUILD_CMD=%%CLOVER_BUILD_CMD:__CLOVER_LOG__="%CLOVER_LOG%"%%

rem # escape unwanted char if any

set CLOVER_BUILDINFOS_STR=%CLOVER_BUILDINFOS_STR:\=\\%
set CLOVER_BUILDINFOS_STR=%CLOVER_BUILDINFOS_STR:"=\"%

rem # generate Version.h

echo #define EDK2_REVISION "%EDK2_REVISION%">%F_VER_H%
echo #define CLOVER_BUILDDATE "%BUILDDATE%">>%F_VER_H%
echo #define CLOVER_VERSION "%CLOVER_VERSION%">>%F_VER_H%
echo #define CLOVER_REVISION "%CLOVER_REVISION%%CLOVER_REVISION_SUFFIX%">>%F_VER_H%
echo #define CLOVER_BUILDINFOS_STR "%CLOVER_BUILDINFOS_STR%">>%F_VER_H%

rem # clean build

FOR /D %%p IN ("%CLOVER_BUILD_PATH%\*.*") DO rmdir "%%p" /s /q

rem # exec command

%CLOVER_BUILD_CMD%

rem # copy binary

if exist "%QEMU_EFI_PATH%" (
  copy /B /Y "%CLOVER_BUILD_PATH%\X64\CLOVERX64.efi" "%QEMU_EFI_PATH%\BOOT\BOOTX64.efi">nul
  copy /B /Y "%CLOVER_BUILD_PATH%\X64\FSInject.efi" "%QEMU_EFI_PATH%\CLOVER\drivers">nul
  copy /B /Y "%CLOVER_BUILD_PATH%\X64\OsxAptioFixDrv.efi" "%QEMU_EFI_PATH%\CLOVER\drivers">nul
)

pause