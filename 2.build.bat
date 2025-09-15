set INC=external\nuget\Microsoft.Web.WebView2\build\native\include
set LIB=external\nuget\Microsoft.Web.WebView2\build\native\x64\WebView2LoaderStatic.lib

tools\vcpp.bat /utf-8 /O3 /W4 /DNDEBUG /DUNICODE /DWIN32 LivePageWall.cpp /I %INC% %LIB% ^
    user32.lib gdi32.lib dwmapi.lib advapi32.lib shell32.lib Shlwapi.lib /link /SUBSYSTEM:WINDOWS