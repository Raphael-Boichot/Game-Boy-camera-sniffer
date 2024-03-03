# The Game Boy camera analog interceptor
A data logger to intercept analog images from a Game Boy Camera. It consists of a middle man circuit in direct derivation of the camera sensor ribbon that drops anything that it sees as 8-bits data images to an SD card. It does not interfere at all with normal Game Boy Camera operation.

# Why ?
Because it could be fun to record data directly from the Game Boy Camera sensor before it even enters the MAC-GBD. In this case, the signal being analog, any bits per pixel definition can be recorded. The whole device proposed here is fully autonomous and can be powered by drawing current on the 5V bus of the Game Boy or by USB cable (see next sections). It should work with GBA, GBC and DMG but has been developped on GBC.

# What ?
**Software:**
- Install the last [Arduino IDE](https://www.arduino.cc/en/software).
- Install the [Earle F. Philhower Raspberry Pi Pico Arduino core for Arduino IDE](https://github.com/earlephilhower/arduino-pico) via the Arduino Board manager (see [installation guide](https://github.com/earlephilhower/arduino-pico#installing-via-arduino-boards-manager)).
- Compile the code @250 MHz and flash your board.

OR

- Just drop the compiled uf2 file to the board in mass storage media mode (Connect the Pico to USB port with the BOOTSEL button pushed and release).

**Hardware:**
- [PCB](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/tree/main/PCB) ordered at [JLCPCB](https://jlcpcb.com/). Just drop the Gerber .zip files on their site and order with default options. Use at your own risk.
- An [Arduino Pi Pico](https://fr.aliexpress.com/item/1005003928558306.html). **Be sure to select the regular/original green board with the official pinout and castellated holes ("Color: Pico Original").**
- a [MAX153 Analog to Digital Converter](https://fr.aliexpress.com/item/1005005084589973.html) (yes it is expensive but old new stock...)
- Some [9 pins JST ZH1.5MM connector](https://fr.aliexpress.com/item/32920487056.html). If it comes with straight pins, you can bend them easily.
- Some [9 pins, double head, 10 cm, JST ZH1.5MM cables](https://fr.aliexpress.com/item/1005004501408268.html).
- a big decoupling electrolytic capacitor, typically [1000 µF - 10 volts](https://fr.aliexpress.com/item/1005002958594141.html).
- 1 [microswitches SS-12D00G](https://fr.aliexpress.com/item/1005003938856402.html) to shift between USB and Game Boy power (do not solder at all to keep powering by USB only).
- 1 [regular 5 mm LEDs](https://fr.aliexpress.com/item/32848810276.html) and 1 [through hole resistors](https://fr.aliexpress.com/item/32866216363.html) of 250 to 1000 Ohms (low value = high brighness).

# How ?
The device uses a MAX153 analog to digital converter (ADC). The idea comes from an [old paper](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/Yerazunis%20(1999)%20An%20Inexpensive%2C%20All%20Solid-state%20Video%20and%20Data%20Recorder%20for%20Accident%20Reconstruction.pdf) proposing to use the [M64282FP artificial retina coupled with a MAX153 flash ADC](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/Mitsubishi%20Integrated%20Circuit%20M64282FP%20Image%20Sensor.pdf) as a 5 fps road dashcam system. This leads to [this project](https://github.com/Raphael-Boichot/Mitsubishi-M64282FP-dashcam) which uses the internal ADC of a Pi Pico but the idea of the present sniffer was to directly intercept data during normal operation of the Game Boy Camera, like the [GB Interceptor](https://github.com/Staacks/gbinterceptor). It requires a real external flash ADC, the SAR method (Successive Aproximation Register) used in the Pi Pico being too slow (not by much, but too slow anyway for this application). The MAX153 is used in its most simple way, **mode 0**, which allows a voltage conversion in about 800 ns in total. The cycle width of the camera sensor clock being 1900 ns, there is plenty of time left. The MAX153 has the advantage to have a very broad range of readable acceptable voltage. The scale here is set between 0 and 3.3 volts, close or similar to a real Game Boy Camera. The MAX153 have multiple driving modes so the sniffer may probably be improved to go faster.

The data format recorded on SD card is the simpliest possible to favor writing speed. Each boot of the Game Boy will create a new session file which contains for each image 32 bytes of header information, then the raw 8 bits data for 128x120 pixels. The project comes with a [Matlab decoder](Tools/Movie_and_Gif_Maker_from_raw_data.m) but the file format is very easy to decode with any tool. **If you are interested by this project and still reading me, I suppose that you are nerd enough to write your own decoder** in Javascript, Python, GNU Octave, etc. Data are returned in 128x120 pixels because [5-6 last lines are unusable](https://github.com/Raphael-Boichot/Game-Boy-chips-decapping-project#game-boy-camera-mitsubishi-m64282fp) (sensor is theoretically 128x128, but practically 128x123).

The two cores of the Pico are used. Core 1 speaks with the sensor while Core 0 deals with the internal interrupts and the SD card. Using core 0 for dealing with the sensor is impossible as it leads to stalling every ms due to timer interrupts. Some part of the code should sounds strange but remind that the code is the fruit of many trials an errors to find the optimal timings. It is stable in its current form. Due to the very short timings, the Pico is overclocked at 250 MHz. Interrupts are too slow (and not always triggered due to the fact that the signal is rather unstable) to be used efficiently so polling was mandatory. Apart from that, the code is short and simple, it can be summarized as:
- Initialise GPIOs, SD card and filename, put the RD pin of MAX153 high (waiting state);
- Wait for a rising front on CAM_READ and enter the conversion loop;
- For 128x120 pixels, do:
  - Wait 10 cycles;
  - Set the MAX153 RD pin low (ask for voltage conversion);
  - Wait 37 cycles (approx 600-700 ns);
  - Get all GPIOs state at once and store value of GPIOs 0-7 as 8 bits char in an array;
  - Set the MAX153 RD pin high;
  - Wait while the CAM_CLOCK is low;
- Dump the pixel array to a memory buffer and rise a flag;
- Write the buffer to SD and reset the flag;
- Loop until next rising front on CAM_READ;

The MAX153 needs 200 ns to recover after a voltage conversion but the CAM_CLOCK cycle is long enough to avoid dealing with that delay, so it is omitted in the code.
Timing between cores 0 and 1 allows recording at several FPS.

The 8-bit images you will get are natively poorly contrasted, this is normal. It appears that the Game Boy camera uses a quite limited narrow voltage range, always in the upper range, and [compensate the lack of contrast with clever dithering matrices](https://github.com/HerrZatacke/dither-pattern-gen). The exact register strategy of the Game Boy Camera is still not fully understood at the moment and this tool may help elucidate it. You can of course enhance contrast during post-treatment of image data.

# Pinout
![Pinout](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/pinout.png)

# Detail of the sensor ribbon
![ribbon](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/pinout2.png)

# How I've made it (yes this is a disgusting prototype !)
![mounting](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/device.png)

# Comparison analog image/what you get with a Game Boy Camera
![comparison](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/test.gif)

# Known flaws
- Powered by the sensor ribbon only, the device needs a big decoupling capacitor to avoid voltage shifting and image artifacts. SD recording consumes indeed a ton of current. Additionnaly, modded Game Boys with fancy screens and NiMH batteries would suffer from this additionnal current draw and may boot randomly. So reserve this mod for unmodded consoles, unless you choose to power the device by USB or with a powerbank, in which case there is no issue at all.
- The MAX153 ADC converts data at approximately 1.4 MSample/sec, but in the other hand the SD card can handle only 150 kB/s in writing mode. This means that I have to drop some frames sometimes to allow recording. The device can handle about 4 fps in 8-bits depth, not more, and only due to SD limitation on Pi Pico.
- The 5V signals from READ and CLOCK of the Game Boy Camera are directly connected to the 3.3V input of the Pi Pico and this is evil, I know. If this is too annoying for you, susceptible student in electrical engineering who still trusts what your teachers say, use a [serious bus transceiver](https://www.ti.com/lit/ds/symlink/sn74lvc4245a.pdf). **Small shitty level shifters common on Aliexpress do not fit the purpose** because they pull their pins HIGH when idle, and the Game Boy does not like that at all. I've tried adding 50k resistors but it does not work at all.
- The image has still more or less jitters in SD mode. I do not understand why but this may probably be fixable with some more work and a more solid design. Serial output on the other hand suffers no glitches.

# Kind warning
The code comes as it. If you're not happy with the current implementation and the Arduino IDE, build your own and debug it ! Tuning the timing parameters was extremely long and tedious and I won't debug any other variant of the current schematic, of the current code or with another IDE as it is perfectly working.

Push request with tested and working improvements are of course still welcomed.
