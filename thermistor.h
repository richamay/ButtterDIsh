// thermistor header
// which analog pin to connect
#define THERMISTORPIN A0
// resistance at 25 degrees C
#define THERMISTORNOMINAL 10000
// temp. for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25
// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 5
// The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT 3950
// the value of the 'other' resistor
#define SERIESRESISTOR 10015

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
