#include <lvgl.h>
#include <Ticker.h>

#define LVGL_TICK_PERIOD 20

Ticker tick;
static lv_disp_buf_t disp_buf;
static lv_color_t buf_1[LV_HOR_RES_MAX * 10];
static lv_color_t buf_2[LV_HOR_RES_MAX * 10];

//
//
//

// TinyPICO https://www.tinypico.com/gettingstarted
#define TFT_CS 5
#define TFT_DC 26
#define TFT_RST 25
//#define TFT_MOSI 23
//#define TFT_MISO 19
//#define TFT_SCLK 18
#define TFT_LITE 32

// TODO:(pv) Look in to seeing if TinyPICO (esp32-pico-d4) supports 80MHz SPI!
//  https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/spi_master.html#gpio-matrix-and-iomux
//  https://www.espressif.com/sites/default/files/documentation/esp32-pico-d4_datasheet_en.pdf
//  https://blog.littlevgl.com/2019-01-31/esp32
//    "The maximum clock rate is still 40 MHz because of ILI9341 but hopefully, it will give better signals."

/**
 * Adafruit "2.8" TFT LCD with Cap Touch Breakout Board w/MicroSD Socket"
 * https://www.adafruit.com/product/2090
 */
// https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Adafruit_GFX.h
// https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Adafruit_SPITFT.h
#include <Adafruit_ILI9341.h> // Display Driver https://github.com/adafruit/Adafruit_ILI9341
// NOTE: ESP32 can't use https://github.com/PaulStoffregen/ILI9341_t3/ per https://github.com/PaulStoffregen/ILI9341_t3/issues/37

// Using faster Hardware SPI (HWSPI); Providing MISO/MOSI/CLK results in slower Software SPI (SWSPI)
Adafruit_ILI9341 display = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

#include <Adafruit_FT6206.h> // Capacitive Touch Driver https://github.com/adafruit/Adafruit_FT6206_Library

//
// 0,1,2,3 == 0,90,180,270 degrees respectively
//
#define DISPLAY_ROTATION 1

// Default is FT62XX_DEFAULT_THRESHOLD but you can try changing it if your screen is too/not sensitive.
#define TOUCH_THRESHOLD 40

// NOTE: Display orientation/rotation may be set,
// but *TOUCH* orientation/rotation is **fixed** to 2 (180)
int displayWidth;
int displayHeight;
int touchWidth;
int touchHeight;

// The FT6206 uses hardware I2C (SCL/SDA)
Adafruit_FT6206 touch = Adafruit_FT6206();

void matchTouchRotationToDisplayRotation(TS_Point& touchPoint) {
  switch (DISPLAY_ROTATION) {
    case 0: {
        // Display set to 0 degrees, Touch fixed at 180 degrees
        touchPoint.x = touchWidth - touchPoint.x;
        touchPoint.y = touchHeight - touchPoint.y;
        break;
      }
    case 1: {
        // Display set to 90 degrees, Touch fixed at 180 degrees
        int x = touchHeight - touchPoint.y;
        int y = displayHeight - (touchWidth - touchPoint.x);
        touchPoint.x = x;
        touchPoint.y = y;
        break;
      }
    case 2: {
        // Display set to 180 degrees, Touch fixed at 180 degrees
        // no-op
        break;
      }
    case 3: {
        // Display set to 270 degrees, Touch fixed at 180 degrees
        int x = touchPoint.y;
        int y = displayHeight - touchPoint.x;
        touchPoint.x = x;
        touchPoint.y = y;
        break;
      }
  }
}

//
//
//

static void lv_tick_handler(void) {
  lv_tick_inc(LVGL_TICK_PERIOD);
}

int my_disp_flush_width, my_disp_flush_height;

static void my_disp_flush(lv_disp_drv_t * disp, const lv_area_t * area, lv_color_t * color_p) {
  my_disp_flush_width = area->x2 - area->x1 + 1;
  my_disp_flush_height = area->y2 - area->y1 + 1;
  display.startWrite();
  display.setAddrWindow(area->x1, area->y1, my_disp_flush_width, my_disp_flush_height);
  while ((my_disp_flush_height--) > 0) {
    display.writePixels((uint16_t*)color_p, my_disp_flush_width, false);
    color_p += my_disp_flush_width;
  }
  display.endWrite();
  lv_disp_flush_ready(disp);
}

