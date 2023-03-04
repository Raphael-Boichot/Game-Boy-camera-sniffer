# The Game Boy camera sniffer
A data logger to intercept analog images from a Game Boy Camera. It consists of a middle man circuit in direct derivation with the camera sensor ribbon that drops anything that it sees as data images to an SD card.

# Why ?
Because it could be fun to record data directly from the Game Boy Camera sensor before it even enters the MAC-GBD. In this case, the signal being analog, any bits per pixel definition can be recorded. The whole device proposed here is fully autonomous by drawing current on the 5V bus of the Game Boy. It works with GBA, GBC and DMG and returns 8 bits images.

# What ?
**Software:**
- Install the last [Arduino IDE](https://www.arduino.cc/en/software).
- Install the [Earle F. Philhower Raspberry Pi Pico Arduino core for Arduino IDE](https://github.com/earlephilhower/arduino-pico) via the Arduino Board manager (see [installation guide](https://github.com/earlephilhower/arduino-pico#installing-via-arduino-boards-manager).
- Compile the code and flash your board.

OR

- Just drop the compile uf2 file to the board in mass storage media mode (Connect the Pico to USB port with the BOOTSEL button pushed).

**Hardware:**
- an Arduino Pi Pico. [Fancy purple Chinese clones](https://fr.aliexpress.com/item/1005003928558306.html) are OK (this is still the genuine RP2040 chip) as long as you do not care that the pinout is completely baroque.
- a [MAX153 Analog to Digital Converter](https://fr.aliexpress.com/item/1005005084589973.html).
- a way to connect the device to the ribbon without destroying the original cable like an opened [replacement cable](https://www.digikey.com/en/products/base-product/jst-sales-america-inc/455/A09ZR09Z/588181) (choose B model).

# How ?
The device uses a MAX153 analog to digital converter. The idea comes from an [old paper](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/Yerazunis%20(1999)%20An%20Inexpensive%2C%20All%20Solid-state%20Video%20and%20Data%20Recorder%20for%20Accident%20Reconstruction.pdf) claiming to use the [M64282FP artificial retina](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/Mitsubishi%20Integrated%20Circuit%20M64282FP%20Image%20Sensor.pdf) as a road dashcam. This leads to [this project](https://github.com/Raphael-Boichot/Mitsubishi-M64282FP-dashcam) which uses the internal ADC of a Pi Pico but the idea here was to see if it was possible to directly intercept data during normal operation of the Game Boy Camera, like the [GB Interceptor](https://github.com/Staacks/gbinterceptor). The MAX153 is used in its most simple way, **mode 0**, which allows a conversion in about 800 µs total, the cycle width of the Game Boy signal being 1.9 µs. The MAX153 has the advantage to have a very broad range of acceptable voltage. The scale here is set between 0 and 3.3 volts, like a real Game Boy Camera.

The data format on SD card is the simpliest possible to favor writing speed. Each boot of the Game Boy will create a session file that contains for each image 32 bytes of information, then the raw 8 bits data for 128x120 pixels. The project comes with A Matlab Decoder but the file format is very easy to decode with any tool.

# Pinout
![Pinout](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/pinout.png)

# Comparison before/after the MAC-GBD
![comparison](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/test.gif)

# Known flaws
- Powered by the sensor ribbon only, the device needs a big decoupling capacitor to avoid voltage shifting and image artifacts. Additionnaly, modded Game Boys with fancy screens and NiMH batteries would suffer from this additionnal current draw and may boot randomly. So reserve this mod for unmodded consoles, unless you choose to power the device by USB, in which case there is no issue at all.
- The MAX153 ADC converts data at approximately 1.4 MSample/sec, but in the other hand the SD card can handle only 150 kB/s in writing mode. This means that I have to drop some frames sometimes to allow recording. The device can handle about 4 fps, not more. Using the second core to record may fix partially this, it has to be done in a future release.
- The READ and CLOCK of the Game Boy Camera are directly connected to the 3.3V input if the Pi Pico and this is evil, I know. If this is too annoying for you, use a [serious bus transciever]([httpBoy crash](https://www.ti.com/lit/ds/symlink/sn74lvc4245a.pdf). Small level shifters common on Aliexpress do not fit because they pull their pins in idle state high, this makes the Game Boy crash.
