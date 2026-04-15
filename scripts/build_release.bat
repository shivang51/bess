@echo off
powershell -ExecutionPolicy Bypass -File "%~dp0build_release.ps1" %*
