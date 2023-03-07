#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "sniffer.h"

// Length of the pixel array, and number of DMA transfers
#define RXCOUNT     (128*128)   // Total pixels

// Pixel color array that is DMA's to the PIO machines and
// a pointer to the ADDRESS of this color array.
// Note that this array is automatically initialized to all 0's (black)
uint8_t picture_data[RXCOUNT];
uint8_t * picture_address = picture_data;

// Give the I/O pins that we're using some names that make sense
#define CTRL_PINS   8           // pin 8 is ready, pin 9 is clock
#define READY_PIN   10          // ADC ready pin
#define DATA_PINS   0           // pins 0..7 are ADC pins
#define SNIFFER_SM  0
#define DMA_PICTURE_CHAN_0 0
#define DMA_PICTURE_CHAN_1 1

volatile bool image_written = true;

void setup() {
  // Choose which PIO instance to use (there are two instances, each with 4 state machines)
  PIO pio = pio0;
  sniffer_program_init(pio, SNIFFER_SM, pio_add_program(pio, &sniffer_program), CTRL_PINS, READY_PIN, DATA_PINS);

  // Channel Zero (sends color data to PIO VGA machine)
  dma_channel_config c0 = dma_channel_get_default_config(DMA_PICTURE_CHAN_0); // default configs
  channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);                     // 8-bit txfers
  channel_config_set_read_increment(&c0, false);                              // yes read incrementing
  channel_config_set_write_increment(&c0, true);                              // no write incrementing
  channel_config_set_dreq(&c0, DREQ_PIO0_TX2) ;                               // DREQ_PIO0_TX2 pacing (FIFO)
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

  // attach interrupts
  irq_set_exclusive_handler(PIO0_IRQ_0, on_pio0_irq0);
  irq_set_enabled(PIO0_IRQ_0, true);
}

void loop() {
  // ...
  while (true) {
    if (!image_written) {
      //SD_write_file(bmp_image);
      image_written = true;
    }
  }
}


void on_pio0_irq0() {
  if (!image_written) return;
  //memcpy(bmp_image + BMP_HEADER_SIZE, picture_data, sizeof(picture_data));
  image_written = false;
}
