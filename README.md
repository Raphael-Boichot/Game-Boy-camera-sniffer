# The Game Boy camera sniffer
A data logger to intercept analog images from a Game Boy Camera. It consists of a middle man circuit in direct derivation of the camera sensor ribbon that drops anything that it sees as data images to an SD card. It does not interfere at all with normal Game Boy Camera operation.

# Why ?
Because it could be fun to record data directly from the Game Boy Camera sensor before it even enters the MAC-GBD. In this case, the signal being analog, any bits per pixel definition can be recorded. The whole device proposed here is fully autonomous by drawing current on the 5V bus of the Game Boy. It works with GBA, GBC and DMG and returns 8 bits images.

# What ?
**Software:**
- Install the last [Arduino IDE](https://www.arduino.cc/en/software).
- Install the [Earle F. Philhower Raspberry Pi Pico Arduino core for Arduino IDE](https://github.com/earlephilhower/arduino-pico) via the Arduino Board manager (see [installation guide](https://github.com/earlephilhower/arduino-pico#installing-via-arduino-boards-manager)).
- Compile the code and flash your board.

OR

- Just drop the compile uf2 file to the board in mass storage media mode (Connect the Pico to USB port with the BOOTSEL button pushed).

**Hardware:**
- an Arduino Pi Pico. [Fancy purple Chinese clones](https://fr.aliexpress.com/item/1005003928558306.html) are OK (this is still the genuine RP2040 chip) as long as you do not care that the pinout is completely baroque.
- a [MAX153 Analog to Digital Converter](https://fr.aliexpress.com/item/1005005084589973.html) (yes it is expensive...)
- a way to connect the device to the ribbon without destroying the original cable like an opened [replacement cable](https://www.digikey.com/en/products/base-product/jst-sales-america-inc/455/A09ZR09Z/588181) (choose B model).
- a big decoupling electrolytic capacitor, typically [1000 µF - 10 volts](https://fr.aliexpress.com/item/1005002958594141.html).
- in option, you can put a LED on GPIO11 to check access to the SD card (with a typical resistor of about 250-500 Ohms).

The sniffer can be powered by the USB cable or the sensor ribbon through CAM_VCC, but in this case it requires a large decoupling capacitor to avoid image artifacts due to voltage shift with time. If powered by USB, **do not connect anything to the CAM_VCC**, just connect CAM_GND and protocol wires.

# How ?
The device uses a MAX153 analog to digital converter. The idea comes from an [old paper](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/Yerazunis%20(1999)%20An%20Inexpensive%2C%20All%20Solid-state%20Video%20and%20Data%20Recorder%20for%20Accident%20Reconstruction.pdf) claiming to use the [M64282FP artificial retina coupled with a MAX153 flash ADC](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/Mitsubishi%20Integrated%20Circuit%20M64282FP%20Image%20Sensor.pdf) as a road dashcam system. This leads to [this project](https://github.com/Raphael-Boichot/Mitsubishi-M64282FP-dashcam) which uses the internal ADC of a Pi Pico but the idea here was to see if it is possible to directly intercept data during normal operation of the Game Boy Camera, like the [GB Interceptor](https://github.com/Staacks/gbinterceptor), and it requires a real external flash ADC, the SAR method used in the Pi Pico being much too slow. The MAX153 is used in its most simple way, **mode 0**, which allows a conversion in about 800 µs total, the cycle width of the Game Boy signal being 1.9 µs. The MAX153 has the advantage to have a very broad range of acceptable voltage. The scale here is set between 0 and 3.3 volts, like a real Game Boy Camera.

The data format on SD card is the simpliest possible to favor writing speed. Each boot of the Game Boy will create a session file that contains for each image 32 bytes of header information, then the raw 8 bits data for 128x120 pixels. The project comes with A Matlab Decoder but the file format is very easy to decode with any tool.

The two cores of the Pico are used but not in a fancy manner. Core 1 does all the work while Core 0 deals with the internal interrupts only. I think writing to SD would be delegate to core 0 too in the future. Using only core 0 leads to stalling every ms, which leads to about 30 jitters in every image. Some part of the code should sounds strange but remind that the code is the fruit of many trials an errors to find the optimal timings. It is stable in its current form.

The image you will get will look poorly contrasted, this is normal. It appears that the Game Boy camera uses a quite limited narrow voltage range, always in the upper range. The exact register strategy is still not fully understood at the moment and this tool may help elucidate it.

# Pinout
![Pinout](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/pinout.png)

# Detail of the sensor ribbon
![](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/pinout2.png)

# Comparison before/after the MAC-GBD
![comparison](https://github.com/Raphael-Boichot/Game-Boy-camera-sniffer/blob/main/Bibliography/test.gif)

# Known flaws
- Powered by the sensor ribbon only, the device needs a big decoupling capacitor to avoid voltage shifting and image artifacts. Additionnaly, modded Game Boys with fancy screens and NiMH batteries would suffer from this additionnal current draw and may boot randomly. So reserve this mod for unmodded consoles, unless you choose to power the device by USB, in which case there is no issue at all.
- The MAX153 ADC converts data at approximately 1.4 MSample/sec, but in the other hand the SD card can handle only 150 kB/s in writing mode. This means that I have to drop some frames sometimes to allow recording. The device can handle about 4 fps, not more. Using the second core to record may fix partially this, it has to be done in a future release.
- The READ and CLOCK of the Game Boy Camera are directly connected to the 3.3V input if the Pi Pico and this is evil, I know. If this is too annoying for you, use a [serious bus transciever](https://www.ti.com/lit/ds/symlink/sn74lvc4245a.pdf). Small level shifters common on Aliexpress do not fit because they pull their pins in idle state high, this makes the Game Boy crash.
