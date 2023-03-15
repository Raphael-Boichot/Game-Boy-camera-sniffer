//By https://github.com/untoxa (PIO) and https://github.com/Raphael-Boichot (timing on real stuff)

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
// assembled program:
#include "sniffer.pio.h"


// Length of the pixel array, and number of DMA transfers
#define RXCOUNT     (128*128)   // Total pixels
#define CHIPSELECT 17//for SD card, I recommend not changing it

#define USE_SERIAL //mode for outputing image in ascii to the serial console, unstable if SD is used at the same time

#ifndef  USE_SERIAL
#define USE_SD //to allow recording on SD
#endif

#ifdef  USE_SD
#include <SPI.h>  //for SD
#include <SD.h>  //for SD
#endif

// Pixel color array that is DMA's to the PIO machines and
// a pointer to the ADDRESS of this color array.
// Note that this array is automatically initialized to all 0's (black)
uint8_t picture_data[RXCOUNT];
uint8_t * picture_address = picture_data;
unsigned long Next_ID;
char pixel;
char LED =   25;//internal LED high when READ_CAMERA is high
int pixel_out;

#define SNIFFER_SM  0
#define DMA_PICTURE_CHAN_0 0
#define DMA_PICTURE_CHAN_1 1

// Choose which PIO instance to use (there are two instances, each with 4 state machines)
PIO pio = pio0;

// reset DMA channel to start from the beginning of the pixel array
static inline void reset_dma(void) {
  dma_hw->ch[DMA_PICTURE_CHAN_0].write_addr = (uint32_t)picture_data;
}
// signal pio interrupt handled
static inline void pio_interrupt_commit(void) {
  if (pio_interrupt_get(pio, 0)) pio_interrupt_clear(pio, 0);
}


//// image array and dump routine
uint8_t image[RXCOUNT];
volatile bool image_ready = false;
//static inline void dump_image(uint8_t * data, size_t size) {
//  for (size_t i = 0; i != size; i++) {
//    pixel_out = *data++;
//    Serial.print(pixel_out, HEX);
//  }
//  printf("\n\n");
//}


void setup() {
  // Initialize stdio
  sniffer_program_init(pio, SNIFFER_SM, pio_add_program(pio, &sniffer_program));

  // Channel Zero (sends color data to PIO VGA machine)
  dma_channel_config c0 = dma_channel_get_default_config(DMA_PICTURE_CHAN_0); // default configs
  channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);                     // 8-bit txfers
  channel_config_set_read_increment(&c0, false);                              // yes read incrementing
  channel_config_set_write_increment(&c0, true);                              // no write incrementing
  channel_config_set_dreq(&c0, pio_get_dreq(pio, SNIFFER_SM, false));
  channel_config_set_chain_to(&c0, DMA_PICTURE_CHAN_1);                       // chain to other channel

  dma_channel_configure(
    DMA_PICTURE_CHAN_0,                         // Channel to be configured
    &c0,                                        // The configuration we just created
    picture_data,                               // Write address
    &pio->rxf[SNIFFER_SM],                      // Read address
    RXCOUNT,                                    // Number of transfers; in this case each is 1 byte.
    false                                       // Don't start immediately.
  );

  // Channel One (reconfigures the first channel)
  dma_channel_config c1 = dma_channel_get_default_config(DMA_PICTURE_CHAN_1); // default configs
  channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);                    // 32-bit txfers
  channel_config_set_read_increment(&c1, false);                              // no read incrementing
  channel_config_set_write_increment(&c1, false);                             // no write incrementing
  channel_config_set_chain_to(&c1, DMA_PICTURE_CHAN_0);                       // chain to other channel

  dma_channel_configure(
    DMA_PICTURE_CHAN_1,                         // Channel to be configured
    &c1,                                        // The configuration we just created
    &dma_hw->ch[DMA_PICTURE_CHAN_0].write_addr, // Write address (channel 0 read address)
    &picture_address,                           // Read address (POINTER TO AN ADDRESS)
    1,                                          // Number of transfers, in this case each is 4 byte
    false                                       // Don't start immediately.
  );

  // Initialize PIO state machine counter
  pio_sm_put_blocking(pio, SNIFFER_SM, RXCOUNT - 1);

  // Start pio machine
  pio_enable_sm_mask_in_sync(pio, (1u << SNIFFER_SM));

  // Start DMA channel 0
  dma_start_channel_mask((1u << DMA_PICTURE_CHAN_0)) ;

  // Set up IRQ0
  pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
  irq_set_exclusive_handler(PIO0_IRQ_0, image_captured_isr);
  irq_set_priority(PIO0_IRQ_0, 0);
  irq_set_enabled(PIO0_IRQ_0, true);
  uint32_t seq = 0;

  gpio_init(LED);       gpio_set_dir(LED, GPIO_OUT);//internal LED, flashes when data are transmitted to serial or SD
  Serial.begin(2000000);
}

void loop() {
  if (image_ready) {
    gpio_put(LED, 1);//triggers the additionnal LED on
    //    dump_image(image, sizeof(image));

#ifdef  USE_SD
    dump_data_to_SD_card();//raw format, just 8 bits pixel value in a 128*120 row
#endif

#ifdef USE_SERIAL
    dump_data_to_serial();//dump raw data to serial in ASCII for debugging - you can use the Matlab code ArduiCam_Matlab.m into the repo to probe the serial and plot images
#endif
    gpio_put(LED, 0);//triggers the LED off
    image_ready = false;
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
  for (int i = 0; i < 128 * 120; i++) {
    pixel = image[i];
    if (pixel <= 0x0F) Serial.print('0');
    Serial.print(pixel, HEX);
    Serial.print(" ");
  }
  Serial.println("");
}

void dump_data_to_SD_card() {
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

// PIO ISR handler
void image_captured_isr(void) {
  static uint32_t seq = 0;
  // signal image was captured
  if (!image_ready) memcpy(image, picture_data, sizeof(picture_data)), image_ready = true;
  // signal irq0 handled
  pio_interrupt_commit();
}
