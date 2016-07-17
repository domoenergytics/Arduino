// --- Programme Arduino ---
// Copyright finizi - Créé le 1er juillet 2016
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

I2C display

Réception via le bus i2c des informations à afficher sur un écran également i2c,
typiquement, un écran oled en utilisant la librairie "U8glib".
L'utilisation de la librairie "U8glib", et notamment de différentes polices, 
peut amener a utiliser une grande partie de la ROM disponible.
Pour palier cela, une des possibilité est d'utiliser un Arduino dédié à l'affichage, 
qui va gérer l'affichage et uniquement l'affichage, en fonction des informations 
reçues via le bus i2c.

Cet exemple concerne le code qui gère l'affichage.
 - Réception des informations
 - Afficher les informations demandée par l'autre arduino

*************************************************************/


#include "U8glib.h"
#include <Wire.h>


/*************************************************************
*   Déclaration des objets
*/
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_DEV_0|U8G_I2C_OPT_NO_ACK|U8G_I2C_OPT_FAST);  // Fast I2C / TWI 


/*************************************************************
*   Les constantes
*/
const byte i2cRemoteDisplay = 8; //0x08;
const String messages[2] = { 
  "Message n" + String(char(176)) + " 1",
  "Second message" 
  };  

/*************************************************************
*   Déclaration des variables globales
*/
int message = 0;
double gpsLng = -999.0;
double gpsLat = 99.0;
String toDisp = "Coordinates";




/**************************************************************************
**
**  Fonctions exécutées suite à une évènement
**
**************************************************************************/
 
// function that executes whenever data is received from master
void receiveEvent(int howMany) {
  String str = "";
  // Construction de la chaine reçue, caractère par caractère
  while (Wire.available() > 0) { 
    char c = Wire.read(); 
    str += String(c);
  }
  Serial.print("receiveEvent : ");
  Serial.println(str);
  i2cParse(str);
}


/**************************************************************************
**
**  setup()  ==>  Fonction obligatoire d'initialisation
**
**************************************************************************/

void setup(void) {
  Serial.begin(115200);             // Initialiser le port série pour le terminal
  Serial.println("Serial Init !");

  oledClear();                      // Efface l'écran
  oledDrawStr("oled setup", 0, 20); // Affiche une info
  Serial.println("oLed init !");
  delay(500);

  Wire.begin(i2cRemoteDisplay);     // Initialisation du bus i2c
  Wire.onReceive(receiveEvent);     // On commence à écouter l'évènement
  Serial.println("i2c init !");
}


/**************************************************************************
**
**  loop()  ==>  Fonction obligatoire. Boucle infinie de fonctionnement
**
**************************************************************************/
 
void loop(void) {
  //Wire.begin(i2cRemoteDisplay);     // Initialisation du bus i2c
  //oledDraw();                       // Affiche ce qui est demandé par le maitre
  Serial.println(millis());         // Envoie une valeur au terminal pour monter que ça tourne
  delay(1000);                      // Attendre une seconde. L'écra sera au mieux mis à jour toutes les secondes
}


/**************************************************************************
**
**  Fonctions diverses
**
**************************************************************************/
 
// Analyse de la chaine reçue
void i2cParse(String s) {
  int egalPlace = 0;
  String arg = "";
  String val = "";
  egalPlace = s.indexOf("=");
  if (egalPlace > 0) {
    arg = s.substring(0, egalPlace);
    val = s.substring(egalPlace+1);
    //Serial.println(arg);
    //Serial.println(val);

    if (arg == "Lat") {
      gpsLat = val.toFloat();
    }
    if (arg == "Lng") {
      gpsLng = val.toFloat();
    }
    if (arg == "Msg") {
      message = val.toInt();
    }
    if (arg == "Disp") {
      toDisp = val;
      oledDraw();                       // Affiche ce qui est demandé par le maitre
    }
  }
}


