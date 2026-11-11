@echo off
rem Life Framework playtest launcher. Double-click this file to play.
rem It opens a small window (launcher\play.ps1) that finds Arma Reforger
rem through Steam and starts it with the playtest town loaded.
start "" powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -STA -File "%~dp0launcher\play.ps1" %*
