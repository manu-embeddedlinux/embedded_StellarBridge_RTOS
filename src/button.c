#include <driver/gpio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "display.h"

#define DISP_BUTTON_GPIO 18
#define POWER_LATCH_GPIO 40
#define DISP_PWR_CTRL 16
#define LONG_PRESS_TIME_MS 4000
#define DEBOUNCE_TIME_MS 50

static bool backlight_state = true;

// Function declarations
void button_monitoring_task(void* pvParameters);
static bool is_first_boot = true;


void configure_gpio() {

    // Configure display control pin
    gpio_config_t disp_ctrl_config = {
        .pin_bit_mask = (1ULL << DISP_PWR_CTRL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&disp_ctrl_config);
    //gpio_set_drive_capability(DISP_PWR_CTRL, GPIO_DRIVE_CAP_3);
    gpio_set_level(DISP_PWR_CTRL, 1);  // Display on by default-


    // Configure button pin as input with internal pull-up
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << DISP_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&button_config);

    // Configure power latch pin as output
    gpio_config_t power_config = {
        .pin_bit_mask = (1ULL << POWER_LATCH_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&power_config);
}

static lv_obj_t *low_battery_screen = NULL;
/*
void show_low_battery_screen(lv_display_t *disp) {
    if (!low_battery_screen) {
        low_battery_screen = lv_obj_create(lv_display_get_scr_act(disp));
        lv_obj_set_size(low_battery_screen, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_bg_color(low_battery_screen, lv_color_hex(0xFF0000), LV_PART_MAIN);

        lv_obj_t *battery_label = lv_label_create(low_battery_screen);
        lv_label_set_text(battery_label, "LOW BATTERY!\nPlease Charge Immediately");
        lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_align(battery_label, LV_ALIGN_CENTER, 0, 0);
    }
    lv_obj_clear_flag(low_battery_screen, LV_OBJ_FLAG_HIDDEN);
}
*/
void hide_low_battery_screen() {
    if (low_battery_screen) {
        lv_obj_add_flag(low_battery_screen, LV_OBJ_FLAG_HIDDEN);
    }
}


extern  lv_obj_t *battery_icon;  // Global Battery Icon Object

void button_monitoring_task(void* pvParameters) {
    TickType_t press_start = 0;
    bool button_pressed = false;
    
    while (1) {
        int button_state = gpio_get_level(DISP_BUTTON_GPIO);
        
        if (button_state == 0 && !button_pressed) {  // Button pressed
            button_pressed = true;
            press_start = xTaskGetTickCount();
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
        }
        else if (button_state == 1 && button_pressed) {  // Button released
            button_pressed = false;
            TickType_t press_duration = xTaskGetTickCount() - press_start;
            
            if (press_duration >= pdMS_TO_TICKS(LONG_PRESS_TIME_MS)) {
                
                int adc_raw = adc1_get_raw(BATTERY_ADC_CHANNEL);
    

                float voltage_global =((adc_raw*3.3)/4095)*2.07;
              
                
                // Optional: Add any cleanup code here before power off
                // Stop all animations
                lv_anim_del_all();
                
                // Remove all screens
                lv_obj_t* scr = lv_screen_active();
                if (scr) {
                    // Clean all children of the current screen
                    lv_obj_clean(scr);
                }

           /*    if(voltage_global<3.6)
                {

                    lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_EMPTY);
                    lv_obj_set_style_text_color(battery_icon, lv_color_hex(0xFF0000), LV_PART_MAIN);
                    //                    show_low_battery_screen(lv_display_get_default());
                    vTaskDelay(pdMS_TO_TICKS(5000));  // Small delay for stabilization

                }*/

                
                // lv_obj_invalidate(lv_scr_act());

                // Create a new, blank screen
                lv_obj_t* new_screen = lv_obj_create(NULL);
                if (new_screen) {
                    lv_screen_load(new_screen);
                }

                // void *buf1, *buf2;
                // size_t buf_size;
                lv_display_t* display = lv_display_get_default();
                // lv_display_get_draw_buf(display, &buf1, &buf2, &buf_size);

                // if (buf1) {
                //     memset(buf1, 0, buf_size);
                // }
                // if (buf2) {
                //     memset(buf2, 0, buf_size);
                // }
                esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(display);
                esp_lcd_panel_disp_on_off(panel_handle, false);
                esp_lcd_panel_reset(panel_handle);

                // void *draw_buf = lv_display_get_draw_buf(display);
                // size_t draw_buf_size = lv_display_get_draw_buf_size(display);
                
                // if (draw_buf && draw_buf_size > 0) {
                //     memset(draw_buf, 0, draw_buf_size);
                // }
               
                
                // Notify LVGL flush is complete
                // lv_display_flush_ready(display);

                // esp_lcd_panel_disp_on_off(display, false);
                // esp_lcd_panel_del(display);
                

                // lv_refr_now(display);
                // lv_display_flush_ready(display);
                // lv_disp_remove(display); // Cleanup display registration.
                // lv_deinit(); // Cleanup LVGL resources.
                
                

                // Long press detected - power off
                gpio_set_level(POWER_LATCH_GPIO, 0);
            }
            else {
                // Short press detected - toggle display
                backlight_state = !backlight_state;
                gpio_set_level(DISP_PWR_CTRL, backlight_state);
                gpio_set_level(LCD_BL, backlight_state);
            }
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // Small delay to prevent busy waiting
    }
}