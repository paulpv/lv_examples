//#define LV_CONF_INCLUDE_SIMPLE 1
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
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_CLK 18
#define TFT_LITE 32

// NOTE: ESP32 can't use https://github.com/PaulStoffregen/ILI9341_t3/ per https://github.com/PaulStoffregen/ILI9341_t3/issues/37
// https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Adafruit_GFX.h
// https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Adafruit_SPITFT.h
#include <Adafruit_ILI9341.h> // Display Driver https://github.com/adafruit/Adafruit_ILI9341

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

TS_Point touchPointCurrent;
TS_Point touchPointPrevious;
bool isTouched;

static bool my_input_read(lv_indev_drv_t * drv, lv_indev_data_t* data) {
  data->point.x = isTouched ? touchPointCurrent.x : touchPointPrevious.x;
  data->point.y = isTouched ? touchPointCurrent.y : touchPointPrevious.y;
  data->state = isTouched ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
  return false; // No buffering now so no more data read
}

#if LV_USE_LOG != 0
static void my_print(lv_log_level_t level, const char * file, uint32_t line, const char * dsc)
{
  Serial.printf("%s@%d->%s\r\n", file, line, dsc);
  //delay(100);
}
#endif

//
//
//

#define DISPLAY_COLOR_TEXT ILI9341_WHITE
#define DISPLAY_COLOR_BACKGROUND ILI9341_BLACK

// Default font is 6x8
#define DISPLAY_FONT_HEIGHT 8
#define DISPLAY_FONT_ORIGIN_Y 0
#define DISPLAY_FONT_SCALE 2

// Backlight PWM properties 12kHz 8-bit PWM
#define BACKLIGHT_CHANNEL 0
#define BACKLIGHT_FREQUENCY 12000
#define BACKLIGHT_RESOLUTION_BITS 8

#define BACKLIGHT_CONTROL_COLOR_FOREGROUND ILI9341_MAGENTA
#define BACKLIGHT_CONTROL_COLOR_BACKGROUND ILI9341_BLACK
#define BACKLIGHT_CONTROL_HEIGHT (DISPLAY_FONT_HEIGHT + 2)

#define BACKLIGHT_INACTIVE_PERCENT 0.05

float backlightPercent = 0.2;

void setupDisplayBrightness() {
  pinMode(TFT_LITE, OUTPUT);
  ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQUENCY, BACKLIGHT_RESOLUTION_BITS);
  ledcAttachPin(TFT_LITE, BACKLIGHT_CHANNEL);
  setDisplayBrightness(backlightPercent, true);
}

void setDisplayBrightness(float value, bool interactive) {
  Serial.println("setDisplayBrightness(" + String(value) + ", " + String(interactive) + ")");
  if (value < 0) {
    Serial.println("setDisplayBrightness: !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    value = 0;
  }
  if (value > 1) {
    value = 1;
  }
  int dutyCycle = (int) round(255 * value);
  //Serial.println("setDisplayBrightness: dutyCycle=" + String(dutyCycle));
  ledcWrite(BACKLIGHT_CHANNEL, dutyCycle);

  if (interactive) {
    backlightPercent = value;
  }

  int brightnessWidth = (int) round(displayWidth * value);
  //Serial.println("setDisplayBrightness: brightnessWidth=" + String(brightnessWidth));
  display.fillRect(0, displayHeight - BACKLIGHT_CONTROL_HEIGHT, brightnessWidth, BACKLIGHT_CONTROL_HEIGHT, BACKLIGHT_CONTROL_COLOR_FOREGROUND);
  display.fillRect(brightnessWidth, displayHeight - BACKLIGHT_CONTROL_HEIGHT, displayWidth, BACKLIGHT_CONTROL_HEIGHT, BACKLIGHT_CONTROL_COLOR_BACKGROUND);
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
  if (event == LV_EVENT_VALUE_CHANGED) {
    lv_cpicker_color_mode_t color_mode = lv_cpicker_get_color_mode(cpicker);
    switch (color_mode) {
      case LV_CPICKER_COLOR_MODE_HUE: {
          uint16_t hue = lv_cpicker_get_hue(cpicker);
          Serial.printf("colorPicker_event_handler: LV_EVENT_VALUE_CHANGED hue=%d\r\n", hue);
          break;
        }
      case LV_CPICKER_COLOR_MODE_SATURATION: {
          uint8_t saturation = lv_cpicker_get_saturation(cpicker);
          Serial.printf("colorPicker_event_handler: LV_EVENT_VALUE_CHANGED saturation=%d\r\n", saturation);
          break;
        }
      case LV_CPICKER_COLOR_MODE_VALUE: {
          uint8_t value = lv_cpicker_get_value(cpicker);
          Serial.printf("colorPicker_event_handler: LV_EVENT_VALUE_CHANGED value=%d\r\n", value);
          break;
        }
    }
    lv_color_t color = lv_cpicker_get_color(cpicker);
    Serial.printf("colorPicker_event_handler: LV_EVENT_VALUE_CHANGED color=0x%08X\r\n", color);
  }
}

void colorPickerInstantiate(bool instantiate) {
  Serial.printf("colorPickerInstantiate: instantiate=%d", instantiate);
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
    lv_cpicker_set_style(colorPicker, LV_CPICKER_STYLE_MAIN, &styleMain);
    lv_cpicker_set_style(colorPicker, LV_CPICKER_STYLE_INDICATOR, &styleIndicator);
    lv_obj_set_event_cb(colorPicker, colorPicker_event_handler);
    lv_obj_set_size(colorPicker, pickerSize, pickerSize);
    lv_obj_align(colorPicker, NULL, LV_ALIGN_CENTER, 0, 0);

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
  Serial.println("setup: Display Resolution: " + String(displayWidth) + "x" + String(displayHeight));

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

void loop() {
  if (touch.touched()) {
    touchPointCurrent = touch.getPoint();
    //touchText = "touchPoint=(" + String(touchPoint.x) + "," + String(touchPoint.y) + "," + String(touchPoint.z) + ")";
    //Serial.println("loop: ORIGINAL " + touchText);
    if (touchPointCurrent.x == 0 && touchPointCurrent.y == 0 && touchPointCurrent.z == 0) {
      // Ignore occasional "false" 0,0,0 presses
      touchPointCurrent.x = touchPointCurrent.y = touchPointCurrent.z = -1;
    } else {
      matchTouchRotationToDisplayRotation(touchPointCurrent);
      // NOTE:(pv) Leave this uncommented until I solve why setBrightness is being passed a negative %...
      //touchText = "touchPoint=(" + String(touchPoint.x) + "," + String(touchPoint.y) + "," + String(touchPoint.z) + ")";
      //Serial.println("loop: ROTATED " + touchText);

      //lastInteractionMillis = currentMillis;
    }
  } else {
    touchPointCurrent.x = touchPointCurrent.y = touchPointCurrent.z = -1;
  }

  isTouched = touchPointCurrent.x != -1 && touchPointCurrent.y != -1 && touchPointCurrent.z != -1;

  if (touchPointCurrent.y > displayHeight - BACKLIGHT_CONTROL_HEIGHT) {
    float widthPercent = touchPointCurrent.x / (float) displayWidth;
    setDisplayBrightness(widthPercent, true);
  }

  lv_task_handler();
  delay(5);

  if (isTouched) {
    touchPointPrevious = touchPointCurrent;
  }
}
