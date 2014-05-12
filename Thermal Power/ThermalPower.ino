// --- Programme Arduino ---
// Copyright finizi - Créé le 20 janvier 2014
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

/*
****************************************************************************************************

    Dispositif pour mesurer une puissance thermique.
    Obtenir une différence de température entre 2 capteurs.
    Obtenir le débit de fluide caloporteur.
    Calculer la puissance par la formule : P (W) = m (L/h) * cp * dT * 1.16  avec cp = 1 pour l'eau
      
      
    Utilisation des broches
      
      Analogiques
        aucune

      Digitales
        2 :  => Flow Meter
        3 :  => Capteur température entrée
        4 :  Net - SS for SD card
        5 :  => Capteur température sortie
        10 : Net - SS for ethernet controller
        11 : Net - SPI bus : MOSI
        12 : Net - SPI bus : MISO
        13 : Net - SPI bus : SCK
       

****************************************************************************************************
*/


/*
****************************************************************************************************
**
**  Plan
**    Déclarations
**     . Includes
**     . Defines
**     . Variables globales
**     . Objets
**    Fonctions obligatoires
**     . setup
**     . loop
**    Fonctions utilisateur
**     . SIGNAL
**     . useInterrupt
**     . url
**     . flow
**     . power
**     . sendToServer
**     . printAddress
**
****************************************************************************************************
*/


#include <avr/wdt.h>               // Pour le WatchDog
#include <OneWire.h>               // Pour gérer les bus OneWire
#include <DallasTemperature.h>     // Pour les capteurs de température
#include <SPI.h>                   // Pour la carte Ethernet
#include <Ethernet.h>              // Pour la carte Ethernet
#include <EthernetServer.h>        // Pour la communication Ethernet
#include <EthernetClient.h>        // Pour la communication Ethernet
#include <stdlib.h>


/*
**  Declaration des parametres de fonctionnement
*/
const byte netServerIP[4] = { 192, 168, 22, 44 };           // Adresse IP du server vers lequel envoyer les infos 
const int netServerPort = 8901;
const float CoefLitHour = 60.0 / 7.5;                       // FlowMeter 1/2 : Coefficient de conversion de tick par secondes en litres par heures

const unsigned long temperatureReadInterval = 9L * 1000L;   // Delai entre deux lectures de température (9 secondes)
const float flowSartLimit = 2.0;                            // Débit, en L/H séparant l'état de marche et l'état d'arrêt

const unsigned long serverPostingIntervalOn = 1L;           // Intervale, en minutes, d'envoi des données lorsque la pompe est en marche (toutes les minutes)
const unsigned long serverPostingIntervalOff = 15L;         // Intervale, en minutes, d'envoi des données lorsque la pompe est à l'arrêt (toutes les 15 minutes)

const float temperatureMini = -40.0;                        // Température en dessous de la quelle elle n'est pas prise en compte
const float temperatureMaxi = 130.0;                        // Température au dessus de la quelle elle n'est pas prise en compte


/*
**  Declaration des parametres de l'installation
*/
byte netMac[6] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60};     // Adresse MAC de la carte ethernet arduino
const int pinFlowMeter = 2;            // Débitmètre
const int pinTempIn = 3;               // Capteur température entrée
const int pinTempOut = 5;              // Capteur température sortie


/*
**  Declaration des constantes
*/
const int etatInconnu = 0; // L'état est inconnu. C'est la valeur par défaut. C'est donc l'état au démarrage
const int etatStop = 1;    // Le fluide caloporteur ne circule pas
const int etatMarche = 2;  // Le fluide caloporteur circule. La pompe est donc en fonctionnement
const int etatDefaut = 3;  // L'installation se met en défaut


/* 
** Déclaration des variables et initialisations
*/
int etat = etatInconnu;
int lastEtat = etatInconnu;
int beforeDefaultEtat = etatInconnu;
float tempIn = -99.9;
float tempOut = -99.9;

// Chaines d'état à afficher
const String etatsText[4] = { 
  "Inconnu",         // #define etatInconnu 0
  "Stop",            // #define etatStop 1
  "Marche",          // #define etatMarche 2
  "Defaut"           // #define etatDefaut 3
};

unsigned long serverLastConnectionTime = 0L;  // Dernière fois que l'on a envoyé des données au server, en millisecondes
unsigned long temperatureTime = 0L;           // Dernière fois que les températures ont été lues, en millisecondes     
char fBuf[32];                                // Buffer char pour recevoir un float converti en string

