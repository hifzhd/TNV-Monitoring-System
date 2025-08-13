#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <Arduino.h>

const adc1_channel_t channel = ADC1_CHANNEL_6;  // GPIO34
const adc_bits_width_t width = ADC_WIDTH_BIT_12;
const adc_atten_t atten = ADC_ATTEN_DB_11; // ~0–3.3V

esp_adc_cal_characteristics_t adc_chars;

void setup() {
  Serial.begin(115200);

  // Configure ADC1
  adc1_config_width(width);
  adc1_config_channel_atten(channel, atten);

  // Characterize ADC
  esp_adc_cal_characterize(ADC_UNIT_1, atten, width, 1100, &adc_chars);
}

void loop() {
  int raw = adc1_get_raw(channel);
  uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

  Serial.printf("raw=%d, voltage=%u mV\n", raw, voltage_mv);
  delay(200);
}