/**************************************************************************
**
**  Fonctions d'affichage
**
**************************************************************************/
 
// Gestion des informations à afficher, en fonction de la demande du maitre
void oledDraw() {
  if (toDisp == "Coordinates") {
    oledDrawPosition();
    return;
  }
  if (toDisp == "Message") {
    oledDrawStr(messages[message], 0, 30);
    return;
  }
  oledDrawStr("Toto fait v" + String(char(233)) + "lo", 0, 20);
}


// Effacement complet de l'écran Oled
void oledClear() {
  Serial.println("Clear");
  u8g.setColorIndex(0);
  u8g.drawBox(0, 0, 127, 63);
  u8g.setColorIndex(1);
}


// affichage d'une chaine à l'emplacement indiqué
void oledDrawStr(String lStr, int x, int y) {
  int i = 0;
  Wire.end();
  u8g.firstPage();  
  do {
    u8g.setFont(u8g_font_unifont);
    u8g.drawStr( x, y, lStr.c_str());
  } while( u8g.nextPage() );
  delay(10);
  Wire.begin(i2cRemoteDisplay);                
}

// Affichage sur l'écran Oled
void oledDrawPosition(void) {
  char str[12];
  // On commence par ne plus écouter le bus i2c
  Wire.end();
  u8g.firstPage();  
  do {
    //u8g.setFont(u8g_font_unifont);
    u8g.setFont(u8g_font_9x18);
    //u8g.setFont(u8g_font_osb21);
    u8g.drawStr( 0, 16, Hms(gpsLat, 'l').c_str());
    u8g.drawStr( 0, 40, Hms(gpsLng, 'g').c_str());
  } while( u8g.nextPage() );
  // On reprend l'écoute du bus i2c
  Wire.begin(i2cRemoteDisplay);                
}

// Formatage d'une coordonnée
// c:  valeur décimale de la coordonnée
// lg: caractère indiquant le type de coordonnée : L = latitude; G = longitude
String Hms(double c, char lg) {
  const int dec = 1;      // Affiche une décimale après les secondes
  String disp;
  int nbC = 2;            // Nombre de chiffres pour la latitude
  disp.reserve(16);
  bool neg = c > 0 ? 0 : 1;
  char sign, coord;
  if ((lg == 'G') || (lg == 'g')) {
    sign = c > 0 ? 'E' : 'W';
    disp = String(sign) + " ";
    nbC = 3;
  }
  else {
    sign = c > 0 ? 'N' : 'S';
    disp = String(sign) + "  ";
  }
  c = abs(c);
  int entPart = (int) c;
  float rest = c - (float)entPart;
  int min = (int) (60.0 * rest);
  rest -= min / 60.0;
  int sec = (int) (3600.0 * rest);
  rest -= sec / 3600.0;
  int decSec = 0.5 + (pow(10.0, dec) * ((3600.0 * rest) - (int) (3600.0 * rest)));
  if (decSec >= 10) {
    decSec = 0;
    sec += 1;
  }
  if (sec >= 60) {
    sec = 0;
    min += 1;
  }
  if (min >= 60) {
    min = 0;
    entPart += 1;
  }
  disp += addLeadingZeroes(String(entPart), nbC);
  disp += String(char(176));
  disp += addLeadingZeroes(String(min), 2);
  disp += "'";
  disp += addLeadingZeroes(String(sec), 2);
  disp += "\"";
  disp += addLeadingZeroes(String(decSec), dec);
  return(disp);  
}

// Ajout de "0" devant la chaine
String addLeadingZeroes(String s, int lMax) {
  return(addLeadingChar(s, '0', lMax));
}

// Ajout du caractère passé en paramètre devant la chaine
String addLeadingChar(String s, char c, int lMax) {
  String l;
  l.reserve(lMax);
  l = "";
  for(int i = s.length(); i < lMax; i++) {
    l += String(c);
  }
  return(l + s);
}


