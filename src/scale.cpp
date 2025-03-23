#include <HX711.h>
#include <Arduino.h>

#define LOADCELL_DOUT_PIN  4
#define LOADCELL_SCK_PIN   3
#define LOADCELL_POWER_PIN 6
float calibration_factor = 4220.0; // Put your own calibration factor here

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
  //delay(500);
  int times = 20;
	long sum = 0;
  long lastSum = 0;
  boolean finished = false;
  int stableCounter = 0;

	for (byte i = 0; i < times && !finished; i++) {
		sum = scale.read();
    if (sum == lastSum) {
      stableCounter++;
      if (stableCounter >= 2) {
        finished = true;
      }
    }
    else {
      stableCounter = 0;
    }
    lastSum = sum;
    scale.set_offset(sum); // Set the scale to 0.0
    
		// Probably will do no harm on AVR but will feed the Watchdog Timer (WDT) on ESP.
		// https://github.com/bogde/HX711/issues/73
		delay(0);
	}
}
float updateScale(){
  return scale.get_units();
}