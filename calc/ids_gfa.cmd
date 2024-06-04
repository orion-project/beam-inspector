set INC="C:\Program Files\IDS\ids_peak\comfort_sdk\api\include\ids_peak_comfort_c"
set LIB="C:\Program Files\IDS\ids_peak\comfort_sdk\api\lib\x86_64\ids_peak_comfort_c.lib"
set TGT=..\tmp\ids_comfort
gcc -o ids_gfa ids_gfa.c -I%INC% %LIB% ^
    && copy /Y ids_gfa.exe %TGT% ^
    && %TGT%\ids_gfa.exe
