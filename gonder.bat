@echo off
cd /d "%~dp0"
git add .
git commit -m "Guncelleme"
git push
pause
