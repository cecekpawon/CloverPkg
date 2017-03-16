@echo off

rem # https://github.com/cecekpawon/CloverPkg
rem # 12/20/2016 4:55:03 PM

rem # setup EDK2

set CYGWIN_HOME=c:\cygwin
set NASM_BIN=%CYGWIN_HOME%\bin\
rem set PYTHON_HOME=c:\Python27
rem set PYTHON_PATH=%PYTHON_HOME%
rem set PYTHON_FREEZER_PATH=%PYTHON_PATH%\Scripts

set "CURRENTDIR=%CD%"
for %%i in ("%CURRENTDIR%") do set "CLOVER_BASEPATH=%%~nxi"

pushd .
cd ..
rem git pull
set "WORKSPACE=%CD%"
rem ForceRebuild
call %WORKSPACE%\edksetup.bat
popd

rem # local vars

rem GCC49 VS2015x86
set MYTOOLCHAIN=VS2015x86
rem set "BUILD_OPTIONS=-D EMBED_APTIOFIX -D EMBED_FSINJECT"
set BUILD_OPTIONS=

set "CLOVER_PATH=%WORKSPACE%\%CLOVER_BASEPATH%"
set "CONF_PATH=%CLOVER_PATH%\Conf"
set "CLOVER_BUILD_PATH=%WORKSPACE%\Build\Clover\RELEASE_%MYTOOLCHAIN%"
set "F_VER_H=%CLOVER_PATH%\Version.h"
set "F_REV_TXT=rev.txt"
set "CLOVER_VERSION=2.3k"
set "CLOVER_REVISION_SUFFIX="
set "CLOVER_DSC=%CLOVER_PATH%\%CLOVER_BASEPATH%.dsc"
set "CLOVER_LOG=%CLOVER_PATH%\%CLOVER_BASEPATH%.log"
set "QEMU_EFI_PATH=G:\QVM\DISK\EFI"
set DRV_LIST=(FSInject OsxAptioFixDrv)
set EDK2_REVISION_MAGIC=8925

rem # get revision

cd %WORKSPACE%

if exist "%F_REV_TXT%" goto SetEDK2Revision

for /f %%i in ('git ls-remote --get-url') do set EDK2_REMOTE=%%i
for /f "tokens=2" %%i in ('svn info %EDK2_REMOTE%^|find "Revision"') do set EDK2_REVISION=%%i
set /a "EDK2_REVISION=%EDK2_REVISION%-%EDK2_REVISION_MAGIC%"
echo %EDK2_REVISION%>%F_REV_TXT%

:SetEDK2Revision

set /P EDK2_REVISION=<%F_REV_TXT%

cd %CURRENTDIR%

git rev-list --count HEAD>%F_REV_TXT%
set /P CLOVER_REVISION=<%F_REV_TXT%

rem # setup command

set BUILDDATE=%date:~10,4%-%date:~4,2%-%date:~7,2% %time:~0,-3%
set "CLOVER_BUILD_CMD=build -s -p __CLOVER_DSC__ %BUILD_OPTIONS% -a X64 -t %MYTOOLCHAIN% -b RELEASE -n %NUMBER_OF_PROCESSORS% -j __CLOVER_LOG__"
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

echo #define CLOVER_BUILDDATE "%BUILDDATE%">%F_VER_H%
echo #define EDK2_REVISION "%EDK2_REVISION%">>%F_VER_H%
echo #define CLOVER_VERSION "%CLOVER_VERSION%">>%F_VER_H%
echo #define CLOVER_REVISION "%CLOVER_REVISION%%CLOVER_REVISION_SUFFIX%">>%F_VER_H%
echo #define CLOVER_BUILDINFOS_STR "%CLOVER_BUILDINFOS_STR%">>%F_VER_H%

rem # clean build

FOR /D %%p IN ("%CLOVER_BUILD_PATH%\*.*") DO rmdir "%%p" /s /q

rem # exec command

%CLOVER_BUILD_CMD%

rem # copy binary

if exist "%QEMU_EFI_PATH%" (
  for %%i in %DRV_LIST% do (
    copy /B /Y "%CLOVER_BUILD_PATH%\X64\%%i.efi" "%QEMU_EFI_PATH%\CLOVER\drivers">nul
  )
  copy /B /Y "%CLOVER_BUILD_PATH%\X64\Clover.efi" "%QEMU_EFI_PATH%\BOOT\BOOTX64.efi">nul
)

pause