TS_Point touchPointCurrent, touchPointPrevious;
bool isTouched;
const long displayActiveTimeoutMillis = 30000;
unsigned long lastInteractionMillis = millis();

bool touchUpdate() {
  if (touch.touched()) {
    touchPointCurrent = touch.getPoint();
    // NOTE:(pv) Leave this uncommented until I solve why setBrightness is being passed a negative #...
    //Serial.printf("touchUpdate: ORIGINAL touchPointCurrent=(%d, %d, %d)\n", touchPointCurrent.x, touchPointCurrent.y, touchPointCurrent.z);
    if (touchPointCurrent.x == 0 && touchPointCurrent.y == 0 && touchPointCurrent.z == 0) {
      // Ignore occasional "false" 0,0,0 presses
      touchPointCurrent.x = touchPointCurrent.y = touchPointCurrent.z = -1;
    } else {
      matchTouchRotationToDisplayRotation(touchPointCurrent);
      // NOTE:(pv) Leave this uncommented until I solve why setBrightness is being passed a negative #...
      //Serial.printf("touchUpdate:  ROTATED touchPointCurrent=(%d, %d, %d)\n", touchPointCurrent.x, touchPointCurrent.y, touchPointCurrent.z);

      lastInteractionMillis = millis();
    }
  } else {
    touchPointCurrent.x = touchPointCurrent.y = touchPointCurrent.z = -1;
  }

  isTouched = touchPointCurrent.x != -1 && touchPointCurrent.y != -1 && touchPointCurrent.z != -1;

  if (isTouched) {
    touchPointPrevious = touchPointCurrent;
  }
}

static bool my_input_read(lv_indev_drv_t * drv, lv_indev_data_t* data) {
  data->point.x = isTouched ? touchPointCurrent.x : touchPointPrevious.x;
  data->point.y = isTouched ? touchPointCurrent.y : touchPointPrevious.y;
  data->state = isTouched ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
  return false; // No buffering now so no more data read
}

#if LV_USE_LOG != 0
static void my_print(lv_log_level_t level, const char * file, uint32_t line, const char * dsc)
{
  Serial.printf("%s@%d->%s\n", file, line, dsc);
  //delay(100);
}
#endif

//
//
//

// Backlight PWM properties 12kHz 8-bit PWM
#define BACKLIGHT_CHANNEL 0
#define BACKLIGHT_FREQUENCY 12000
#define BACKLIGHT_RESOLUTION_BITS 8

#define BACKLIGHT_INACTIVE_PERCENT 0.05

float backlightPercent = 0.8;

lv_obj_t * sliderBrightness;

void setupDisplayBrightness() {
  pinMode(TFT_LITE, OUTPUT);
  ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQUENCY, BACKLIGHT_RESOLUTION_BITS);
  ledcAttachPin(TFT_LITE, BACKLIGHT_CHANNEL);
  setDisplayBrightness(backlightPercent, true, false);
}

static void sliderBrightness_event_handler(lv_obj_t * slider, lv_event_t event) {
  if (event != LV_EVENT_VALUE_CHANGED) return;
  uint16_t value = lv_slider_get_value(slider);
  //Serial.printf("sliderBrightness_event_handler: value=%d\n", value);
  setDisplayBrightness(value * 0.01, true, false);
}

void setDisplayBrightness(float percent, bool remember, bool updateUi) {
  Serial.printf("setDisplayBrightness(%f, %d, %d)\n", percent, remember, updateUi);
  if (percent < 0) {
    percent = 0;
  }
  if (percent > 1) {
    percent = 1;
  }

  int dutyCycle = (int) round(255 * percent);
  //Serial.printf("setDisplayBrightness: dutyCycle=%d\n", dutyCycle);
  ledcWrite(BACKLIGHT_CHANNEL, dutyCycle);

  if (remember) {
    backlightPercent = percent;
  }

  if (updateUi && sliderBrightness != NULL) {
    lv_slider_set_value(sliderBrightness, round(percent * 100), LV_ANIM_OFF);
  }
}

