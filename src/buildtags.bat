@echo off
ctags -R --kinds-C=+l --c++-kinds=+p --fields=+iaS --extras=+q --language-force=C++ .

REM NOTE:
REM It's important to pass '--kinds-C=+l' for variable definition be detected.
