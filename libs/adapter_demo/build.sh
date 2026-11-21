gcc adapter_demo.c \
    -shared -Wl,--exclude-all-symbols \
    -fanalyzer -Wall -Wextra -Werror -Wno-unused-parameter \
    -I ../adapter -o adapter_demo.dll