#include <HX711.h>
#include <Arduino.h>

#define LOADCELL_DOUT_PIN  4
#define LOADCELL_SCK_PIN   3
#define LOADCELL_POWER_PIN 6
float calibration_factor = 4220.0; // eksempelverdi

HX711 scale;

void setupScale(){
  pinMode(LOADCELL_POWER_PIN, OUTPUT);
  digitalWrite(LOADCELL_POWER_PIN, HIGH);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_gain();
  scale.set_scale(calibration_factor);
  scale.tare(); 
}

void tareScale(){
  delay(500);
  scale.tare();
}

float updateScale(){
  return scale.get_units();
}