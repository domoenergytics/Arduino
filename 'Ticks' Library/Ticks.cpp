/*
**
**   Ticks : cpp file
** 
**  From a sample by Nick Gammon on Arduino forum
**  http://forum.arduino.cc/index.php/topic,156476.0.html
**
**  http://www.domoenergytics.com
**  This code is in the public domain.
**
*/

#include "Ticks.h"

// Ticks class constructor
// Parameters are:
//  - numInterrupt => interrupt number
//  - pinTicks => Arduino pin use for sensor
//  - period => Base period time, in ms.  1000 for 1 second, can give average over 5 & 25 seconds
Ticks::Ticks (const byte numInterrupt, const byte pinTicks, const int period) : _numInterrupt(numInterrupt), _pinTicks(pinTicks), _period(period) {
}


// function to call when initializing in setup() function
void Ticks::begin() {
  pinMode(_pinTicks, INPUT);     // Initializes the digital pin used in INPUT
  switch (_numInterrupt) {
    case 0: 
      attachInterrupt (0, isr0, RISING); 
      _instance0 = this;
      break;
    case 1: 
      attachInterrupt (1, isr1, RISING); 
      _instance1 = this;
      break;
  }
  // Wait a small before initializing variables
  delay(2);
  _nbTicks = 0;
  _currentMillis = millis();  
  _currentTicks = _nbTicks;
  _i1 = 0;
  _i2 = 0;

  // Initialization of arrays
  for (int i = 0; i < ArraySize; i++) {
    _millis1[i] = _currentMillis;
    _ticks1[i] = _currentTicks;
    _millis2[i] = _currentMillis;
    _ticks2[i] = _currentTicks;
  }
} 


// Function to call systematically in the main loop() function) 
void Ticks::operate() {
  // To manage the instantaneous value
  _oldTicks = _currentTicks;
  _oldMillis = _currentMillis;
  _currentTicks = _nbTicks;
  _currentMillis = millis();

  // To manage the values over 5 periods
  if((_currentMillis - _millis1[_i1]) > _period) { 
    _i1 = roll(_i1, 1);
    _millis1[_i1] = _currentMillis;
    _ticks1[_i1] = _currentTicks;
  }
  
  // To manage the values over 25 periods
  if((_currentMillis - _millis2[_i2]) > _period * 5) { 
    _i2 = roll(_i2, 1);
    _millis2[_i2] = _currentMillis;
    _ticks2[_i2] = _currentTicks;
  }
}


// Function called by the interruption
void Ticks::handleTicks() {
  _nbTicks++;
} 


// Function to get the current number of Ticks
unsigned int Ticks::currentTicks() {
  return(_nbTicks);
}


// Function to get the instantaneous number of Ticks per second
// Calculation is made on every call to the operate() function
float Ticks::instantTickRate() {
  float dTicks;
  float dSecond;

  dTicks = _currentTicks - _oldTicks;
  if (dTicks < 0.0) {
    dTicks += 65536.0;
  }
  dSecond = (_currentMillis - _oldMillis) / 1000.0;
  if (dSecond < 0.0) {
    dSecond += 4294967.296;
  }
  return(dTicks / dSecond);
}


// Function to get the average number of Ticks per second, over the last period
// Calculation is theoretically done in each period, but in the fact is on a time content between 1 and 2 periods
// Indeed, if the period is set to for example 1000 ms and each loop lasts 990 ms, it will take the second pass for the calculation
float Ticks::TickRate1Period() {
  float dTicks;
  float dSecond;
  int oldI = roll(_i1, ArraySize - 1);

  dTicks = _ticks1[_i1] - _ticks1[oldI];
  if (dTicks < 0.0) {
    dTicks += 65536.0;
  }
  dSecond = (_millis1[_i1] - _millis1[oldI]) / 1000.0;
  if (dSecond < 0.0) {
    dSecond += 4294967.296;
  }
  return(dTicks / dSecond);
}


// Function to get the average number of Ticks per second, over the last 5 periods
float Ticks::TickRate5Period() {
  float dTicks;
  float dSecond;
  int oldI = roll(_i1, ArraySize + 1);

  dTicks = _ticks1[_i1] - _ticks1[oldI];
  if (dTicks < 0.0) {
    dTicks += 65536.0;
  }
  dSecond = (_millis1[_i1] - _millis1[oldI]) / 1000.0;
  if (dSecond < 0.0) {
    dSecond += 4294967.296;
  }
  return(dTicks / dSecond);
}


// Function to get the average number of Ticks per second, over the last 25 periods per block of 5 periods
float Ticks::TickRate25Period() {
  float dTicks;
  float dSecond;
  int oldI = roll(_i2, ArraySize + 1);

  dTicks = _ticks2[_i2] - _ticks2[oldI];
  if (dTicks < 0.0) {
    dTicks += 65536.0;
  }
  dSecond = (_millis2[_i2] - _millis2[oldI]) / 1000.0;
  if (dSecond < 0.0) {
    dSecond += 4294967.296;
  }
  return(dTicks / dSecond);
}


// Internal function to get the next cell of tables
int Ticks::roll(int i, int n) {
  return(( i + n) % ArraySize);
}


// Interrupt Service glue routines
void Ticks::isr0() {
  _instance0->handleTicks();  
} 
void Ticks::isr1() {
  _instance1->handleTicks ();  
}

// for use by Interrupt Service glue routines
Ticks *Ticks::_instance0;
Ticks *Ticks::_instance1;
