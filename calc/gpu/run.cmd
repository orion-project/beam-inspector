@echo off
set INC="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.5\include"
set LIB="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.5\lib\x64\OpenCL.lib"
g++ cl%1.cpp -O3 -I%INC% %LIB% -o cl%1.exe && cl%1.exe