//
//
//

static lv_disp_drv_t disp_drv;
static lv_disp_t * disp;
static uint32_t time_sum = 0;
static uint32_t refr_cnt = 0;
static lv_obj_t * result_label;

static void refr_monitor(lv_disp_drv_t * disp_drv, uint32_t time_ms, uint32_t px_num) {
  time_sum += time_ms;
  refr_cnt++;
  //Serial.printf("refr_monitor: refr_cnt=%d\n", refr_cnt);
  float time_avg = time_sum / (float) refr_cnt;
  float fps = 1000 / time_avg;
  char buf[256];
  sprintf(buf, "FPS ~ %03.02f", fps);
  lv_label_set_text(result_label, buf);

  if (refr_cnt >= 10) {
    disp_drv->monitor_cb = NULL;
  }

  lv_obj_invalidate(lv_disp_get_scr_act(disp));
}

static void buttonTest_event_handler(lv_obj_t * button, lv_event_t event) {
  //Serial.printf("buttonTest_event_handler: event=%d\n", event);
  if (event != LV_EVENT_CLICKED) return;

  Serial.println("buttonTest_event_handler: event=LV_EVENT_CLICKED");

  if (disp->driver.monitor_cb != NULL) return;

  time_sum = 0;
  refr_cnt = 0;
  disp->driver.monitor_cb = refr_monitor;
  lv_obj_invalidate(lv_disp_get_scr_act(disp));
}

lv_obj_t * buttonInstantiate_label = NULL;
lv_obj_t * colorPicker = NULL;

static void buttonShow_event_handler(lv_obj_t * button, lv_event_t event) {
  //Serial.printf("buttonShow_event_handler: event=%d\n", event);
  if (event != LV_EVENT_CLICKED) return;

  Serial.println("buttonShow_event_handler: event=LV_EVENT_CLICKED");

  colorPickerInstantiate(colorPicker == NULL);
}

static void colorPicker_event_handler(lv_obj_t * cpicker, lv_event_t event) {
  if (event != LV_EVENT_VALUE_CHANGED) return;
  lv_cpicker_color_mode_t color_mode = lv_cpicker_get_color_mode(cpicker);
  switch (color_mode) {
    case LV_CPICKER_COLOR_MODE_HUE: {
        uint16_t hue = lv_cpicker_get_hue(cpicker);
        Serial.printf("colorPicker_event_handler: LV_EVENT_VALUE_CHANGED hue=%d\n", hue);
        break;
      }
    case LV_CPICKER_COLOR_MODE_SATURATION: {
        uint8_t saturation = lv_cpicker_get_saturation(cpicker);
        Serial.printf("colorPicker_event_handler: LV_EVENT_VALUE_CHANGED saturation=%d\n", saturation);
        break;
      }
    case LV_CPICKER_COLOR_MODE_VALUE: {
        uint8_t value = lv_cpicker_get_value(cpicker);
        Serial.printf("colorPicker_event_handler: LV_EVENT_VALUE_CHANGED value=%d\n", value);
        break;
      }
  }
  lv_color_t color = lv_cpicker_get_color(cpicker);
  Serial.printf("colorPicker_event_handler: LV_EVENT_VALUE_CHANGED color=0x%08X\n", color);
}

lv_obj_t * buttonCircle;
lv_obj_t * buttonIn;
lv_obj_t * buttonLine;

static void buttonIndicator_event_handler(lv_obj_t * button, lv_event_t event) {
  uint8_t indicator;
  if (button == buttonCircle) {
    indicator = LV_CPICKER_INDICATOR_CIRCLE;    
  } else if (button == buttonIn) {
    indicator = LV_CPICKER_INDICATOR_IN;    
  } else if (button == buttonLine) {
    indicator = LV_CPICKER_INDICATOR_LINE;
  }
  lv_cpicker_set_indicator_type(colorPicker, indicator);
}

