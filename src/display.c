#include "display.h"
#include "esp_log.h"
static const char *TAG = "display";

#define Low_Battery_LightOff 16
esp_lcd_panel_handle_t panel_handle = NULL;

static lv_obj_t *connected_devices_label;  // Global Connected Devices label object
static int prev_connected_devices = -1;    // Initial count set to -1 to indicate uninitialized

static lv_obj_t *wifi_icon;     // Wifi Icon object
static lv_obj_t *hotspot_icon; // Hotspot Icon object
static lv_obj_t *battery_label; // Global Battery Label Object
 lv_obj_t *battery_icon;  // Global Battery Icon Object
static lv_obj_t *charging_icon = NULL; // Charging icon label object
#define power_latch 40

void lvgl_port_init(void)
{
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_BL,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(LCD_BL, 0);

    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_CLK,
        .mosi_io_num = LCD_MOSI,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };

    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC,
        .cs_gpio_num = LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // Declare and initialize panel_config
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };

    ESP_LOGI(TAG, "Install GC9A01 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));

    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));

    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Turn on LCD backlight");
   // gpio_set_level(LCD_BL, 1);

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();


    // create a lvgl display
    lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);
    assert(display != NULL);  // Ensure the display creation was successful

    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    size_t draw_buffer_sz = LCD_H_RES * 50 * sizeof(lv_color16_t);

    // void *buf1 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    // assert(buf1);
    // void *buf2 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    // assert(buf2);

    void *buf1 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_DMA);
    assert(buf1 != NULL);
    void *buf2 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_DMA);
    assert(buf2 != NULL);

    // initialize LVGL draw buffers
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    // associate the mipi panel handle to the display
    lv_display_set_user_data(display, panel_handle);
    // set color depth
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    // set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb(display, example_lvgl_flush_cb);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };

    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));


    ESP_LOGI(TAG, "Register io panel event callback for LVGL flush ready notification");
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = example_notify_lvgl_flush_ready,
    };

    /* Register done callback */
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display));

    //Create a task pinned to Core 1 for LVGL processing (Assuming Core 0 is used by Wifi)
    BaseType_t task_result = xTaskCreatePinnedToCore(
        lvgl_port_task,                    // Task function
        "LVGL Task",                  // Task name
        4096,                         // Stack size
        NULL,                         // Task parameters
        tskIDLE_PRIORITY + 1,         // Task priority
        NULL,                         // Task handle (not needed here)
        1                              // Core to pin the task to (0 or 1)
    );

    if (task_result == pdPASS) {
        ESP_LOGI(TAG, "LVGL task created and pinned to core 1");
    } else {
        ESP_LOGE(TAG, "Failed to create LVGL task");
    }

    // ESP_LOGI(TAG, "Create LVGL task");
    // xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Display LVGL Meter Widget");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    //_lock_acquire(&lvgl_api_lock);
   // lvgl_ui(display);
   // _lock_release(&lvgl_api_lock);

   int vbus_state = gpio_get_level(VBUS_SENSE_PIN);
   ESP_LOGI("VBUS", "VBUS state: %d", vbus_state);

    //Mallesh Add this
 
    vTaskDelay(pdMS_TO_TICKS(10));  // Small delay for stabilization
    // Perform ADC reading
    float adc_sample=0;
    int samples = 10;
    int total_adc = 0;
    for(int i = 0; i < samples; i++) {
        total_adc += adc1_get_raw(BATTERY_ADC_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(2));  // small delay between samples
    }
    int adc_raw = total_adc / samples;
    adc_sample =((adc_raw*3.3)/4095);
    ESP_LOGI(TAG, "adc_raw %d, adc sample %.2f ", adc_raw, adc_sample);

    if (adc_sample < 1.43) {   // equal to  3.38  ( 1.43 * 0.42211)
            gpio_set_level(LCD_BL, 1);
            // Voltage LOW → show warning
            ESP_LOGW(TAG, "Low Battery, skipping full UI");
        
            _lock_acquire(&lvgl_api_lock);
            lv_obj_t *scr = lv_display_get_screen_active(display);
            lv_obj_clean(scr);  // Clear the screen
            lv_obj_t *label = lv_label_create(scr);
            lv_label_set_text(label, "Low Battery\nPlease Charge\r\n");
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
            _lock_release(&lvgl_api_lock);
        
            // (Optional) Wait and shut down device
            vTaskDelay(pdMS_TO_TICKS(5000));
            gpio_set_level(power_latch, 0);  // Comment out if you don't want auto shutdown
    }
    else
    {
        gpio_set_level(LCD_BL, 1);        // Voltage OK → launch main UI
        ESP_LOGI(TAG, "Direct Battery voltage high Launching full UI");
        _lock_acquire(&lvgl_api_lock);
        lvgl_ui(display);
        _lock_release(&lvgl_api_lock);
    }
}

void lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");

    while (1) {
        _lock_acquire(&lvgl_api_lock);
        lv_timer_handler();
        _lock_release(&lvgl_api_lock);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    example_lvgl_port_update_callback(disp);
    
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    assert(panel_handle != NULL);

    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // because SPI LCD is big-endian, we need to swap the RGB bytes order
    lv_draw_sw_rgb565_swap(px_map, (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);

    // Notify LVGL that the flushing is done
    lv_display_flush_ready(disp);
}

char volt_str[15];
void lvgl_ui(lv_display_t *disp)
{
    
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    // Background Image
    lv_obj_t *bg_image = lv_img_create(scr);
    lv_img_set_src(bg_image, &radial_bg);   //Set the image source
    lv_obj_align(bg_image, LV_ALIGN_CENTER, 0, 0);  //Align the image to the center
    lv_obj_clear_flag(bg_image, LV_OBJ_FLAG_SCROLLABLE);   //Remove the scroll bar

    // Destra Logo
    lv_obj_t *logo_image = lv_img_create(scr);
    lv_img_set_src(logo_image, &destralogomain);  // Set the source to the converted image data
    lv_obj_align(logo_image, LV_ALIGN_TOP_MID, 0, 40);     
    lv_img_set_zoom(logo_image, 128);           // Set the zoom to 50% (128 out of 256)
    lv_obj_align(logo_image, LV_ALIGN_TOP_MID, 0, 20);

    //Connected devices label
    connected_devices_label = lv_label_create(scr);
    lv_obj_set_style_text_color(connected_devices_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN); // Orange-red color for better emphasis
    lv_obj_set_style_text_font(connected_devices_label, &lv_font_montserrat_48, LV_PART_MAIN); // Larger font size
    lv_label_set_text(connected_devices_label, "0");
    lv_obj_align(connected_devices_label, LV_ALIGN_CENTER, 0, -20);

    // "Connected devices" text label
    lv_obj_t *device_count_label;
    device_count_label = lv_label_create(scr);
    lv_label_set_text(device_count_label, "Device Connected");
    lv_obj_set_style_text_color(device_count_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN); // Red color for "Device Connected"
    lv_obj_set_style_text_font(device_count_label, &lv_font_montserrat_14, LV_PART_MAIN); // Smaller font
    lv_obj_align(device_count_label, LV_ALIGN_CENTER, 0, 10);  // Position below count

    // Wifi Icon
    wifi_icon = lv_label_create(scr);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x0000FF), LV_PART_MAIN); // Blue color for Wi-Fi
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(wifi_icon, LV_ALIGN_BOTTOM_MID, -75, -55);

    // Hotspot Icon
    hotspot_icon = lv_img_create(scr);
    lv_img_set_src(hotspot_icon, &blue_hotspot);
    lv_obj_align_to(hotspot_icon, wifi_icon, LV_ALIGN_OUT_RIGHT_MID, 25, 0);
    lv_obj_add_flag(hotspot_icon, LV_OBJ_FLAG_HIDDEN);  // Initially hidden

    // Arrow Icon
    lv_obj_t *arrow_icon = lv_img_create(scr);
    lv_img_set_src(arrow_icon, &arrow_img);  // Set the image source to the arrow icon
    lv_obj_align_to(arrow_icon, wifi_icon, LV_ALIGN_OUT_RIGHT_MID, 75, 0);  // Position next to Wi-Fi icon
    
    // Battery Icon
    battery_icon = lv_label_create(scr);
    lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_FULL);  // Default battery symbol
    lv_obj_set_style_text_color(battery_icon, lv_color_hex(0x00FF00), LV_PART_MAIN);  // Green for battery
    lv_obj_set_style_text_font(battery_icon, &lv_font_montserrat_20, LV_PART_MAIN);   // Font for battery icon
    lv_obj_align(battery_icon, LV_ALIGN_BOTTOM_MID, 75, -55);  // Position bottom-right

    // To remove the scrollbar from the screen
    lv_obj_clear_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLLABLE);

  /*//ADC label (For testing purpose only)
     battery_label = lv_label_create(scr);
     lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
     lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, LV_PART_MAIN);
     lv_label_set_text(battery_label, "Mallesh:" );  // Update with new value
     lv_obj_align_to(battery_label, connected_devices_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);  // Position below connected devices label
   */
  /*
  //ADC label (For testing purpose only)
battery_label = lv_label_create(scr);
lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, LV_PART_MAIN);
*/
/*battery_label = lv_label_create(scr);
lv_obj_set_style_text_color(battery_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, LV_PART_MAIN);
lv_label_set_text(battery_label, "Voltage: -- V");  // Initial text
lv_obj_align_to(battery_label, battery_icon, LV_ALIGN_OUT_TOP_MID, 0, -10);  // Position it above the battery icon
*/

    // Timer to update the connected devices label
    lv_timer_create(update_connected_devices, 1000, NULL);  // Update every 1 second

    // Create the timer to update the WiFi icon every 2 seconds
    lv_timer_create(update_wifi_icon_cb, 2000, NULL);
    
    // Timer to update the battery label
    lv_timer_create(update_battery_label, 5000, NULL);  // Call every 5 seconds

    // Timer to monitor VBUS sense for charger connectivity
    lv_timer_create(monitor_vbus_sense, 1000, NULL);  // Poll every 500ms
    
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
static void example_lvgl_port_update_callback(lv_display_t *disp)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    lv_display_rotation_t rotation = lv_display_get_rotation(disp);

    switch (rotation) {
    case LV_DISPLAY_ROTATION_0:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
        break;
    case LV_DISPLAY_ROTATION_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
        break;
    case LV_DISPLAY_ROTATION_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISPLAY_ROTATION_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    }
}

