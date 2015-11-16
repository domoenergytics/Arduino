// --- Programme Arduino ---
// Copyright finizi - Créé le 14 octobre 2015
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
******************************************************************************************************************************************************
**
**  Mesure de la profondeur dans la cuve à eau de pluie
**   - Une mesure régulière (tous les jours)
**   - Une mesure à la demande
**   - Toutes les mesures sont envoyées vers la base de données
**   - Il y a plusieurs solutions pour faire des relevés réguliers mais relativement espacés :
**      1) En comptant le nombre de millisecondes écoulées depuis la dernière mesure (86400000 par jour) -> Interval régulier uniquement
**      2) En récupérant l'heure via la carte réseau -> Programmation horaire; plusieurs par jour possible
**         Programmation 2 fois par jour, à 11h00 TU et 23h00 TU
**      3) En réagissant à une demande externe générée par un server
**     Les solutions 2 & 3 ont été implémentées
**   - Lors de la lecture du niveau d'eau:
**      * Prendre une série de mesures rapides pour en faire une moyenne sur plusieurs écahantillons
**      * On conserve les X dernières mesures
**      * Tant qu'il y a au moins une augmentation dans cette série, on recommence (phase 1)
**      * Si toutes les mesures montrent une diminution, on passe à la phase 2 
**      * Tant qu'il n'y a que des diminutions, on recommence (phase 2)
**      * Si au moins une augmentation (donc à la première augmentation, on conserve la mesure
**    Les documents jSon reront donc de la forme
**      {"status":"ok", "level":"1207"}
**    Les informations sont transmises au server dans l'URL.
**      L'URL sera de la forme :
**       - http://192.168.1.1:8080/data/WaterTank/status=ok&level=1207
**
**
**  Modifications:
**   - 
**
**
******************************************************************************************************************************************************
*/



/*
******************************************************************************************************************************************************
**
**      Utilisation des broches
**      
**      Analogiques
**        0 :  => Capteur de pression
**        1 :  
**        2 :  
**        3 :  
**        4 :  
**        5 :  
**
**       Digitales
**        0 :
**        1 :
**        2 :  => Relais alimentation électrique de la pompe
**        3 :
**        4 :  Net - SS for SD card
**        5 :  => LED
**        6 :  
**        7 :  
**        8 :  
**        9 :  
**        10 : Net - SS for ethernet controller / LCD - Backlit control  (broche pliée)
**        11 : Net - SPI bus : MOSI
**        12 : Net - SPI bus : MISO
**        13 : Net - SPI bus : SCK
**       
**
******************************************************************************************************************************************************
*/


/*
******************************************************************************************************************************************************
**
**  Bugs:
**    - 
**
******************************************************************************************************************************************************
*/


/*
******************************************************************************************************************************************************
**
**  Evolutions  
**
**   . 
**
**
******************************************************************************************************************************************************
*/


/*
******************************************************************************************************************************************************
**
**  Structure du code
**   . Includes
**   . Defines
**   . Variables globales
**   . Objets
**   . setup()
**   . loop()
**   . fonctions diverses
**
******************************************************************************************************************************************************
*/


/*************************************************************
*   Includes
*/

#include <avr/wdt.h>               // Pour le WatchDog
#include <SPI.h>                   // Pour la carte Ethernet
#include <Ethernet.h>              // Pour la carte Ethernet
#include <EthernetServer.h>        // Pour la communication Ethernet
#include <EthernetClient.h>        // Pour la communication Ethernet
#include <EthernetUdp.h>           // Pour le protocole NTP qui permet de récupérer l'heure



/*************************************************************
*   Defines & constantes
*/
const int printPause = 50;           // Pause après envoi sur le terminal série, pour laisser le temps au destinataire de traiter l'information
const int pinRelayPump = 2;          // Broche du relai d'alimentation électrique de la pompe à air
const int pinPressure = A0;          // Broche reliée au capteur de pression
const int pinInternalLED = 13;       // Led interne sur la carte Arduino - Ne pas utiliser avec le shield ethernet
const int pinLED = 5;                // Led du montage 

const int relayOn = 0;
const int relayOff = 255;

const int httpMaxChar = 25;          // Nombre de caractère maxi à traiter
//const long millisDay = 86400000L;    // Nombre de millisecondes dans une journée
const int nbConsReads = 5;           // Nombre de mesures consécutives à observer pour obtenir un résultat stable
const int nLoop = 100;               // Nombre de lectures à effectuer pour avoir une mesure
const int maxCycles = 100;           // Nombre de cycle maxi avant mise en défaut
const int ntpEveryMinutes = 10;      // Demande NTP (heure) toutes les 10 minutes
const unsigned long seventyYears = 2208988800UL;
                                     // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:

// Déclaration des états
const int statusUnknown = 0;         // Etat inconnu ou indéfini
const int statusWaiting = 1;         // En attende de déclencher une lecture de pression
const int statusOperatingPhase1 = 2; // Une lecture de pression est en cours, en phase 1
const int statusOperatingPhase2 = 3; // Une lecture de pression est en cours, en phase 2
const int statusDefault = 99;        // Mise en défaut car hauteur d'eau non lue correctement

const int ntpPacketSize = 48;        // NTP time stamp is in the first 48 bytes of the message



/*************************************************************
*   Déclaration des variables globales
*/

int loopStatus = statusUnknown;      // Etat en cours
int loopLastStatus = statusUnknown;  // Etat précédent
int ledStatus = LOW;

float waterLevel = 0;
float reads[nbConsReads-1];          // Enregistrement des lectures successives
int cycles = 0;                      // Nombre de cycle depuis le démarrage de la pompe


// Déclaration relatives à la config réseau
byte netMac[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};  // Adresse MAC de la carte ethernet arduino
byte netIp[4] = { 192, 168, 1, 2 };                      // Adresse IP de la carte Arduino
byte netGateway[4] = { 192, 168, 1, 200 };               // Adresse IP de la passerelle
byte netMask[4] = { 255, 255, 255, 0 };                  // Masque sous-réseau
byte sqlServer[4] = { 192, 168, 1, 1 };                  // Adresse IP vers laquelle envoyer les donn�es
unsigned int sqlPort = 8080;                             // Port HTTP sur lequel écoute le server SQL
unsigned int udpPort = 8888;                             // Port pour écouter les packets UDP
char timeServer[] = "time.nist.gov"; // time.nist.gov NTP server
//char timeServer[] = "192.168.2.200"; // server sur le réseau local

byte packetBuffer[ ntpPacketSize];   // buffer to hold incoming and outgoing packets
unsigned long nextNtp;               // millis() à atteindre pour demander l'heure                   
int ntpHour = -1;
int ntpMinute = -1;
int ntpSecond = -1;

/*************************************************************
*   Déclaration des objets
*/

EthernetServer httpServer(80);       // on crée un objet serveur utilisant le port 80 (port HTTP par défaut)
EthernetClient sqlClient;            // objet client pour envoyer des données
EthernetUDP udp;




/**************************************************************************
**
**  setup()  ==>  Fonction obligatoire d'initialisation
**
**************************************************************************/

void setup() {

  // Initialisation du WatchDog
  wdt_enable(WDTO_8S);

  // Initialisation de la communication série
  Serial.begin(115200);  
  Serial.println("Starting V 1.0 ...");
  delay(printPause);

  // Déclarer les broches comme "output" ou "input"
  pinMode(pinLED, OUTPUT); 
  pinMode(pinRelayPump, OUTPUT); 
  pinMode(pinPressure, INPUT);

  // Initialisation des broches pour que les relais ne soient pas actifs au reset
  digitalWrite(pinRelayPump, relayOff);

  Serial.println("Pins initialized.");
  delay(printPause);

  // Initialisation de la connexion Ethernet avec l'adresse MAC, l'adresse IP et le masque
  Ethernet.begin(netMac, netIp, netGateway, netMask);
  Serial.print("Ethernet initialized on IP ");
  for(int i = 0; i < 4; i++) {
    if (i > 0) {
      Serial.print(".");
    }
    Serial.print(netIp[i]);
  }
  udp.begin(udpPort);
  Serial.println("");
  delay(printPause);

  // Initialisation des variables
  for(int i=0; i<nbConsReads; i++) {
    reads[i] = NULL;
  }
  //getNtpTime();
  nextNtp = 0;
  
  // Initialisation du serveur interne, et commence à écouter les clients
  httpServer.begin();   

  flushSerial();   // Pour vider le port série des ordres ventuellement arriv lors de l'initialisation
  loopStatus = statusWaiting;
  Serial.println("Setup finish.");
  delay(printPause);

}



/**************************************************************************
**
**  loop()  ==>  Fonction obligatoire. Boucle infinie de fonctionnement
**
**************************************************************************/

void loop() {

  // Reset du watchDog : C'est reparti pour 8 secondes
  wdt_reset();

  // Lire les éventuelles commandes reçues sur les ports série ou réseau et y répondre
  serialCommandProcess();     
  httpCommandProcess();

  // Gérer la mesure de pression
  if (isOperating()) {
    float p = pressureRead(pinPressure);
    addPressure(p);
    Serial.print(p, 2);
    Serial.println(" kPa");
    delay(printPause);
    int e = pressureCheckEvolution();
    if (e > 0) {
      // La pile est pleine, la phase 1 a bien commencée
      if ( (loopStatus == statusOperatingPhase1) && (e == 2) ) { 
        // Phase 1 ET que des diminutions
        loopStatusChange(statusOperatingPhase2);
      }
      if ( (loopStatus == statusOperatingPhase2) && (e == 1) ) {
        // Phase 2 ET une augmentation : On a trouvé une valeur
        airPumpStop(statusWaiting);
      }
    }
    cycles++;
    if (cycles >= maxCycles) {
      airPumpStop(statusDefault);
    }
  }

  // Gérer le temps - Toutes les 10 minutes environ, récupérer l'heure
  if ( (millis() > nextNtp) && !isOperating() ) {
    getNtpTime();
    nextNtp = calcNextNtp(nextNtp);
    if (checkTime()) {
      airPumpStart();
    }
  }
  
  // Faire clignoter la LED, en fonction de loopStatus
  toggleLedStatus();
  // Si pas en cours de lecture, faire une longue pause
  if (!isOperating()) {
    // Si en défaut, quelques éclats supplémentaires
    if (loopStatus == statusDefault) {
      for (int i=0; i<7; i++) {
        toggleLedStatus();
        delay(100);
      }
      delay(800);
    }
    else {
      delay(1500);
    }
  }
}


/**************************************************************************
**
**  Fonctions diverses
**
**************************************************************************/

// Vérifie si c'est l'heure de lire la hauteur d'eau
boolean checkTime() {
  // Entre respectivement 11h00 et 23h00
  if ( (ntpHour == 11) || (ntpHour == 23) ) {
    // et 11h10 et 23h10
    if (ntpMinute < ntpEveryMinutes) {
      return(true);
    }
  }
  return(false);
}

// Change l'état de la LED
void toggleLedStatus(void) {
  if (ledStatus == LOW) {
    ledStatus = HIGH;
  }
  else {
    ledStatus = LOW;
  }
  digitalWrite(pinLED, ledStatus);
}

// Vérifie si c'est une phase d'activité 
boolean isOperating() {
  return ( (loopStatus == statusOperatingPhase1) || (loopStatus == statusOperatingPhase2) );
}

// Changer le status courant
void loopStatusChange(int newStatus) {
  loopLastStatus = loopStatus;
  loopStatus = newStatus;
  Serial.print("loopStatus=");
  Serial.println(textStatus(loopStatus));
}


// Envoi des données vers le server de dataLogging
void ethernetSendData() {
  // Envoyer le rapport vers le server
  if (sqlClient.connect(sqlServer, sqlPort)) {
    sqlClient.print("GET /data/");
    sqlClient.print("WaterTank");
    sqlClient.print("/status=");
    sqlClient.print(textStatus(loopStatus));
    sqlClient.print("&level=");
    sqlClient.print(waterLevel, 0);
    sqlClient.println(" HTTP/1.0");
    sqlClient.println();
    delay(2);
    sqlClient.stop();
  }
}

// Envoie des données de lecture sur le port série
void serialSendData(void) {
  Serial.print("{\"status\":\"");
  Serial.print(textStatus(loopStatus));
  Serial.print("\"");
  jsonPrint("level", waterLevel, 0);
  Serial.println("}");
}

// Chaine du statut
String textStatus(int currentStatus) {
  if (currentStatus == statusWaiting) {
    return("Waiting");
  }
  else if (currentStatus == statusOperatingPhase1) {
    return("OperatingPhase1");
  }
  else if (currentStatus == statusOperatingPhase2) {
    return("OperatingPhase2");
  }
  else if (currentStatus == statusDefault) {
    return("Default");
  }
  return("Unknown");
}

//  Ajout d'une valeur au document jSon, après vérification
void jsonPrint(String label, float val, int precis) {
  if ( !isinf(val) && !isnan(val) && (val <= 4294967040.0) && (val >= -4294967040.0) ) {
    Serial.print(",\"");
    Serial.print(label);
    Serial.print("\":");
    Serial.print(val, precis);
  }
}


/**************************************************************************
**
**  Gestion des commandes reçues
**
**************************************************************************/

// Lire les éventuelles commandes reçues sur le port série et y répondre
void serialCommandProcess(void) {
  String commandText = String("");
  char byteInSerial;
  while (Serial.available() > 0) {
    byteInSerial = Serial.read();
    if ( (byteInSerial == 10) || (byteInSerial == 13) ) {
      //Serial.println("*");
      commandProcess(commandText);
    }
    else {
      //Serial.println("+");
      commandText += String(byteInSerial);
    }
  }
  if (commandText.length() > 0) {
    //Serial.println("-");
    commandProcess(commandText);
  }
}  

void commandProcess(String commandTxt) {
  Serial.println("commandProcess");
  // Test les différentes commandes possibles
  if (commandTxt.startsWith("Reset")) {
    commandReset();
  }
  if (commandTxt.startsWith("ReadWaterLevel")) {
    commandInitOperate();
  }
  if (commandTxt.startsWith("AbortProcess")) {
    commandAbortProcess();
  }
  else {
    commandUnknown(commandTxt);
  }
}

// Commande de reset
void commandReset() { 
  Serial.println("commandReset");
  delay(printPause);
  // Demander au watchdog de ne pas attendre plus que 15 millisecondes avant d'agir
  wdt_enable(WDTO_15MS);
  // Demander une longue attente pour déclencher le watchdog
  delay(10000);
}

// Commande d'initialisation de la lecture du niveau d'eau dans la cuve
void commandInitOperate(void) { 
  Serial.println("commandInitOperate");
  delay(printPause);
  airPumpStart();
}

// Commande AbortProcess
void commandAbortProcess(void) {
  Serial.println("commandAbortProcess");
  delay(printPause);
  digitalWrite(pinRelayPump, relayOff);
  loopStatusChange(statusWaiting);
}

// Commande inconnue !
void commandUnknown(String commandTxt) { 
  Serial.print("commandUnknown:");
  Serial.println(commandTxt);
  delay(printPause);
}

// Démarrage de la pompe à air - Démarrage du processus de lecture du niveau d'eau
void airPumpStart(void) {
  digitalWrite(pinRelayPump, relayOn);
  loopStatusChange(statusOperatingPhase1);
  cycles = 0;
}

// Arret de la pompe à air - Fin du processus de lecture du niveau d'eau
// Envoi des données de lecture
void airPumpStop(int newStatus) {
  waterLevel = pressureKpaToMm(reads[nbConsReads-1]);
  digitalWrite(pinRelayPump, relayOff);
  loopStatusChange(newStatus);
  serialSendData();
  ethernetSendData();
}

// Pour vider le port série
void flushSerial(void) {
  while(1) {
    if (Serial.available() == 0)  {
      break;
    }
    else {
       Serial.read();
    }
  }
}


/**************************************************************************
**
**  Gestion du réseau
**
**************************************************************************/

// Lire les éventuelles commandes reçues sur le port ethernet et y répondre
void httpCommandProcess(void) {
  // Exemple d'envoi de commande : http://192.168.1.2/ReadWaterLevel
  // Déclaration des variables
  String chaineRecue = ""; // Pour y mettre la chaine reçue
  int comptChar = 0;       // Pour compter les caractères reçus

  // crée un objet client basé sur le client connecté au serveur
  EthernetClient httpClient = httpServer.available();
  if (httpClient) { // si l'objet client n'est pas vide
    // Initialisation des variables utilisées pour l'échange serveur/client
    chaineRecue = "";        // Vide le String de reception
    comptChar = 0;           // Compteur de caractères en réception à 0  
    if (httpClient.connected()) {       // Si que le client est connecté
      while (httpClient.available()) {  // Tant que des octets sont disponibles en lecture
        char c = httpClient.read();     // Lit l'octet suivant reçu du client (pour vider le buffer au fur à mesure !)
        comptChar++;                    // Incrémente le compteur de caractère reçus
        if (comptChar <= httpMaxChar) { // Les 25 premiers caractères sont suffisants pour analyses la requête
          chaineRecue += c;             // Ajoute le caractère reçu au String pour les N premiers caractères
        }
      } 
      // Si la chaine recue fait au moins 25 caractères ET qu'elle commence par "GET"
      if ( (chaineRecue.length() >= httpMaxChar) && (chaineRecue.startsWith("GET")) ) { 
        // Extrait à partir du 6ème caractère
        httpSend(httpClient,"OK");
        commandProcess(chaineRecue.substring(5));
      }
    } // Fin while client connected
    delay(10);           // On donne au navigateur le temps de recevoir les données
    httpClient.stop();   // Fermeture de la connexion avec le client
  }
}  

void httpSend(EthernetClient httpClient, String txt) {
  // Envoi d'une entete standard de réponse http
  httpClient.print("HTTP/1.1 20");
  if (txt.length() == 0) {
    //httpClient.println("HTTP/1.1 204 OK");
    httpClient.print("4");
  }
  else {
    //httpClient.println("HTTP/1.1 200 OK");
    httpClient.print("0");
  }
  httpClient.println(" OK");
  //httpClient.println("HTTP/1.1 200 OK");
  httpClient.println("Content-Type: text/html");
  httpClient.println("Connection: close");
  httpClient.println();
  // Affiche chaines caractères simples
  if (txt.length() > 0) {
    httpClient.println(txt);
  }
  delay(200);
}

// Envoi une demande au server NTP dont l'adresse est en paramètre
unsigned long sendNtpPacket(char* address) {
  // Initialise le buffer à 0 par défaut
  memset(packetBuffer, 0, ntpPacketSize);
  // Initialisée la demande NTP
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;            // Stratum, or type of clock
  packetBuffer[2] = 6;            // Polling Interval
  packetBuffer[3] = 0xEC;         // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // Envoi du paquet IP de demande NTP
  udp.beginPacket(address, 123);  // Les requetes NTP se font sur le port 123
  udp.write(packetBuffer, ntpPacketSize);
  udp.endPacket();
}

void getNtpTime(void) {
  // Envoyer un packet Ntp au server
  sendNtpPacket(timeServer);
  // attendre la réponse du server NTP. 100 ms pour un server local est suffisant. Prévoir plus pour un server Internet
  delay(100);
  if (udp.parsePacket()) {
    // Le server NTP a répondu. 
    // Lire lire données reçues dans le buffer
    udp.read(packetBuffer, ntpPacketSize); 
    // l(heure se trouve codée sur 4 octets à partir de l'octet 40
    // soit 2 'mots' à extraire
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // Combiner les 4 octets (2 'mots') dans un entier long pour avoir l'heure NTP (le nombre de secondes depuis le 1er janvier 1900)
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    // Convertir l'heure NTP en temps usuel. Soustraire 70 ans pour avoir l'"epoch" (le temps UNIX)
    unsigned long epoch = secsSince1900 - seventyYears;

    // Récupérer Heures, Minutes et Secondes
    ntpHour = (epoch  % 86400L) / 3600;    // Il y a 86400 secondes par jour
    ntpMinute = (epoch  % 3600) / 60;      // il y a 3600 secondes par minute
    ntpSecond = epoch % 60;
    
    // Envoi de l'heure au terminal série
    Serial.print("l'heure GMT est ");      // GMT (Greenwich Meridian Time) ou heure TU, ou encore UTC
    Serial.print(ntpHour); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if (ntpMinute < 10) {
      // Pour les 10 premières minutes, il faut plcer d'abord un "0"
      Serial.print('0');
    }
    Serial.print(ntpMinute); 
    Serial.print(':');
    if (ntpSecond < 10) {
      // Pour les 10 premières secondes, il faut plcer d'abord un "0"
      Serial.print('0');
    }
    Serial.println(ntpSecond); 
  }
}

// Prochaine lecture de l'heure
unsigned long calcNextNtp(unsigned long currentMillis) {
  return(currentMillis + (ntpEveryMinutes * 60L * 1000L));
}



/**************************************************************************
**
**  Lecture de la pression
**
**************************************************************************/



/**************************************************************************
**
**  Lecture de la pression du composant Freescale MPX5050DP
**    Valeur d'isopression :  32  => O kPa
**    Valeur max :            1016 => 50 kPa
**    Valeur min :            11  => dépression
**
**    Max 3 mètres d'eau, soit 30 kPa
**
**    1 bar = 1OO kPa
**
**************************************************************************/
const float pressureBitMin = 32.0;      // Valeur brute en isopression
const float pressureBitMax = 1016.0;    // Valeur brute maxi
const float pressureKPaMin = 0.0;       // kPa en isopression
const float pressureKPaMax = 50.0;      // kPa maxi
const float pressureMmPerKpa = 100.0;   // Coefficient à appliquer aux kPa pour avoir des millimètres d'eau

// Vérifie l'évolution des mesures
// Renvoi:
//    0 -> pas assez de mesure
//    1 -> il y a au moins une augmentation dans la série
//    2 -> toutes les mesures montrent une diminution 
int pressureCheckEvolution(void) {
  if (reads[nbConsReads-1] == NULL) {
    //Serial.println(reads[nbConsReads-1]);
    return(0);  
  }
  for (int i=1; i<nbConsReads-1; i++) {
    if (reads[i-1] < reads[i]) {
      //Serial.println(i);
      //Serial.print(reads[i-1]);
      //Serial.print(" < ");
      //Serial.println(reads[i]);
      return(1);
    }
  }
  //Serial.println("OK");
  return(2);
}


// Ajoute une mesure, faite sur la broche passée en paramètre, dans la pile des mesures
void addPressure(float p) {
  // Dépiler si la pile est pleine
  if (reads[nbConsReads-1] != NULL) {
    for (int i=1; i<nbConsReads; i++) {
      reads[i-1] = reads[i];
    }
    reads[nbConsReads-1] = NULL;
  }
  // Placer la valeur dans la première case vide
  for (int i=0; i<nbConsReads; i++) {
    if (reads[i] == NULL) {
      reads[i] = p;
      break;
    }
  }
}

// Effectue une mesure sur la broche passée en paramètre et renvoi une valeur moyenne exprimée en kPa
float pressureRead(int pin) {
  long sumReads = 0;
  float avgReads = 0;
  for (int i=0; i<nLoop; i++) {
     sumReads += pressureReadAnalogPin(pin);
     delay(5);
  }
  avgReads = (float)sumReads / (float)nLoop;
  //Serial.println(avgReads, 2);
  return (pressureRawToKpa(avgReads));
}

// Lit la broche analogique passée en paramètre et renvoi une valeur brute
int pressureReadAnalogPin(int pin) {
  int r = analogRead(pin);
  //float p = (r - pressureBitMin) / (pressureBitMax - pressureBitMin) * (pressureKPaMax - pressureKPaMin);
  //return p;
  return (r);
}

// Convertir une valeur brute lue sur une broche en kPa
float pressureRawToKpa(float avgR) {
  return (avgR - pressureBitMin) / (pressureBitMax - pressureBitMin) * (pressureKPaMax - pressureKPaMin);
}

//Transforme les kPa en millimètres d'eau
float pressureKpaToMm(float kpa) {
  return kpa * pressureMmPerKpa; 
}

/**************************************************************************
**
**  MPX5050DP DataSheet
**    Pressure range      : From 0 to 50 kPa
**    Supply voltage (Vs) : 5 V
**    Min pressure offset : 0.2 V
**    Full scale ouput    : 4.7 V
**    Full scale span     : 4.5 V
**    Sensitivity         : 90 mV / kPa
**    Transfert function  : Vout = Vs * (P * 0.0018 + 0.04)
**    Valeur isopression  : 32
**
**************************************************************************/


/**************************************************************************
**
**  MPX5500DP DataSheet
**    Pressure range      : From 0 to 500 kPa
**    Supply voltage (Vs) : 5 V
**    Supply current (Io) : 10 mA max
**    Min pressure offset : 0.2 V
**    Full scale ouput    : 4.7 V
**    Full scale span     : 4.5 V
**    Sensitivity         : 9 mV / kPa
**    Transfert function  : Vout = Vs * (P * 0.0018 + 0.04)  ?
**    Valeur isopression  : 37
**
**************************************************************************/

