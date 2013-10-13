/*
**
**  Ticks : fichier Header
**
**  From a sample by Nick Gammon on Arduino forum
**  http://forum.arduino.cc/index.php/topic,156476.0.html
**
**  http://www.domoenergytics.com
**  This code is in the public domain.
**
*/

#ifndef Ticks_h
#define Ticks_h
#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

// Array size for average calculations
#define ArraySize 6

class Ticks {
  static void isr0();
  static void isr1();
  static Ticks *_instance0;
  static Ticks *_instance1;
  
  byte _i1;
  byte _i2;
  unsigned long _millis1[ArraySize];
  unsigned long _millis2[ArraySize];
  int _ticks1[ArraySize];
  int _ticks2[ArraySize];

  unsigned long _currentMillis;
  unsigned long _oldMillis;
  int _currentTicks;
  int _oldTicks;
  
  const byte _numInterrupt;
  const byte _pinTicks;
  const int _period;
  volatile unsigned int _nbTicks;

  void handleTicks();
  
  public:
    Ticks (const byte numInterrupt, const byte pinTicks, const int period);
    void begin();
    void operate();
    unsigned int currentTicks();
    float instantTickRate();
    float TickRate1Period();
    float TickRate5Period();
    float TickRate25Period();
  
  private:
    int roll(int i, int n);
};  

#endif
