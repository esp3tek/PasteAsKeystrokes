@echo off
setlocal

where cl >nul 2>nul
if %errorlevel%==0 (
    rc /nologo /fo resources.res resources.rc
    cl /nologo /O2 /W3 paste_as_keystrokes.c resources.res /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib gdi32.lib /OUT:PasteAsKeystrokes.exe
    if exist paste_as_keystrokes.obj del paste_as_keystrokes.obj
    if exist resources.res del resources.res
    goto :done
)

where gcc >nul 2>nul
if %errorlevel%==0 (
    windres resources.rc -O coff -o resources.o
    gcc paste_as_keystrokes.c resources.o -o PasteAsKeystrokes.exe -mwindows -luser32 -lshell32 -lgdi32 -s -O2
    if exist resources.o del resources.o
    goto :done
)

echo No C compiler found. Install either MSVC (cl.exe) or MinGW-w64 (gcc.exe).
exit /b 1

:done
echo Built PasteAsKeystrokes.exe
endlocal
