cl /LD ^
    /analyze /Wall /WX ^
    /wd5045 /wd4820 /wd4100 ^
    /external:W1 /external:I ../adapter ^
    adapter_demo.c

: Disabled warinings:
: 5045 - something about Spectre protection has been automatically added
: 4820 - X bytes padding added after data member Y
: 4100 - 'X': unreferenced parameter
