//By Raphaël BOICHOT, made around 2023-03-04
//This code allows sniffing and recording analog data on a Game Boy Camera ribbon cable without changing the camera behavior
//it creates a raw datafile easy to decode each time the camera is booted.
//Reading an image takes 30 ms so hardware interrupts creates jitters every 4-5 lines if using core 0.
//So core 0 deals with the internal interrupts and SD/serial, core 1 with bitbanging the MAX153 on time
//5The code so runs on core 1, core 0 dealing with interrupts
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#define NOP __asm__ __volatile__ ("nop\n\t")
#define CHIPSELECT 17//for SD card, I recommend not changing it

//Beware, SD card MUST be attached to these pins as the pico seems not very tolerant with SD card pinout, they CANNOT be changed
//   SD_MISO - to pi pico pin GPIO16
//   SD_MOSI - to pi pico pin GPIO19
//   SD_CS   - to pi pico pin GPIO17
//   SD_SCK  - to pi pico pin GPIO18

//#define USE_SERIAL //mode for outputing image in ascii to the serial console, unstable if SD is used at the same time

#ifndef  USE_SERIAL
#define USE_SD //to allow recording on SD
#endif

#ifdef  USE_SD
#include <SPI.h>  //for SD
#include <SD.h>  //for SD
#endif

//the device do not use a level shifter, you have to trust the 5V tolerance of the pi pico or add a SN74LVC4245A !
//the small level shifters adafruit-like do no work because they are default high what crashes the Game Boy immediately
char READ_CAMERA =  8;//signal that goes up at the beginning of data transmission
char CLOCK = 9;//1 MHz cloock from the Game boy Camera
char READ_MAX153 =  10;//goes low to trigger the MAX153, wait 6-700 ns then need a 200 ns recovery time
char LED_WRITE =  11;//you can add a LED to check access to SD if you want to
char LED =   25;//internal LED high when READ_CAMERA is high
char pixel;
int counter, t_in, t_out;
unsigned char CamData[128 * 128];// raw sensor data in 8 bits per pixel
unsigned char OutputData[128 * 128];// raw sensor data in 8 bits per pixel
unsigned long Next_ID;
char storage_file_name[20];
bool record_enable = 0;
bool output_enable = 0;
bool write_to_SD = 0;
void setup()
{ //MAX153 outputs bits, DX=GPIOX fo simplicity
  gpio_init(0);       gpio_set_dir(0, GPIO_IN);//MAX153 D0
  gpio_init(1);       gpio_set_dir(1, GPIO_IN);//MAX153 D1
  gpio_init(2);       gpio_set_dir(2, GPIO_IN);//MAX153 D2
  gpio_init(3);       gpio_set_dir(3, GPIO_IN);//MAX153 D3
  gpio_init(4);       gpio_set_dir(4, GPIO_IN);//MAX153 D4
  gpio_init(5);       gpio_set_dir(5, GPIO_IN);//MAX153 D5
  gpio_init(6);       gpio_set_dir(6, GPIO_IN);//MAX153 D6
  gpio_init(7);       gpio_set_dir(7, GPIO_IN);//MAX153 D7

  gpio_init(LED_WRITE);       gpio_set_dir(LED_WRITE, GPIO_OUT);//additional LED, you can get rid of it
  gpio_init(LED);       gpio_set_dir(LED, GPIO_OUT);//internal LED, flashes when data are transmitted
  gpio_init(READ_CAMERA);      gpio_set_dir(READ_CAMERA, GPIO_IN); gpio_pull_down(READ_CAMERA);//CAM_READ, signal to trigger reading from camera
  gpio_init(CLOCK);      gpio_set_dir(CLOCK, GPIO_IN); gpio_pull_down(CLOCK);//CAM_CLOCK signal to trigger reading a pixel from camera
  gpio_init(READ_MAX153);      gpio_set_dir(READ_MAX153, GPIO_OUT); gpio_put(READ_MAX153, 1);///MAX153 RD, signal to trigger reading to the MAX153 in mode 0
  set_sys_clock_khz(250000, true);//mandatory to complete all the tasks within 2 microseconds
#ifdef USE_SERIAL
  Serial.begin(2000000);//in case of debugging
#endif

#ifdef  USE_SD
  SD.begin(CHIPSELECT);
  ID_file_creator("/Dashcam_storage.bin");//create a file on SD card that stores a unique file ID from 1 to 2^32 - 1
  Next_ID = get_next_ID("/Dashcam_storage.bin");//get the file number on SD card
  Next_ID++;//update for next session.
  sprintf(storage_file_name, "/CamData/%07d.raw", Next_ID); //update filename
  store_next_ID("/Dashcam_storage.bin", Next_ID);//store last known file/directory# to SD card
#endif

}
void loop()//core 0 records the images
{
  if (output_enable == 1) {
    write_to_SD = 1;
    output_enable = 0;
    gpio_put(LED_WRITE, 1);//triggers the additionnal LED on
#ifdef  USE_SD
    record_image();//raw format, just 8 bits pixel value in a 128*120 row
#endif

#ifdef USE_SERIAL
    dump_data_to_serial();//dump raw data to serial in ASCII for debugging - you can use the Matlab code ArduiCam_Matlab.m into the repo to probe the serial and plot images
#endif
    gpio_put(LED_WRITE, 0);//triggers the LED off
    write_to_SD = 0;
  }
}


