// --- Programme Arduino ---
// Copyright finizi - Created july 2016 the 1st
// www.DomoEnergyTICs.com
//  Code sous licence GNU GPL :
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License,
//  or any later version.
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

/*************************************************************

I2C master

Envoie via le bus i2c des informations à afficher sur un écran également i2c,
typiquement, un écran oled utilisant la librairie "U8glib".
L'utilisation de la librairie "U8glib", et notamment de différentes polices, 
peut amener a utiliser une grande partie de la ROM disponible.
Pour palier cela, une des possibilité est d'utiliser un Arduino dédié à l'affichage, 
qui va gérer l'affichage et uniquement l'affichage, en fonction des informations 
reçues via le bus i2c.

Cet exemple concerne le code qui ne fait qu'envoyer les informations à afficher.
Envoie des informations : exemples de coordonnées géographiques et de message

*************************************************************/

#include <Wire.h>


/*************************************************************
*   Les constantes
*/
const byte i2cRemoteDisplay = 0x08;

/*************************************************************
*   Déclaration des variables globales
*/
int x = 0;
int y = 0;


/**************************************************************************
**
**  setup()  ==>  Fonction obligatoire d'initialisation
**
**************************************************************************/

void setup() {
  Serial.begin(115200);
  Serial.println("Serial Init !");
  Serial.println("==> i2cMaster <==");
  // Joindre le bus i2c en tant que maitre
  Wire.begin();
  Serial.println("Wire begin !");
}


/**************************************************************************
**
**  loop()  ==>  Fonction obligatoire. Boucle infinie de fonctionnement
**
**************************************************************************/
 
void loop() {
  // On envoie de nouvelle coordonnées à chaque fois
  i2cSendCoordinate(i2cRemoteDisplay, "Lat", 47 + (random(1000) / 99.0) );
  i2cSendCoordinate(i2cRemoteDisplay, "Lng", -3 - (random(1000) / 99.0) );
  x++;
  switch (x) {
    case 5:
      // Envoyer le message à afficher
      i2cSendString(i2cRemoteDisplay, "Msg", String(y) );
      y = y == 0 ? 1 : 0;
      // Afficher le message
      i2cSendString(i2cRemoteDisplay, "Disp", "Message" );
      break;
    case 10:
      // afficher autre chose
      i2cSendString(i2cRemoteDisplay, "Disp", "toto" );
      x = 0;
      break;
    default: 
      // Afficher les coordonnées précédement envoyées
      i2cSendString(i2cRemoteDisplay, "Disp", "Coordinates" );
      break;
  }
  delay(1000);
}


/**************************************************************************
**
**  Fonctions diverses
**
**************************************************************************/
 
// Convertion d'un float en String
String floatToString(float val, unsigned char dec) {
  char str[30];
  dtostrf(val, 1, dec, str);
  return(String(str));
}

// Envoie d'une chaine de caractères
void i2cSendString(int i2cAddrs, String what, String txt) {
  byte n;
  byte error;
  // On va essayer 5 fois, car l'esclave n'est pas toujours à l'écoute
  for (int i = 0; i < 5; i++) {
    Wire.begin();                // Reprendre le contrôle du bus i2c à chaque fois
    Wire.beginTransmission(i2cAddrs); 
    Wire.write(what.c_str());
    Wire.write("=");  
    Wire.write(txt.c_str());
    error = Wire.endTransmission();    // stop transmitting
    if (error == 0) {
      // Si error vaut 0, c'est que c'est passé, ne pas réessayer
      break;
    }
    // Il est possible que l'arduino esclave soit occupé à afficher
    delay(100);    // On attend un peu avant nouvel essai
  }
}

// Envoie d'une coordonnée (latitude ou longitude)
void i2cSendCoordinate(int i2cAddrs, String what, float val) {
  i2cSendString(i2cAddrs, what, floatToString(val, 6));
}



