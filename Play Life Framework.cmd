@echo off
rem Double-click to play: hosts the playtest server from origin/main and opens the game.
rem Stop with: tools\cli playtest stop (or run this file with: stop)
cd /d "%~dp0"
node tools\cli.mjs playtest %*
if errorlevel 1 pause