unsigned long loopFlowMeterLastCalcTime = 0L; // Dernière fois que l'on a calculé le débit
unsigned long loopLastPulses = 0L;
float loopMeanFlow = 0.0;

unsigned long sendFlowMeterLastCalcTime = 0L; // Dernière fois que l'on a envoyé le débit
unsigned long sendLastPulses = 0L;
float sendMeanFlow = 0.0;


/*
**  Variables utilisées dans les interruptions
*/
volatile unsigned long pulses = 0L;           // Pour compter le nombre de battements
volatile unsigned char lastFlowPinState;      // Pour conserver l'état précédent d'une boucle à la suivante
volatile unsigned int lastFlowRateTimer = 0;  // Conserver le temps entre 2 battements
volatile float flowRate;                      // Calcul du débit


/*
**  Création des objets
*/
OneWire oneWireIn(pinTempIn);            // on défini la broche utilisée pour la température d'entrée
OneWire oneWireOut(pinTempOut);          // on défini la broche utilisée pour la température de sortie

// Passer une référence OneWire à l'objet Dallas Temperature. 
DallasTemperature sensorIn(&oneWireIn);
DallasTemperature sensorOut(&oneWireOut);

// Tableaux indiquant l'adresse des capteurs
DeviceAddress thermoIn;
DeviceAddress thermoOut;

// On instancie un objet client pour pouvoir se connecter à un server Internet
EthernetClient netClient;



/*
****************************************************************************************************
**
**  setup  ==>  Fonction obligatoire d'initialisation
**
****************************************************************************************************
*/
void setup() {

  // Initialisation de la communication série, pour le débuggage
  Serial.begin(115200); 
  Serial.println("Serial init OK");

  // Initialisation du WatchDog
  wdt_enable(WDTO_8S);
  Serial.println("Watchdog OK");

  // Initialisation du débitmètre
  pinMode(pinFlowMeter, INPUT);
  digitalWrite(pinFlowMeter, HIGH);
  lastFlowPinState = digitalRead(pinFlowMeter);
  useInterrupt(true);
  Serial.println("FlowMeter init OK");

  // Initialisation du capteur de température d'entrée
  Serial.print("Input temperature sensor :"); 
  sensorIn.begin();
  Serial.print("  Found ");
  Serial.print(sensorIn.getDeviceCount(), DEC);
  Serial.println(" devices.");
  // Vérifie le mode d'alimentation
  Serial.print("  Parasite power is: "); 
  if (sensorIn.isParasitePowerMode()) {
     Serial.println("ON");
  }
  else {
    Serial.println("OFF");
  }
  if (!sensorIn.getAddress(thermoIn, 0)) {
    Serial.println("  Unable to find address for Device 0"); 
  }
  Serial.print("  Device 0 Address: ");
  printAddress(thermoIn);
  sensorIn.setResolution(thermoIn, 12);
  Serial.print("  Device 0 Resolution: ");
  Serial.println(sensorIn.getResolution(thermoIn), DEC); 

  // Initialisation du capteur de température de sortie
  Serial.print("Output temperature sensor :"); 
  sensorOut.begin();
  Serial.print("  Found ");
  Serial.print(sensorOut.getDeviceCount(), DEC);
  Serial.println(" devices.");
  // Vérifie le mode d'alimentation
  Serial.print("  Parasite power is: "); 
  if (sensorOut.isParasitePowerMode()) {
    Serial.println("ON");
  }
  else {
    Serial.println("OFF");
  }
  if (!sensorOut.getAddress(thermoOut, 0)) {
    Serial.println("  Unable to find address for Device 1"); 
  }
  Serial.print("  Device 1 Address: ");
  printAddress(thermoOut);
  sensorOut.setResolution(thermoOut, 12);
  Serial.print("  Device 1 Resolution: ");
  Serial.println(sensorOut.getResolution(thermoOut), DEC); 
  Serial.println("Sensors OK");

  // Initialisation de la connexion Ethernet avec l'adresse MAC, l'adresse IP et le masque
  if (Ethernet.begin(netMac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // On peut éventuellement essayer de configurer la carte avec une IP fixe
    //Ethernet.begin(netMac, IpFixe);
  }  
  Serial.println("Ethernet DHCP OK");
  Serial.print("Local IP = ");
  Serial.println(Ethernet.localIP());
  Serial.println("Ethernet OK");

  // Initialisation de l'état
  etat = etatInconnu;
  lastEtat = etatInconnu;
  beforeDefaultEtat = etatInconnu;
  Serial.println("End Etat");

  // Informer le server du setup en lui envoyant l'IP locale
  String setupUrl;
  setupUrl = "ip=";
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    setupUrl += Ethernet.localIP()[thisByte];
    if (thisByte < 3) {
      setupUrl += ".";
    }
  }
  Serial.println(setupUrl);
  sendToServer("ip", setupUrl);
  Serial.println("End Setup");
}



