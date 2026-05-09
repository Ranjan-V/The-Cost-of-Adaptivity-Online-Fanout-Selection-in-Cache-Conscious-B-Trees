@echo off
setlocal

where pdflatex >nul 2>nul
if errorlevel 1 (
    echo ERROR: pdflatex not found on PATH.
    echo Install TeX Live or MiKTeX, then rerun this script.
    exit /b 1
)

pdflatex -interaction=nonstopmode main.tex
bibtex main
pdflatex -interaction=nonstopmode main.tex
pdflatex -interaction=nonstopmode main.tex

echo.
echo Paper build complete: main.pdf
