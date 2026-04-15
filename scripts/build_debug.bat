@echo off
powershell -ExecutionPolicy Bypass -File "%~dp0build_debug.ps1" %*