/*
****************************************************************************************************
**
**  loop  ==>  Fonction obligatoire. Boucle infinie de fonctionnement
**
****************************************************************************************************
*/
void loop() {
  
  /*************************************************************
  **
  ** Reset du watchDog : C'est reparti pour 8 secondes
  */
  wdt_reset();


  /*************************************************************
  **
  ** Lecture des températures
  */
  boolean temperatureRead = false;
  // Ce n'est pas la peine de lire les températures à chaque boucle. Toutes les [temperatureReadInterval] millisecondes suffit
  if ( (temperatureTime == 0) || ( (millis() - temperatureTime > temperatureReadInterval) && (millis() >= temperatureTime) ) ) {
    temperatureTime = millis();
    // Lecture des températures
    sensorIn.requestTemperatures();
    tempIn = sensorIn.getTempC(thermoIn);
    sensorOut.requestTemperatures();
    tempOut = sensorOut.getTempC(thermoOut);
    temperatureRead = true;
  }

  
  /*************************************************************
  **
  ** Cohérence des températures
  */
  // Si les températures autour du ballon sont trop basses, c'est qu'il y a un problème
  if ( (etat == etatMarche) && ( (tempIn < 0.5) || (tempOut < 0.5) ) ) {
    // On est en défaut
    Serial.println("Default status !");
    // Si on ne l'était pas avant, il faut enregistrer l'état en cours
    if (etat != etatDefaut) {
      beforeDefaultEtat = etat;
    }
    // On peut passer en défaut
    etat = etatDefaut;
  }
  else {
    // On n'est pas (on n'est plus) en défaut
    // Si on était en déafut auparavant, on remet l'état d'avant le défaut
    if (etat == etatDefaut) {
      etat = beforeDefaultEtat;
    }
  }


  /*************************************************************
  **
  ** Calcul du débit à chaque boucle
  **   Permet de calculer l'état du capteur
  */
  unsigned long loopPulses = pulses;
  unsigned long loopMillis = millis();

  if (loopMillis > loopFlowMeterLastCalcTime) {
    loopMeanFlow = (loopPulses - loopLastPulses) * 1000.0 / (loopMillis - loopFlowMeterLastCalcTime);
  }
  loopMeanFlow *=  CoefLitHour;
  loopFlowMeterLastCalcTime = loopMillis;
  loopLastPulses = loopPulses;
  
  
  /*************************************************************
  **
  ** Actions à mener en fonction des mesures effectuées et de l'état en cours
  */
  switch(etat) {
    case etatInconnu:       // ==> Survient normalement au démarrage
      // Vérifier la circulation du fluide
      if (loopMeanFlow > flowSartLimit) {
        etat = etatMarche;
      }
      else {
        etat = etatStop;
      }
      break;  
    case etatStop:          // ==> Pas de circulation de fluide
      // Vérifier si le fluide circule
      if (loopMeanFlow > flowSartLimit) {
        etat = etatMarche;
      }
      break;
    case etatMarche:        // ==> Le fluide caloporteur circule.
      // Vérifier si le fluide ne circule plus
      if (loopMeanFlow < flowSartLimit) {
        etat = etatStop;
      }
      break;
    case etatDefaut:        // ==> L'installation s'est mise en défaut
      // Ne rien faire et laisser ainsi
      break;  
  }


  /*************************************************************
  **
  ** Envoyer les données sur un server si :
  **   - L'état a changé
  **   - OU  * L'interval est écoulé  
  **         * ET une lecture vient d'avoir lieu
  ** Le but est d'envoyer des infos le plus fraiches et pertinentes possibles au server
  */
  if (  (lastEtat != etat) 
        || ( (etat == etatMarche) 
            && (millis() - serverLastConnectionTime > serverPostingIntervalOn * 60L * 1000L) 
            && (temperatureRead) 
            && ( millis() >= serverLastConnectionTime) ) 
        || ( (etat == etatStop) 
            && (millis() - serverLastConnectionTime > serverPostingIntervalOff * 60L * 1000L) 
            && (temperatureRead) 
            && ( millis() >= serverLastConnectionTime) ) 
      ) {
    
    // Calcul du débit moyen à envoyer
    unsigned long sendPulses = pulses;
    unsigned long sendMillis = millis();
    if (sendMillis > sendFlowMeterLastCalcTime) {
      sendMeanFlow = (sendPulses - sendLastPulses) * 1000.0 / (sendMillis - sendFlowMeterLastCalcTime);
    }
    sendMeanFlow *=  CoefLitHour;
    sendFlowMeterLastCalcTime = sendMillis;
    sendLastPulses = sendPulses;
        
    String sendUrl = url(0);
    if (sendToServer("data", sendUrl) == true) {
      serverLastConnectionTime = millis();   // Réinitialise le compteur
    }
    // Debug : On imprime les données envoyées au port série
    Serial.println(sendUrl);
  }
  lastEtat = etat;
  
  // Attendre avant de boucler (2 secondes)
  delay(2000);
}	    


