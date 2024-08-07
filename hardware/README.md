# Hardware

This directory contains the KiCad files for manufacturing the Funbox PCB. Includes schematic, pcb, and generated BOM.
Funbox uses a 125B sized enclosure.

## Funbox v1

VERIFIED WORKING - using OshPark standard board manufacturing

Funbox v1 uses a 125B enclosure with the following features:
- Stereo In/Out
- 9v power
- 6 potentiometer knobs 
- 3 3-way switches On-Off-On
- 2 SPST momentary footswitches
- 2 LEDs
- 2 Dipswitches

[Tayda Drill Template](https://drill.taydakits.com/box-designs/new?public_key=bXVVdnZXK0pOVWJjZG5YOGtwd282Zz09Cg==)

![app](https://github.com/GuitarML/Funbox/blob/main/hardware/images/funbox_v1_front.jpg)
![app](https://github.com/GuitarML/Funbox/blob/main/hardware/images/funbox_v1_inside.jpg)
![app](https://github.com/GuitarML/Funbox/blob/main/hardware/images/funbox_v1_pcb.jpg)


## Funbox v2

Funbox v2 adds MIDI In/Out via two 1/8" TRS jacks and up to a 4 dip switch array, instead of 2.

[Tayda Drill Template](https://drill.taydakits.com/box-designs/new?public_key=M3BDZTdRSmFWRkF4QXpQUzRORjIwQT09Cg==)

CURRENTLY UNTESTED 

## Funbox v3.2

Funbox v3 replaces the MIDI out jack from version 2 with an Expression Input jack. 
Version 3.2 replaces the TL072 Quad Op Amp with a MCP6024 for reduced noise.
Note: Since most Expresion pedals use 1/4" TRS cables, an adapter is required (Female 1/4" to Male 1/8" stereo adapter, or 1/4" to 1/8" TRS cable.)

[Tayda Drill Template](https://drill.taydakits.com/box-designs/new?public_key=M3BDZTdRSmFWRkF4QXpQUzRORjIwQT09Cg==) (Same as version 2)

The Gerber files used for ordering from PCBway have been added in GerberSeed_PCBway_Funbox_v3p2.zip (note that other manufacturers may have different Gerber requirements)

The Funbox v3.2 board is a shared project on OSHPark, click [here](https://oshpark.com/shared_projects/MnthZS4N) to go to the project page for easy ordering.

VERIFIED WORKING

Funbox v3 uses a 125B enclosure with the same features a version 1 with the following additions:
- Midi Input from a 1/8 inch TRS jack
- Expression Input from a 1/8 inch TRS jack
- 4 Dipswitches

![app](https://github.com/GuitarML/Funbox/blob/main/hardware/images/funbox_v3_front.jpg)
![app](https://github.com/GuitarML/Funbox/blob/main/hardware/images/funbox_v3_inside.jpg)