void setup1()
{
  // Nothing here
}

void loop1()//core 1 polls the camera
{
  if ((gpio_get(READ_CAMERA) == 1)) //READ signal from camera goes high on a rising front
  {
    gpio_put(LED, 1);//triggers the internal LED on
    counter = 0;//resets the rank in pixel array
    while (counter < (128 * 120))//last 8 lines are junk, no need to record them
    {
      {
        for (int i = 0; i < 10; i++) NOP;//wait a bit to let the VOUT stabilize
      }
      gpio_put(READ_MAX153, 0);//triggers a measurement on the MAX153
      {
        for (int i = 0; i < 37; i++) NOP;//37 minimal waiting time for data to be ready with the MAX153, about 6-700 ns, but depends on temperature so a margin is taken
      }
      CamData[counter] = gpio_get_all() & 0b00000000000000000000000011111111; //get all the GPIOs state at once. The mask is not necessary as packing an int into a char does the same job...
      gpio_put(READ_MAX153, 1);//MAX153 returns in idle mode
      counter++;
      while (gpio_get(CLOCK) == 0);//wait until CLOCK camera goes high again
    }
    while (gpio_get(READ_CAMERA) == 1); //wait until READ_CAMERA camera goes low again
    gpio_put(LED, 0);//triggers the internal LED off
    if (write_to_SD == 0) memcpy(OutputData, CamData, sizeof(CamData));
    output_enable = 1;
  }
}

void ID_file_creator(const char * path) {
#ifdef  USE_SD
  uint8_t buf[4] = {0, 0, 0, 0};
  if (!SD.exists(path)) {
    File file = SD.open(path, FILE_WRITE);
    //start from a fresh install on SD
    file.write(buf, 4);
    file.close();
  }
#endif
}

unsigned long get_next_ID(const char * path) {
#ifdef  USE_SD
  uint8_t buf[4];
  File file = SD.open(path);
  file.read(buf, 4);
  Next_ID = ((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]));
  file.close();
#endif
  return Next_ID;
}

void store_next_ID(const char * path, unsigned long Next_ID) {
#ifdef  USE_SD
  uint8_t buf[4];
  File file = SD.open(path, FILE_WRITE);
  file.seek(0);
  buf[3] = Next_ID >>  0;
  buf[2] = Next_ID >>  8;
  buf[1] = Next_ID >> 16;
  buf[0] = Next_ID >> 24;
  file.write(buf, 4);
  file.close();
#endif
}

void dump_data_to_serial() {
  Serial.println("Data transmitted");
  delay(250);
  for (int i = 0; i < 128 * 120; i++) {
    pixel = OutputData[i];
    if (pixel <= 0x0F) Serial.print('0');
    Serial.print(pixel, HEX);
    Serial.print(" ");
  }
  Serial.println("");
}

void record_image() {
#ifdef  USE_SD
  File dataFile = SD.open(storage_file_name, FILE_WRITE);
  // if the file is writable, write to it:
  if (dataFile) {
    dataFile.write("RAW_8BIT_128x120");//Just a marker with keywords
    dataFile.write("ANALOG_RECORDER_");//Just a marker to keep compatibility with https://github.com/Raphael-Boichot/Mitsubishi-M64282FP-dashcam
    dataFile.write(OutputData, 128 * 120);
    dataFile.close();
  }
#endif
}
