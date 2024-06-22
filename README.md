# Funbox

Funbox is a digital stereo guitar effect platform using the Daisy Seed by Electrosmith.
This repo includes the KiCad design files for the PCB, as well as software packages to configure your
Funbox as a fully featured guitar pedal. You can build your own and use the effects included here,
or use it as a starting place to design your own awesome pedal!

**Work In Progress** Check back for updates!

![app](https://github.com/GuitarML/Funbox/blob/main/hardware/images/funbox_v3_front.jpg)

The resources included here allow you to build a guitar pedal with comparable features to commercial brands. 
Soldering is required to build the hardware. Tayda drill templates are linked along with an Adobe Illustrator 
template for UV printing the enclosure, if desired. 


![app](https://github.com/GuitarML/Funbox/blob/main/software/images/funbox_infographic.jpg)

## Hardware

The Funbox pedal consists of the following main components:

 - Daisy Seed (order from [Electro-Smith](https://www.electro-smith.com/)) - $30
 - Funbox PCB (order from OSHPark or other manufacturer) - $30 for minimum of 3 from OshPark
 - 125B Enclosure ([Powder coat](https://www.taydaelectronics.com/cream-125b-style-aluminum-diecast-enclosure.html), [Custom Drill](https://www.taydaelectronics.com/125b-custom-drill-enclosure-service.html), and [UV print](https://www.taydaelectronics.com/125b-uv-printing-service.html) ordered from [Tayda](https://www.taydaelectronics.com/)) - $15
 - Electronic Components (Tayda, Mouser, Amazon, Digikey (for Dipswitches) etc.)
 - Hardware Components for Terrarium (1/4" Mono Audio Jacks, Power Jack, Knobs, Switches) (I like [LoveMySwitches](https://lovemyswitches.com/))
 - "Funbox" Software - Build your own binary from this repo or download .bin from the [Releases page]()
<br><br>
There are alot of variables in building your own pedal, but typically the Funbox will cost around $120 to $150 to build.

IMPORTANT: If you use the Drill Template, double check that the hole diameters match your components. Especially for the LED's, which use the larger diameter hole for 5mm [LED Bezel](https://lovemyswitches.com/5mm-chrome-metal-led-bezel-bag-of-5/).

The PCB design KiCad is provided here, which can be used to order from a manufacturer such as OSHPark or PCBWay. 


## Software

The following pedal modules are provided here:

Planet Series
- [Mars](https://github.com/GuitarML/Funbox/blob/main/software/Mars): An amp sim/delay using neural models and Impulse Responses and three delay modes.
- [Jupiter](https://github.com/GuitarML/Funbox/blob/main/software/Jupiter): A Reverb with a focus on EQ for shaping the sound.
- [Neptune](https://github.com/GuitarML/Funbox/blob/main/software/Neptune): A Reverb/Delay capable of ethereal sounds.
- [Pluto](https://github.com/GuitarML/Funbox/blob/main/software/Pluto): A Dual/Stereo looper with variable speed/direction control and real time loop effects.

There are also a couple [Experiments](https://github.com/GuitarML/Funbox/blob/main/software/Experiments) including a Chorus/Reverb and FFT based filter.

### Build the Software
Head to the [Electro-Smith Wiki](https://github.com/electro-smith/DaisyWiki) to learn how to set up the Daisy environment on your computer.

```
# Clone the repository
$ git clone https://github.com/GuitarML/Funbox.git
$ cd Funbox

# initialize and set up submodules
$ git submodule update --init --recursive

# Build the daisy libraries (after installing the Daisy Toolchain):
# Replace the daisy_petal files in libDaisy/src with the files in the "mod" directory to properly map controls on Funbox.
$ make -C libDaisy
$ make -C DaisySP

# Build the desired pedal firmware (Mars pedal shown below as example) (after installing the Daisy Toolchain)
$ cd software/Mars
$ make
```

Then upload the firmware (.bin) to your Funbox pedal with the following commands (or use the [Electrosmith Web Programmer](https://electro-smith.github.io/Programmer/))
```
# This is the procedure for uploading to Flash memory on the Daisy Seed, if using SRAM memory use Bootloader method (Mars and Neptune use SRAM).
cd your_pedal
# using USB (after entering bootloader mode)
make program-dfu
# using JTAG/SWD adaptor (like STLink)
make program
```

To upload your firmware to SRAM (more memory available here for larger programs), see the [libdaisy Bootloader Reference](https://electro-smith.github.io/libDaisy/md_doc_2md_2__a7___getting-_started-_daisy-_bootloader.html#autotoc_md49)
or use the [Electrosmith Web Programmer](https://electro-smith.github.io/Programmer/)

To use the Electrosmith Web Programmer to upload programs to SRAM (480KB available instead of 128KB in Flash), do this:
1. Connect to the Daisy Seed as normal from the [Electrosmith Web Programmer](https://electro-smith.github.io/Programmer/).
2. Click "Advanced" button at the bottom, and click "Flash Bootloader Image".
3. On the Daisy Seed, press the RESET button (LED should be lit), and then quickly press the BOOT button. The BOOT LED should now be pulsing.
4. Back in the Web Programmer, choose your .bin file from the "Choose File" button, and then click the "Program" button to upload to SRAM.
5. To load a program back to Flash after using the Bootloader, do the normal upload procedure to overwrite the Bootloader with your new program.

