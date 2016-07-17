i2c screen slave Arduino
=======
Two Arduino Sketches to show how driving an i2c screen from a slave Arduino.
The first one for the master Arduino sending commands to the slave
The second one for the slave Arduino to display informations

Using a small OLED screen with u8glib library requires, according fonts used, an important place in the ROM. 
So important, that he sometimes still more room to implement key functions provided. 
One solution is to use a second Arduino, slave, dedicated to the display and connected to the main Arduino through i2c bus. 
The OLED screen is also on the same I2C bus.
