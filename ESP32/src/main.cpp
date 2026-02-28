#include <Arduino.h>
#define LGFX_USE_V1
#include "ble_manager.h"
#include "ui.h"
#include <CST816S.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

// Custom LovyanGFX Setup provided by User
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read = 20000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 6;
      cfg.pin_mosi = 7;
      cfg.pin_miso = -1;
      cfg.pin_dc = 2;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 10;
      cfg.pin_rst = -1;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.panel_width = 240;
      cfg.panel_height = 240;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;
CST816S touch(4, 5, 1, 0); // sda, scl, rst, int

#define BUF_SIZE 120

// LVGL Display buffer mapped for DMA
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[2][SCREEN_WIDTH * BUF_SIZE]; // Double buffering

// Display flush callback for LovyanGFX
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area,
                   lv_color_t *color_p) {
  if (tft.getStartCount() == 0) {
    tft.endWrite();
  }

  tft.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1,
                   area->y2 - area->y1 + 1, (uint16_t *)&color_p->full);

  lv_disp_flush_ready(disp_drv);
}

// Touchpad read callback
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  if (touch.available()) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touch.data.x;
    data->point.y = touch.data.y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize Touch Controller
  touch.begin();

  // Initialize TFT Display using LovyanGFX DMA workflow
  tft.init();
  tft.initDMA();
  tft.startWrite();

  // Enable Backlight specific to this board
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);

  // Initialize LVGL
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf[0], buf[1], SCREEN_WIDTH * BUF_SIZE);

  // Register Display Driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_WIDTH;
  disp_drv.ver_res = SCREEN_HEIGHT;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Register Touch Driver
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  // Initialize user interfaces
  ui_init();

  // Initialize BLE Server / Client
  ble_init();
}

void loop() {
  lv_timer_handler();
  delay(5);
}
