#!/bin/bash

# Variable IDS_COMFORT_SDK must point to the `comfort_sdk` directory

gcc adapter_demo_ids.c \
  -shared -Wall -Werror -Wl,--exclude-all-symbols \
  -I ../adapter \
  -I ${IDS_COMFORT_SDK}/api/include \
  -L ${IDS_COMFORT_SDK}/api/lib/x86_64 \
  -lids_peak_comfort_c \
  -o adapter_demo_ids.dll
