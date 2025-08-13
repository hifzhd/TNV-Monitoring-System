#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <Arduino.h>

void setup() {
  Serial.begin(115200);

  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
      ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);

  if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
    Serial.println("Two Point calibration value applied!");
  } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.println("Vref calibration value applied!");
  } else {
    Serial.println("Default Vref used, no eFuse calibration found.");
  }

  Serial.printf("Vref: %d mV\n", adc_chars.vref);
}

void loop() {}