/*
****************************************************************************************************
**
**  SIGNAL  ==>  Fonction système
**               Interrupt is called once a millisecond, looks for any pulses from the sensor!
**               source : https://github.com/adafruit/Adafruit-Flow-Meter
**
****************************************************************************************************
*/
SIGNAL(TIMER0_COMPA_vect) {
  uint8_t x = digitalRead(pinFlowMeter);
  
  if (x == lastFlowPinState) {
    lastFlowRateTimer++;
    if (lastFlowRateTimer > 1000) {
      flowRate = 0;
      lastFlowRateTimer = 0;
    }
    return; // nothing changed!
  }
  
  if (x == HIGH) {
    //low to high transition!
    pulses++;
  }
  lastFlowPinState = x;
  flowRate = 1000.0;
  flowRate /= lastFlowRateTimer;  // in hertz
  lastFlowRateTimer = 0;
}



/*
****************************************************************************************************
**
**  useInterrupt  ==>  Fonction qui initialise la récupération des interruptions
**                     source : https://github.com/adafruit/Adafruit-Flow-Meter
**
****************************************************************************************************
*/
void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
  }
}


/*
****************************************************************************************************
**
**      Fonctions diverses
**
****************************************************************************************************
*/

/*
** Formatte les données à envoyer au format url
**     example : {"etat":"Stop","in":23.88,"out":23.81,"flow":225.21,"Wh":2700, "millis":16019}
**            => "etat=Stop&in=23.88&out=23.81&flow=225.21&Wh=2700&millis=16019" 
*/
String url(boolean etatOnly) {
  String x;
  x = "etat=";
  x += etatsText[etat];
  if (!etatOnly) {
    x += "&in=";
    dtostrf(tempIn, 3, 2, fBuf);
    x += fBuf;
    x += "&out=";
    dtostrf(tempOut, 3, 2, fBuf);
    x += fBuf;
    x += "&flow=";
    dtostrf(flow(), 3, 2, fBuf);
    x += fBuf;
    x += "&Wh=";
    dtostrf(power(), 3, 2, fBuf);
    x += fBuf;
    x += "&millis=";
    x += millis();
  }
  return(x);
}

/*
** Calcul du débit. Le but est d'avoir un chiifre si il n'y a rien
*/
float flow() {
  float tr = sendMeanFlow;
  if (isnan(tr)) {
    tr = 0;
  }
  return(tr);
}

/*
** Calcul de la puissance thermique
**   P (kW) = m (m3/h) * cp * dT * 1.16
**   P (W) = m (L/h) * cp * dT * 1.16
**    avec cp = 1 pour l'eau
*/
float power() {
  float wh = 0;
  wh = flow() * 1.0 * (tempIn - tempOut) * 1.16;
  return(wh);
}

/*
** Envoie des données au server
*/
boolean sendToServer(String lRoot, String datas) {
  boolean ret = false;
  if (netClient.connect(netServerIP, netServerPort)) {
    netClient.print("GET /");
    netClient.print(lRoot);
    netClient.print("/");
    netClient.print(datas.cstr());
    netClient.println(" HTTP/1.0");
    netClient.println();
    delay(2);
    netClient.stop();
    ret = true;
  }
  else {
    Serial.println("No connection");
  }
  return(ret);
}


/*
** Envoie au terminal série l'adresse du capteur
*/
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) {
      Serial.print("0");
    }
    Serial.print(deviceAddress[i], HEX);
  }
  Serial.println("");
}


/*
****************************************************************************************************
**
**      FIN
**
****************************************************************************************************
*/

