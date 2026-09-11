@echo off
call "E:\VisualStudio\VC\Auxiliary\Build\vcvarsall.bat" x64

set WV2_INC=C:\Users\Administrator\Desktop\qqshow2000-cpp\webview2\build\native\include
set WV2_LIB=C:\Users\Administrator\Desktop\qqshow2000-cpp\webview2\build\native\x64

cl.exe /O2 /MT /EHsc /I"%WV2_INC%" main.cpp /Fe:QQShow2000.exe /link user32.lib ole32.lib oleaut32.lib "%WV2_LIB%\WebView2Loader.dll.lib" /SUBSYSTEM:WINDOWS

copy "%WV2_LIB%\WebView2Loader.dll" . /Y

echo Build done.
pause
