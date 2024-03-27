# Calculations

This directory contains different calculation scripts and sample programs intended for testing numerical algorithms and performance comparison for choosing an optimal option.

## Python

It's preferable to use virtual environment for running Python scripts in order not to overload the global scope with unnecessary and probably conflicting dependencies.

Linux, macOS:

```bash
python3 -m venv .venv
source .venv/bin/activate
```

Windows:

```bash
python -m venv .venv
.venv\Scripts\activate
```

There is no `python3` command on Windows so be sure Python 3 is in the `PATH`

Install requirements:

```bash
pip install -r requirements.txt
```

Run Jupyter (port is optional, default is 8888):

```bash
jupyter notebook --port 9999
```

## C/C++

It's supposed that GCC is used for code compilation. On Windows be sure you have added MinGW bin directory to the `PATH` variable, e.g.:

```bash
set PATH=c:\Qt\Tools\mingw810_32\bin;%PATH%
```

[OpenBLAS](https://github.com/OpenMathLib/OpenBLAS) must be downloaded and unpacked into `$RPOJECT_ROOT/libs/openblas`. Be sure you have a version ([x32](https://github.com/OpenMathLib/OpenBLAS/releases/download/v0.3.26/OpenBLAS-0.3.26-x86.zip) or [x64](https://github.com/OpenMathLib/OpenBLAS/releases/download/v0.3.26/OpenBLAS-0.3.26-x64.zip)) that fits the bitness of used GCC.
