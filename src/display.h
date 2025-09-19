#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/lock.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "router_globals.h"

#include "radial_bg.h"
#include "logo.h"
#include "charging_logo.h"
#include "blue_hotspot.h"
#include "arrow.h"

#define LCD_HOST  SPI2_HOST

#define LCD_MOSI        35
#define LCD_DC          1
#define LCD_RST         21
#define LCD_CS          38
#define LCD_BL          47
#define LCD_CLK         36

#define LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)
#define LCD_V_RES   240
#define LCD_H_RES   240

// Bit number used to represent command and parameter
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

#define LVGL_DRAW_BUF_LINES    20 // number of display lines in each draw buffer
#define LVGL_TICK_PERIOD_MS    2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1
#define LVGL_TASK_STACK_SIZE   (4 * 1024)
#define LVGL_TASK_PRIORITY     2

#define RSSI_STRONG -50
#define RSSI_GOOD   -70
#define RSSI_WEAK   -80

#define BATTERY_ADC_CHANNEL ADC1_CHANNEL_5
#define ADC_SENSE_EN_GPIO 42
#define VBUS_SENSE_PIN 41

void lvgl_port_init(void);

void lvgl_port_task(void *arg);

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);

static _lock_t lvgl_api_lock;

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

static void example_increase_lvgl_tick(void *arg);

void example_lvgl_demo_ui(lv_display_t *disp);

static void example_lvgl_port_update_callback(lv_display_t *disp);

extern int get_wifi_signal_strength();

void lvgl_ui(lv_display_t *disp);

void update_connected_devices(lv_timer_t *timer);

void update_battery_label(lv_timer_t *timer);

void monitor_vbus_sense(lv_timer_t *timer);

void show_charging_icon(lv_obj_t *scr);

void hide_charging_icon_callback(lv_timer_t *timer);

void update_wifi_icon_cb(lv_timer_t *timer);

#endif
