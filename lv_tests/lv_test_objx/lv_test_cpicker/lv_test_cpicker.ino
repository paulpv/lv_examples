//#define LV_CONF_INCLUDE_SIMPLE 1
#include <lvgl.h>
#include <Ticker.h>

#define LVGL_TICK_PERIOD 20

Ticker tick;
static lv_disp_buf_t disp_buf;
static lv_color_t buf_1[LV_HOR_RES_MAX * 10];
static lv_color_t buf_2[LV_HOR_RES_MAX * 10];

#include <Adafruit_ILI9341.h> // Display Driver https://github.com/adafruit/Adafruit_ILI9341

// TinyPICO https://www.tinypico.com/gettingstarted
#define TFT_CS 5
#define TFT_DC 26
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_CLK 18
#define TFT_RST 25
#define TFT_LITE 32

Adafruit_ILI9341 display = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

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

int my_disp_flush_x, my_disp_flush_y;
uint16_t my_disp_flush_color;

void my_disp_flush(lv_disp_drv_t * disp, const lv_area_t * area, lv_color_t * color_p) {
  display.startWrite();
  display.setAddrWindow(area->x1, area->y1, (area->x2 - area->x1 + 1), (area->y2 - area->y1 + 1));
  for (my_disp_flush_y = area->y1; my_disp_flush_y <= area->y2; my_disp_flush_y++) {
    for (my_disp_flush_x = area->x1; my_disp_flush_x <= area->x2; my_disp_flush_x++) {
      my_disp_flush_color = color_p->full;
      display.writeColor(my_disp_flush_color, 1);
      color_p++;
    }
  }
  display.endWrite();
  lv_disp_flush_ready(disp);
}

TS_Point touchPointCurrent;
TS_Point touchPointPrevious;
bool isTouched;

bool my_input_read(lv_indev_drv_t * drv, lv_indev_data_t* data) {
  data->point.x = isTouched ? touchPointCurrent.x : touchPointPrevious.x;
  data->point.y = isTouched ? touchPointCurrent.y : touchPointPrevious.y;
  data->state = isTouched ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
  return false; // No buffering now so no more data read
}

#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char * file, uint32_t line, const char * dsc)
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

static void cpicker_event_handler(lv_obj_t * cpicker, lv_event_t event) {
  if (event == LV_EVENT_VALUE_CHANGED) {    
    lv_cpicker_color_mode_t color_mode = lv_cpicker_get_color_mode(cpicker);
    switch(color_mode) {
      case LV_CPICKER_COLOR_MODE_HUE: {
        uint16_t hue = lv_cpicker_get_hue(cpicker);
        Serial.printf("cpicker_event_handler: LV_EVENT_VALUE_CHANGED hue=%d\r\n", hue);
        break;
      }
      case LV_CPICKER_COLOR_MODE_SATURATION: {
        uint8_t saturation = lv_cpicker_get_saturation(cpicker);
        Serial.printf("cpicker_event_handler: LV_EVENT_VALUE_CHANGED saturation=%d\r\n", saturation);
        break;
      }
      case LV_CPICKER_COLOR_MODE_VALUE: {
        uint8_t value = lv_cpicker_get_value(cpicker);
        Serial.printf("cpicker_event_handler: LV_EVENT_VALUE_CHANGED value=%d\r\n", value);
        break;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("setup: littlevgl cpicker test");

  display.begin();
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

  lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = displayWidth;
  disp_drv.ver_res = displayHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.buffer = &disp_buf;
  lv_disp_t * disp = lv_disp_drv_register(&disp_drv);

  lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_input_read;
  lv_indev_drv_register(&indev_drv);

  tick.attach_ms(LVGL_TICK_PERIOD, lv_tick_handler);

  lv_obj_t *label = lv_label_create(lv_scr_act(), NULL);
  lv_label_set_text(label, "Hello LittlevGL v6.x");
  lv_obj_align(label, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);

  //lv_test_cpicker_1();

  const uint32_t dispWidth = lv_disp_get_hor_res(disp);
  const uint32_t dispHeight = lv_disp_get_ver_res(disp);
  const uint32_t pickerSize = (uint32_t)round(LV_MATH_MIN(dispWidth, dispHeight) * 0.8);
  lv_obj_t * scr = lv_scr_act();

  static lv_style_t styleMain;
  lv_style_copy(&styleMain, &lv_style_plain);
  styleMain.line.width = pickerSize * 0.2;

  static lv_style_t styleIndicator;
  lv_style_copy(&styleIndicator, &lv_style_plain);
  styleIndicator.body.main_color = LV_COLOR_WHITE;
  styleIndicator.body.grad_color = styleIndicator.body.main_color;
  styleIndicator.body.opa = LV_OPA_80;

  lv_obj_t * picker = lv_cpicker_create(scr, NULL);
  lv_cpicker_set_style(picker, LV_CPICKER_STYLE_MAIN, &styleMain);
  lv_cpicker_set_style(picker, LV_CPICKER_STYLE_INDICATOR, &styleIndicator);
  lv_obj_set_event_cb(picker, cpicker_event_handler);
  lv_obj_set_size(picker, pickerSize, pickerSize);
  lv_obj_align(picker, NULL, LV_ALIGN_CENTER, 0, 0);

#if true
  lv_cpicker_set_color(picker, LV_COLOR_CYAN);
  lv_scr_load(scr);
#else
  lv_scr_load(scr);

  lv_anim_t a;
  a.var = picker;
  a.start = 0;
  a.end = 359;
  a.exec_cb = (lv_anim_exec_xcb_t) lv_cpicker_set_hue;
  a.path_cb = lv_anim_path_linear;
  a.act_time = 0;
  a.time = 5000;
  a.playback = 0;
  a.playback_pause = 0;
  a.repeat = 1;
  a.repeat_pause = 1000;
  lv_anim_create(&a);
#endif
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