void update_connected_devices(lv_timer_t *timer)
{
    int connected_devices = getConnectCount();  // Get the current number of connected devices

    // Only update the label if the number of devices has changed
    if (connected_devices != prev_connected_devices) {
        // Create a buffer to hold the updated text
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", connected_devices);  // Update the label text with the new count
        lv_label_set_text(connected_devices_label, buf);  // Update the label

        // Store the current value as the previous value for comparison next time
        prev_connected_devices = connected_devices;

        if (connected_devices > 0) {
            // Show the hotspot icon if there are connected devices
            lv_obj_clear_flag(hotspot_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            // Hide the hotspot icon if there are no connected devices
            lv_obj_add_flag(hotspot_icon, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
/*Original from Manohar*/
// original one 
/*
void update_battery_label(lv_timer_t *timer)
{

    // Enable ADC sensing
    gpio_set_level(ADC_SENSE_EN_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));  // Small delay for stabilization

    // Perform ADC reading
    int adc_raw = adc1_get_raw(BATTERY_ADC_CHANNEL);
    

   float voltage_global =((adc_raw*3.3)/4095)*2.07;
    
    // Disable ADC sensing
    gpio_set_level(ADC_SENSE_EN_GPIO, 0);

    // Define battery level thresholds based on ADC values
    const int BATTERY_FULL_THRESHOLD = 1362;    // ~4.07V
    const int BATTERY_HALF_THRESHOLD = 1287;    // ~3.66V
    
    
   // sprintf(volt_str, "volts: %0.2f V", voltage_global);  // Convert integer to string with "V" suffix

   ESP_LOGI(TAG, "adc_raw %d, vlotage %.2f", adc_raw, voltage_global);

   // Update the volt_str variable with the latest voltage value
   snprintf(volt_str, sizeof(volt_str), "volts: %.2f V", voltage_global);

    // Update battery icon and color based on ADC value
    if (adc_raw >= BATTERY_FULL_THRESHOLD) {
        // Full battery (Green)
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_FULL);
        lv_obj_set_style_text_color(battery_icon, lv_color_hex(0x00FF00), LV_PART_MAIN);
       
        //gpio_set_level(Light_Click, true);
        //gpio_set_level(LCD_BL, true);
    }
    else if (adc_raw >= BATTERY_HALF_THRESHOLD) {
        // Half battery (Yellow)
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_2);
        lv_obj_set_style_text_color(battery_icon, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    }
    else {
        // Low battery (Red)
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_EMPTY);
        lv_obj_set_style_text_color(battery_icon, lv_color_hex(0xFF0000), LV_PART_MAIN);
    //    vTaskDelay(pdMS_TO_TICKS(2000));
      //  gpio_set_level(Low_Battery_LightOff, false);
    //    gpio_set_level(LCD_BL, false);
       }
}*/


//Mallesh take it from desktop file
#define power_latch     40
 int cnt_for_batchk=0;
void update_battery_label(lv_timer_t *timer)
{
    // Enable ADC sensing
    gpio_set_level(ADC_SENSE_EN_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));  // Small delay for stabilization

    // Perform ADC reading
   
    int samples = 10;
    int total_adc = 0;
    for(int i = 0; i < samples; i++) {
        total_adc += adc1_get_raw(BATTERY_ADC_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(2));  // small delay between samples
    }
    int adc_raw = total_adc / samples;
    float voltage = (adc_raw * 3.3) / 4095;

     ESP_LOGI(TAG, "adc_raw %d, volatge %.2f", adc_raw, voltage);

   // Update the volt_str variable with the latest voltage value
   //snprintf(volt_str, sizeof(volt_str), "volts: %.2f V", voltage);
   // lv_label_set_text(battery_label, volt_str);  // Update the label

    // Update battery icon and color based on ADC value
    if(voltage >= 1.62) { // voltage = 3.8378 volt
        cnt_for_batchk=0;
          ESP_LOGI(TAG, "adc_raw:3.8\r\n");
        // Full battery (Green)
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_FULL);
        lv_obj_set_style_text_color(battery_icon, lv_color_hex(0x00FF00), LV_PART_MAIN);
       
        //gpio_set_level(Light_Click, true);
        //gpio_set_level(LCD_BL, true);
    }
   // else if (voltage >= 1.62) {  //equal to  3.8
    else if (voltage < 1.62 && voltage >= 1.54) {  //equal to  3.8378 and 3.648  (1.54/0.42211)
        // Half battery (Yellow)
        cnt_for_batchk=0;
        ESP_LOGI(TAG, "adc_raw:3.7\r\n");
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_2);
        lv_obj_set_style_text_color(battery_icon, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    }
    else if (voltage < 1.54 && voltage > 1.43) //equal 3.648
    {
        cnt_for_batchk=0;
        ESP_LOGI(TAG, "adc_raw:3.65\r\n");
        lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_EMPTY);
        lv_obj_set_style_text_color(battery_icon, lv_color_hex(0xFF0000), LV_PART_MAIN);   
    }
    else if(voltage <1.43 )// equal to 3.4
    {
        cnt_for_batchk=cnt_for_batchk+1;
        ESP_LOGI(TAG, "adc_raw:3.5  cnt:%d\r\n",cnt_for_batchk);
        if(cnt_for_batchk>2)
        {   
            cnt_for_batchk=0;
            ESP_LOGI(TAG, "power trun off\r\n");
            gpio_set_level(power_latch, 0);
        }
    } 
}

void monitor_vbus_sense(lv_timer_t *timer)
{
    static int previous_state = 0;  // Track the previous state of the pin
    int current_state = gpio_get_level(VBUS_SENSE_PIN);

    if (current_state == 1 && previous_state == 0) {
        // VBUS_SENSE_PIN has changed to 1 (charging detected)
        lv_obj_t *scr = lv_scr_act();  // Get the current active screen
        gpio_set_level(LCD_BL, 0);
        vTaskDelay(pdMS_TO_TICKS(100));  // Small delay for stabilization
        gpio_set_level(LCD_BL, 1);
        show_charging_icon(scr);

    }

    previous_state = current_state;  // Update the previous state
}

void show_charging_icon(lv_obj_t *scr)
{
    if (charging_icon == NULL) {
        // Create the charging icon
        charging_icon = lv_img_create(scr);
        lv_img_set_src(charging_icon, &charging_icon_img);  // Replace with your charging icon resource
        lv_obj_align(charging_icon, LV_ALIGN_CENTER, 0, 0);  //Align the image to the center
        lv_obj_add_flag(charging_icon, LV_OBJ_FLAG_HIDDEN);   // Initially hide it
    }

    // Show the icon
    lv_obj_clear_flag(charging_icon, LV_OBJ_FLAG_HIDDEN);

    // Create a timer to hide the icon after a short duration (e.g., 2 seconds)
    lv_timer_t *timer = lv_timer_create(hide_charging_icon_callback, 2000, NULL);
    lv_timer_set_repeat_count(timer, 1);  // Run only once
}

void hide_charging_icon_callback(lv_timer_t *timer)
{
    if (charging_icon != NULL) {
        lv_obj_add_flag(charging_icon, LV_OBJ_FLAG_HIDDEN);  // Hide the icon
    }
}

void update_wifi_icon_cb(lv_timer_t *timer)
{
    static int prev_rssi = -999;  // Initialize to a value outside the valid range for RSSI
    int rssi = get_wifi_signal_strength();

    // Check if the signal strength has changed
    if (rssi == prev_rssi) {
        return;  // No change, so do nothing
    }

    // Update the previous RSSI
    prev_rssi = rssi;

    lv_color_t wifi_color;
    if (rssi > RSSI_STRONG) {
        wifi_color = lv_color_hex(0x00FF00);  // Green for strong signal
    } else if (rssi > RSSI_GOOD) {
        wifi_color = lv_color_hex(0x0000FF);  // Blue for average signal
    } else if (rssi > RSSI_WEAK) {
        wifi_color = lv_color_hex(0xFFFF00);  // Yellow for weak signal
    } else {
        wifi_color = lv_color_hex(0xFF0000);  // Red for no/very weak signal
    }

    // Update the icon color
    lv_obj_set_style_text_color(wifi_icon, wifi_color, LV_PART_MAIN);
}
