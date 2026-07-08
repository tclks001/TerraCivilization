@echo off
setlocal
set SCRIPT_PATH=%~1
if "%SCRIPT_PATH%"=="" (
  echo Usage: run_commandlet.bat ^<path_to_python_script.py^>
  exit /b 1
)
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%..\..\..\.."
set "CLIENT_ROOT=%CD%"
set "EDITOR_CMD=%CLIENT_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

rem -- 动态定位 .uproject：在工程根目录下搜索第一个 .uproject 文件，不依赖目录名或项目名
for /f "delims=" %%F in ('dir /s /b "%CLIENT_ROOT%\*.uproject" 2^>nul') do (
  set "UPROJECT=%%F"
  goto :found_uproject
)
echo No .uproject file found under %CLIENT_ROOT%
exit /b 1
:found_uproject

if not exist "%EDITOR_CMD%" (
  echo UnrealEditor-Cmd.exe not found at %EDITOR_CMD%
  exit /b 1
)
if not exist "%UPROJECT%" (
  echo .uproject not found at %UPROJECT%
  exit /b 1
)
echo Using project: %UPROJECT%
"%EDITOR_CMD%" "%UPROJECT%" -run=pythonscript -script="%SCRIPT_PATH%" -nullrhi -unattended -noshadercompile -nosound -nosplash -nocrashreports
exit /b %ERRORLEVEL%
