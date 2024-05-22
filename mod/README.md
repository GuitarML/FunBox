# libDaisy Modifications

Replace these files in libDaisy before building libDaisy. This is required to correctly map the controls for Funbox hardware.
Applicable for Funbox v1, v2, and v3 PCBs.
<br><br>
INSTRUCTIONS:
```
# Replace libDaisy/src/daisy_pedal.h with Funbox/mod/daisy_pedal.h
# Replace libDaisy/src/daisy_pedal.cpp with Funbox/mod/daisy_pedal.cpp

make -C libDaisy
```
