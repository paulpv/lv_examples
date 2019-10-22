#include <lvgl.h>

#if LV_USE_LOG != 0
static void my_print(lv_log_level_t level, const char * file, uint32_t line, const char * dsc)
{
  Serial.printf("%s@%d->%s\n", file, line, dsc);
}
#endif

void testRgb16to32to16() {
  lv_color_t    color16Expected;
  lv_color32_t  color32;
  lv_color16_t  color16Actual;
  for(uint32_t c = 0; c < 0xFFFF; c++) {
    color16Expected.full = c;
    color32.full = lv_color_to32(color16Expected);
    color16Actual = lv_color_make(color32.ch.red, color32.ch.green, color32.ch.blue);
    if(color16Actual.full != color16Expected.full){
      Serial.printf("testRgb16to32to16: FAIL! Color16 : Before 0x%04X --> After 0x%04X\n", color16Expected.full, color16Actual.full);
      return;
    }
    if( c % 1000 == 0) {
      Serial.printf("testRgb16to32to16: OK : 0x%04X\n", c);
    }
  }
  Serial.println("testRgb16to32to16: PASS!");
}

typedef struct {
  lv_color_t rgb;
  lv_color_hsv_t hsv;
} test_rgb_to_hsv_t;

bool isClose(uint8_t a, uint8_t b, uint8_t d) {
  return abs(a - b) <= d;
}

void testRgbToHsvtoRgb() {
  /*
   * https://en.wikipedia.org/wiki/HSL_and_HSV#Examples
   */
  test_rgb_to_hsv_t expected[] = {     //  H, Shsv,   V      // https://www.htmlcsscolor.com
    { LV_COLOR_MAKE(0xFF, 0xFF, 0xFF), {   0,    0, 100 } }, // LV_COLOR_WHITE
    { LV_COLOR_MAKE(0x80, 0x80, 0x80), {   0,    0,  50 } }, // LV_COLOR_GRAY
    { LV_COLOR_MAKE(0x00, 0x00, 0x00), {   0,    0,   0 } }, // LV_COLOR_BLACK
    { LV_COLOR_MAKE(0xFF, 0x00, 0x00), {   0,  100, 100 } }, // LV_COLOR_RED
    { LV_COLOR_MAKE(0xBF, 0xBF, 0x00), {  60,  100,  75 } }, // "La Rioja"
    { LV_COLOR_MAKE(0x00, 0x80, 0x00), { 120,  100,  50 } }, // LV_COLOR_GREEN
    { LV_COLOR_MAKE(0x80, 0xFF, 0xFF), { 180,   50, 100 } }, // "Electric Blue"
    { LV_COLOR_MAKE(0x80, 0x80, 0xFF), { 240,   50, 100 } }, // "Light Slate Blue"
    { LV_COLOR_MAKE(0xBF, 0x40, 0xBF), { 300,   67,  75 } }, // "Fuchsia"
    { LV_COLOR_MAKE(0xA0, 0xA4, 0x24), {  62,   78,  64 } }, // "Lucky"
    { LV_COLOR_MAKE(0x41, 0x1B, 0xEA), { 251,   89,  92 } }, // "Han Purple"
    { LV_COLOR_MAKE(0x1E, 0xAC, 0x41), { 135,   83,  68 } }, // "Dark Pastel Green"
    { LV_COLOR_MAKE(0xF0, 0xC8, 0x0E), {  50,   94,  94 } }, // "Moon Yellow"
    { LV_COLOR_MAKE(0xB4, 0x30, 0xE5), { 284,   79,  90 } }, // "Dark Orchid"
    { LV_COLOR_MAKE(0xED, 0x76, 0x51), {  14,   66,  93 } }, // "Burnt Sienna"
    { LV_COLOR_MAKE(0xFE, 0xF8, 0x88), {  57,   47, 100 } }, // "Milan"
    { LV_COLOR_MAKE(0x19, 0xCB, 0x97), { 162,   88,  80 } }, // "Caribbean Green"
    { LV_COLOR_MAKE(0x36, 0x26, 0x98), { 248,   75,  60 } }, // "Dark Slate Blue"
    { LV_COLOR_MAKE(0x7E, 0x7E, 0xB8), { 241,   32,  72 } }, // "Moody Blue"
  };
  int test_count = sizeof(expected) / sizeof(expected[0]);
  for(int i=0; i < test_count; i++) {
    test_rgb_to_hsv_t rgb_to_hsv = expected[i];
    lv_color_t colorRgbExpected = rgb_to_hsv.rgb;
    lv_color_hsv_t colorHsvExpected = rgb_to_hsv.hsv;
    Serial.printf("testRgbToHsvtoRgb: colorRgbExpected=0x%08X, colorHsvExpected={ %d, %d, %d }\n", colorRgbExpected, colorHsvExpected.h, colorHsvExpected.s, colorHsvExpected.v);

    //
    // RGB to HSV
    //
    lv_color_hsv_t colorHsvActual = lv_color_to_hsv(colorRgbExpected);
    Serial.printf("testRgbToHsvtoRgb: colorHsvActual={ %d, %d, %d }\n", colorHsvActual.h, colorHsvActual.s, colorHsvActual.v);
    if (!isClose(colorHsvActual.h, colorHsvExpected.h, 3) || 
        !isClose(colorHsvActual.s, colorHsvExpected.s, 3) || 
        !isClose(colorHsvActual.v, colorHsvExpected.v, 3)) {
      Serial.println("testRgbToHsvtoRgb: FAIL!");
      break;
    }

    //
    // HSV to RGB
    //
    lv_color_t colorRgbActual = lv_color_hsv_to_rgb(colorHsvExpected.h, colorHsvExpected.s, colorHsvExpected.v);
    Serial.printf("testRgbToHsvtoRgb: colorRgbActual=0x%08X\n", colorRgbActual);
    if (!isClose(colorRgbActual.ch.red, colorRgbExpected.ch.red, 3) || 
        !isClose(colorRgbActual.ch.green, colorRgbExpected.ch.green, 3) || 
        !isClose(colorRgbActual.ch.blue, colorRgbExpected.ch.blue, 3)) {
      Serial.println("testRgbToHsvtoRgb: FAIL!");
      break;
    }
  }
  Serial.println("testRgbToHsvtoRgb: PASS!");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nsetup: littlevgl color test");
  lv_init();
#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print);
#endif

  testRgb16to32to16();
  testRgbToHsvtoRgb();

  Serial.println("setup: ALL TESTS DONE!");
}

void loop() {}
