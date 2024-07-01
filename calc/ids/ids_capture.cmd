set INC="C:\Program Files\IDS\ids_peak\comfort_sdk\api\include\ids_peak_comfort_c"
set LIB="C:\Program Files\IDS\ids_peak\comfort_sdk\api\lib\x86_64\ids_peak_comfort_c.lib"
set TGT=..\tmp\ids_comfort
gcc -O3 -ffast-math -funsafe-math-optimizations -msse4.2 -o ids_capture ids_capture.c -I%INC% %LIB% ^
    && copy /Y ids_capture.exe %TGT% ^
    && %TGT%\ids_capture.exe
