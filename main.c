#include <stdio.h>

#include "hardware/spi.h"
#include "pico/stdlib.h"

void init_periphery(void);
void switch_state(uint gpio, uint32_t events);
void preset_1(void);
void preset_2(void);
void idle(void);

enum state_t {
  S_IDLE = 0,
  S_PRESET_1 = 1,
  S_PRESET_2 = 2
} current_state = S_IDLE,
  last_state = S_IDLE;

int main() {
  init_periphery();

  while (true) {
    switch (current_state) {
      case S_PRESET_1:
        preset_1();
        break;
      case S_PRESET_2:
        preset_2();
        break;
      default:
        idle();
        break;
    }
  }
}

#define PIN_SPI_RX 0
#define PIN_SPI_CS 1
#define PIN_SPI_CLK 2
#define PIN_SPI_TX 3
#define PIN_SWITCH_STATE_IRQ 14
// INFO: GPIO 26 delivers VCC for interrupt-pin 14
#define PIN_SWITCH_STATE_POWER 26

#define HIGH true

#define FREQ_SPI 100000
#define BITS_PER_TRANSFER 8

void init_periphery(void) {
  stdio_init_all();

  spi_init(spi0, FREQ_SPI);
  spi_set_slave(spi0, true);
  spi_set_format(spi0, BITS_PER_TRANSFER, SPI_CPOL_0, SPI_CPHA_1,
                 SPI_MSB_FIRST);
  gpio_set_function(PIN_SPI_RX, GPIO_FUNC_SPI);
  gpio_set_function(PIN_SPI_CS, GPIO_FUNC_SPI);
  gpio_set_function(PIN_SPI_CLK, GPIO_FUNC_SPI);
  gpio_set_function(PIN_SPI_TX, GPIO_FUNC_SPI);

  gpio_init(PIN_SWITCH_STATE_POWER);
  gpio_set_dir(PIN_SWITCH_STATE_POWER, GPIO_OUT);
  gpio_put(PIN_SWITCH_STATE_POWER, HIGH);

  gpio_set_irq_enabled_with_callback(PIN_SWITCH_STATE_IRQ, GPIO_IRQ_EDGE_RISE,
                                     true, &switch_state);
  gpio_pull_down(PIN_SWITCH_STATE_IRQ);
}

#define DEBOUNCE_TIME_MS 400

static uint32_t last_interrupt_time = 0;

void switch_state(uint gpio, uint32_t events) {
  uint32_t current_time = to_ms_since_boot(get_absolute_time());

  if (current_time - last_interrupt_time < DEBOUNCE_TIME_MS) {
    return;  // Ignore interrupt (debouncing)
  }

  last_interrupt_time = current_time;

  if (current_state == S_IDLE && last_state == S_IDLE) {
    printf("1\n");
    current_state = S_PRESET_2;
  } else if (current_state == S_PRESET_1 && last_state == S_IDLE) {
    printf("2\n");
    current_state = S_IDLE;
    last_state = S_PRESET_1;
  } else if (current_state == S_IDLE && last_state == S_PRESET_1) {
    printf("3\n");
    current_state = S_PRESET_2;
    last_state = S_IDLE;
  } else if (current_state == S_PRESET_2 && last_state == S_IDLE) {
    printf("4\n");
    current_state = S_IDLE;
    last_state = S_PRESET_2;
  } else if (current_state == S_IDLE && last_state == S_PRESET_2) {
    printf("5\n");
    current_state = S_PRESET_1;
    last_state = S_IDLE;
  } else {
    printf("6 %d %d\n", current_state, last_state);
  }
}

#define BUF_LEN 4

static const uint8_t DATA_PRESET_1[BUF_LEN] = {0xA5, 0x85, 0x20, 0x5F};

void preset_1(void) {
  for (size_t i = 0; i < 2800; ++i) {
    if (current_state == S_IDLE) {
      return;
    }
    spi_write_blocking(spi0, DATA_PRESET_1, BUF_LEN);
    printf("Writing Preset 1, pkg %d finished\n", i);
    // TODO: Observe: Which package is sent, when desk stops at safety-position?
    //    If that is known, there might be a value that can be used globally
    //    throughout both for-loops
    if (i == 2300) {
      printf("Sleeping...\n");
      sleep_ms(50);
    }
  }
  printf("Reached position 1\n");
  current_state = S_IDLE;
  last_state = S_PRESET_1;
}

static const uint8_t DATA_PRESET_2[BUF_LEN] = {0xA5, 0x85, 0x10, 0x6F};

void preset_2(void) {
  for (size_t i = 0; i < 2300; ++i) {
    if (current_state == S_IDLE) {
      return;
    }
    spi_write_blocking(spi0, DATA_PRESET_2, BUF_LEN);
    printf("Writing Preset 2, pkg %d finished\n", i);
  }
  printf("Reached position 2\n");
  current_state = S_IDLE;
  last_state = S_PRESET_2;
}

static const uint8_t DATA_IDLE[BUF_LEN] = {0xA5, 0x85, 0x00, 0x7F};

void idle(void) {
  while (true) {
    if (current_state != S_IDLE) {
      return;
    }
    spi_write_blocking(spi0, DATA_IDLE, BUF_LEN);
  }
}
