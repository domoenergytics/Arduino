/*
**
** FlowMeter
**
** FlowMeter read sample
**
** http://www.domoenergytics.com
** This example code is in the public domain.
**
*/

#include "Ticks.h"

const byte pinFlowMeter = 2;    // data pin for FlowMeter sensor
const byte numInterrupt = 0;    // Interrupt number
const int period = 1000;        // Base periode, in milliseconds

const float CoefLitMin = 7.4;


// Instantiate the class for counting ticks
Ticks myTicks(numInterrupt, pinFlowMeter, period);


void setup() {
  Serial.begin(115200);    // Initializes serial communication with PC terminal
  myTicks.begin();         // Initializes the class for counting ticks
}

void loop() {

  myTicks.operate();       // Count

  Serial.print("t : ");
  Serial.print(myTicks.instantTickRate());
  Serial.print("  1s : ");
  Serial.print(myTicks.TickRate1Period());
  Serial.print("  5S : ");
  Serial.print(myTicks.TickRate5Period());
  Serial.print("  25s : ");
  Serial.print(myTicks.TickRate25Period());
  Serial.print(" ==> ");
  Serial.print(CoefLitMin * myTicks.TickRate25Period(), 2);
  Serial.print(" liter/minute");
  Serial.println(" !");

  delay(500);
}
