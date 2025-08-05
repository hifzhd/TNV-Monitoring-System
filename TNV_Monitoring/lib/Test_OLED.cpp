#include <Wire.h>
#include <U8g2lib.h>

// Use the SSD1306 128x32 constructor (works with SSD1309 128x32)
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

void setup() {
  u8g2.begin();
  u8g2.enableUTF8Print(); // Enable UTF8 for symbols if needed
}

void loop() {
  // 1️⃣ Display Text
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);  // Choose a font
  u8g2.drawStr(0, 10, "SSD1309 Test");  // Text at (x=0, y=10)
  u8g2.drawStr(0, 22, "128x32 OLED");
  u8g2.sendBuffer();
  delay(2000);

  // 2️⃣ Draw Shapes
  u8g2.clearBuffer();
  u8g2.drawFrame(0, 0, 128, 32);      // Border
  u8g2.drawBox(10, 8, 20, 16);        // Filled box
  u8g2.drawCircle(64, 16, 8, U8G2_DRAW_ALL); // Circle in center
  u8g2.sendBuffer();
  delay(2000);

  // 3️⃣ Scrolling Text Animation
  for (int x = 128; x > -80; x--) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(x, 16, "Scrolling Text!");
    u8g2.sendBuffer();
    delay(30);
  }
}
