@echo off
rem Life Framework example server: copies config.json into the profile, then starts the server.
rem Set LF_REFORGER_SERVER to the folder that holds ArmaReforgerServer.exe if it is not in the default Steam spot.
rem Extra arguments are passed to the server, e.g.  start.cmd -loadSessionSave
setlocal
set "HERE=%~dp0"
if "%LF_REFORGER_SERVER%"=="" set "LF_REFORGER_SERVER=%ProgramFiles(x86)%\Steam\steamapps\common\Arma Reforger Server"
if not exist "%LF_REFORGER_SERVER%\ArmaReforgerServer.exe" (
  echo ArmaReforgerServer.exe not found in "%LF_REFORGER_SERVER%". Set LF_REFORGER_SERVER to your server folder.
  exit /b 1
)
if not exist "%HERE%profile\profile\LifeFramework" mkdir "%HERE%profile\profile\LifeFramework"
copy /Y "%HERE%config.json" "%HERE%profile\profile\LifeFramework\config.json" >nul
"%LF_REFORGER_SERVER%\ArmaReforgerServer.exe" -config "%HERE%server.json" -profile "%HERE%profile" -maxFPS 60 %*
