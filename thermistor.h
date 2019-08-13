// thermistor header
// define for internal
// resistance at 25 degrees C
#define INTERNAL_THERMISTORNOMINAL 10000
// temp. for nominal resistance (almost always 25 C)
#define INTERNAL_TEMPERATURENOMINAL 25
// The beta coefficient of the thermistor (usually 3000-4000)
#define INTERNAL_BCOEFFICIENT 3950
// the value of the 'other' resistor
#define INTERNAL_SERIESRESISTOR 10015
#ifdef ESP8266
#define FULL_SCALE_CORR 0.3125 / 0.33333 //correction for full scale problem in ESP8266
#define MISC_CORR 14900.0 / 12755.0 //correction for other factors
#else
#define INTERNAL_FULL_SCALE_CORR 1 //correction for full scale problem in ESP8266
#define INTERNAL_MISC_CORR 1 //correction for other factors

#endif

// define for Other
// resistance at 25 degrees C
#define OTHER_THERMISTORNOMINAL 10000
// temp. for nominal resistance (almost always 25 C)
#define OTHER_TEMPERATURENOMINAL 25
// The beta coefficient of the thermistor (usually 3000-4000)
#define OTHER_BCOEFFICIENT 3950
// the value of the 'other' resistor
#define OTHER_SERIESRESISTOR 10015


// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 5

int samples[NUMSAMPLES];

float get_raw_temp(int thermistorpin, float thermistor_nominal, float temperature_nominal, int numsamples, float bcoefficient, float seriesresistor,
                   float full_scale_corr, float misc_corr)
{
  uint8_t i;
  float average;

  // take N samples in a row, with a slight delay
  for (i = 0; i < numsamples; i++) {
    samples[i] = analogRead(thermistorpin);
    delay(100);
  }

  // average all the samples out
  average = 0;
  for (i = 0; i < numsamples; i++) {
    average += samples[i];
  }
  average /= numsamples;

  //correct for the divider scaling on the nodemcu
  average = average * full_scale_corr;
  //  Serial.print("Average analog reading ");
  //  Serial.println(average);

  // convert the value to resistance
  average = 1023 / average - 1;
  average = seriesresistor / average;
  //correct for low VCC and low impedance ADC
  average *= misc_corr;

  //  Serial.print("Thermistor resistance ");
  //  Serial.println(average);

  float steinhart;
  steinhart = average / thermistor_nominal;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= bcoefficient;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (temperature_nominal + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C

  return steinhart;
}
