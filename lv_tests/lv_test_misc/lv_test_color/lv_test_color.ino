#include <lvgl.h>

#if LV_USE_LOG != 0
static void my_print(lv_log_level_t level, const char * file, uint32_t line, const char * dsc)
{
  Serial.printf("%s@%d->%s\n", file, line, dsc);
  //delay(100);
}
#endif

void testRgb16to32() {
  lv_color_t    color16;
  lv_color32_t  color32;
  lv_color16_t  color16_2;
  for(uint32_t c = 0; c < 0xFFFF; c++) {
    color16.full = c;
    color32.full = lv_color_to32(color16);
    color16_2    = lv_color_make(color32.ch.red, color32.ch.green, color32.ch.blue);
    if(color16.full != color16_2.full){
      Serial.printf("testRgb16to32: FAIL Color16 : Before 0x%04X --> After 0x%04X\n", color16.full, color16_2.full);
      return;
    }
    if( c % 1000 == 0) {
      Serial.printf("testRgb16to32: OK : 0x%04X\n", c);
    }
  }
  Serial.println("testRgb16to32: DONE");
}

void testHsvToRgb() {
  lv_color_t color16;
  lv_color_hsv_t colorHsv;
  lv_color_t color16_2;

  color16 = LV_COLOR_RED;
  colorHsv = lv_color_rgb_to_hsv(color16.ch.red, color16.ch.green, color16.ch.blue);
  color16_2 = lv_color_hsv_to_rgb(colorHsv.h, colorHsv.s, colorHsv.v);
  if(color16.full != color16_2.full){
    Serial.printf("testHsvToRgb: FAIL Color16 : Before 0x%04X --> After 0x%04X\n", color16.full, color16_2.full);
  }
  Serial.println("testHsvToRgb: DONE");
}

void testRgbToHsv() {
  
}

void testHsv2ToRgb() {
  
}

void testRgbToHsv2() {
  
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nsetup: littlevgl color test");

  lv_init();

#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print);
#endif

  testRgb16to32();
  testHsvToRgb();
  testRgbToHsv();
  testHsv2ToRgb();
  testRgbToHsv2();

  Serial.println("setup: ALL TESTS DONE!");
}

void loop() {
}