void colorPickerInstantiate(bool instantiate) {
  Serial.printf("colorPickerInstantiate: instantiate=%d\n", instantiate);
  if (instantiate) {
    if (colorPicker != NULL) return;

    const uint32_t dispWidth = lv_disp_get_hor_res(disp);
    const uint32_t dispHeight = lv_disp_get_ver_res(disp);
    const uint32_t pickerSize = (uint32_t)round(LV_MATH_MIN(dispWidth, dispHeight) * 0.8);

    static lv_style_t styleMain;
    lv_style_copy(&styleMain, &lv_style_plain);
    styleMain.line.width = pickerSize * 0.2;

    static lv_style_t styleIndicator;
    lv_style_copy(&styleIndicator, &lv_style_plain);
    styleIndicator.body.main_color = LV_COLOR_WHITE;
    styleIndicator.body.grad_color = styleIndicator.body.main_color;
    styleIndicator.body.opa = LV_OPA_80;

    lv_obj_t * scr = lv_disp_get_scr_act(disp);

    colorPicker = lv_cpicker_create(scr, NULL);
#if false
    lv_obj_set_size(colorPicker, pickerSize, pickerSize);
    lv_cpicker_set_type(colorPicker, LV_CPICKER_TYPE_DISC);
#else
    lv_obj_set_size(colorPicker, pickerSize, pickerSize / 2);
    lv_cpicker_set_type(colorPicker, LV_CPICKER_TYPE_RECT);
#endif
    lv_obj_align(colorPicker, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_cpicker_set_style(colorPicker, LV_CPICKER_STYLE_MAIN, &styleMain);
    lv_cpicker_set_style(colorPicker, LV_CPICKER_STYLE_INDICATOR, &styleIndicator);
    lv_obj_set_event_cb(colorPicker, colorPicker_event_handler);

    buttonCircle = lv_btn_create(scr, NULL);
    lv_btn_set_fit(buttonCircle, LV_FIT_TIGHT);
    lv_obj_align(buttonCircle, NULL, LV_ALIGN_IN_TOP_RIGHT, 0, 0);
    lv_obj_set_event_cb(buttonCircle, buttonIndicator_event_handler);
    lv_obj_t * buttonCircle_label = lv_label_create(buttonCircle, NULL);
    lv_label_set_text(buttonCircle_label, "C");

    buttonIn = lv_btn_create(scr, NULL);
    lv_btn_set_fit(buttonIn, LV_FIT_TIGHT);
    lv_obj_align(buttonIn, NULL, LV_ALIGN_IN_RIGHT_MID, 0, 0);
    lv_obj_set_event_cb(buttonIn, buttonIndicator_event_handler);
    lv_obj_t * buttonIn_label = lv_label_create(buttonIn, NULL);
    lv_label_set_text(buttonIn_label, "I");
    
    buttonLine = lv_btn_create(scr, NULL);
    lv_btn_set_fit(buttonLine, LV_FIT_TIGHT);
    lv_obj_align(buttonLine, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_event_cb(buttonLine, buttonIndicator_event_handler);
    lv_obj_t * buttonLine_label = lv_label_create(buttonLine, NULL);
    lv_label_set_text(buttonLine_label, "L");
    
    lv_label_set_text(buttonInstantiate_label, "Hide");
  } else {
    if (colorPicker == NULL) return;
    lv_obj_del(colorPicker);
    colorPicker = NULL;

    lv_label_set_text(buttonInstantiate_label, "Show");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("setup: littlevgl color picker test");

  display.begin(); // ESP32 SPI_DEFAULT_FREQ is per ~ https://github.com/adafruit/Adafruit_ILI9341/blob/master/Adafruit_ILI9341.cpp#L66
  display.fillScreen(ILI9341_BLACK);
  display.setRotation(2);
  touchWidth = display.width();
  touchHeight = display.height();
  display.setRotation(DISPLAY_ROTATION);
  displayWidth = display.width();
  displayHeight = display.height();
  Serial.printf("setup: Display Resolution: %dx%d\n", displayWidth, displayHeight);

  if (!touch.begin(TOUCH_THRESHOLD)) {
    Serial.println("setup: Unable to start touchscreen.");
  }
  else {
    Serial.println("setup: Touchscreen started.");
  }

  setupDisplayBrightness();

  lv_init();

#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print);
#endif

  lv_theme_t * th = lv_theme_night_init(0, NULL);
  lv_theme_set_current(th);

  lv_disp_buf_init(&disp_buf, buf_1, buf_2, LV_HOR_RES_MAX * 10);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = displayWidth;
  disp_drv.ver_res = displayHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.buffer = &disp_buf;
  disp = lv_disp_drv_register(&disp_drv);

  lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_input_read;
  lv_indev_drv_register(&indev_drv);

  tick.attach_ms(LVGL_TICK_PERIOD, lv_tick_handler);

  lv_obj_t * scr = lv_scr_act();

  lv_obj_t * title = lv_label_create(scr, NULL);
  lv_label_set_text(title, "LittlevGL v6.x Color Picker");
  lv_obj_align(title, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);

  lv_obj_t * buttonInstantiate = lv_btn_create(scr, NULL);
  lv_btn_set_fit(buttonInstantiate, LV_FIT_TIGHT);
  lv_obj_align(buttonInstantiate, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
  lv_obj_set_event_cb(buttonInstantiate, buttonShow_event_handler);
  buttonInstantiate_label = lv_label_create(buttonInstantiate, NULL);
  lv_label_set_text(buttonInstantiate_label, "Show");

#if true

  colorPickerInstantiate(true);

#if true
  lv_cpicker_set_color(colorPicker, LV_COLOR_CYAN);
#else
  lv_anim_t a;
  a.var = colorPicker;
  a.start = 0;
  a.end = 359;
  a.exec_cb = (lv_anim_exec_xcb_t) lv_cpicker_set_hue;
  a.path_cb = lv_anim_path_linear;
  a.act_time = 0;
  a.time = 5000;
  a.playback = 0;
  a.playback_pause = 0;
  a.repeat = 1;
  a.repeat_pause = 0;
  lv_anim_create(&a);
#endif

#endif

  sliderBrightness = lv_slider_create(scr, NULL);
  lv_slider_set_knob_in(sliderBrightness, true);
  lv_slider_set_range(sliderBrightness, 0, 100);
  lv_slider_set_value(sliderBrightness, backlightPercent * 100, LV_ANIM_OFF);
  lv_obj_set_size(sliderBrightness, 25, displayHeight);
  lv_obj_set_event_cb(sliderBrightness, sliderBrightness_event_handler);
  lv_obj_align(sliderBrightness, NULL, LV_ALIGN_IN_RIGHT_MID, 0, 0);

  lv_obj_t * buttonTest = lv_btn_create(scr, NULL);
  lv_btn_set_fit(buttonTest, LV_FIT_TIGHT);
  lv_obj_align(buttonTest, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_event_cb(buttonTest, buttonTest_event_handler);
  lv_obj_t * buttonTest_label = lv_label_create(buttonTest, NULL);
  lv_label_set_text(buttonTest_label, "Test");

  result_label = lv_label_create(scr, NULL);
  lv_label_set_text(result_label, "FPS ~ ?");
  lv_obj_align(result_label, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);

  lv_scr_load(scr);
}

bool displayActiveCurrent, displayActivePrevious;

void loop() {
  touchUpdate();

  displayActiveCurrent = millis() - lastInteractionMillis < displayActiveTimeoutMillis;
  //Serial.printf("loop: displayActiveCurrent=%d\n", displayActiveCurrent);
  if (displayActiveCurrent) {
    if (!displayActivePrevious) {
      Serial.println("Activity timeout reset");
      if (backlightPercent > BACKLIGHT_INACTIVE_PERCENT) {
        Serial.println("Display brighten");
        setDisplayBrightness(backlightPercent, false, true);
      }
    }
  } else {
    if (displayActivePrevious) {
      Serial.println("Activity timeout elapsed");
      if (backlightPercent > BACKLIGHT_INACTIVE_PERCENT) {
        Serial.println("Display dim");
        setDisplayBrightness(BACKLIGHT_INACTIVE_PERCENT, false, true);
      }
    }
  }
  displayActivePrevious = displayActiveCurrent;

  lv_task_handler();
  delay(5);
}
