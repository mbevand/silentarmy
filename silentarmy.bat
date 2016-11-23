@echo off
set GPU_FORCE_64BIT_PTR 0
set GPU_MAX_HEAP_SIZE 100
set GPU_USE_SYNC_OBJECTS 1
set GPU_MAX_ALLOC_PERCENT 100
set GPU_SINGLE_ALLOC_PERCENT 100
py silentarmy.py --use=0,1 -c stratum+tcp://zec.suprnova.cc:2142 -u Genoil.SilentArmy -p z
pause