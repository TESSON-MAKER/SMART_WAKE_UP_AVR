#include <Arduino.h>
#include <U8g2lib.h>

#define DS3231_I2C_ADDRESS 0x68

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, /* clock=*/ 13, /* data=*/ 11, /* CS=*/ 10, /* reset=*/ 8);


const int LED[] = {2, 3, 4, 5, 6};
const int BTN[] = {22, 23, 24, 25, 26};
const int KEY = 30;


const int NumLED = sizeof(LED) / sizeof(LED[0]);
const int NumBTN = sizeof(BTN) / sizeof(BTN[0]);

const int TMP = A0;

int lastButtonState[NumBTN] = {LOW};          // Tableau pour stocker l'etat precedent de chaque bouton
unsigned long lastDebounceTime[NumBTN] = {0}; // Tableau pour stocker le dernier temps de rebond de chaque bouton
unsigned long buttonPressTime[NumBTN] = {0};  // Tableau pour stocker le temps de pression de chaque bouton
unsigned long delayStart = 0;                 // Déclaration de la variable globale

int hour;
int minute;
int second;
int day;
int month;
int year;
int dayWeek;
int minuteUP = 0;
int hourUP = 0;

bool keyState = false;
bool update = false;
int move = 0;

bool UP = false;
bool alarm = false;

//URM37
int URECHO = 8;         // PWM Output 0-50000US,Every 50US represent 1cm
int URTRIG = 9;         // trigger pin
int BUZZER = 12;        // Buzzer Pin

unsigned int DistanceMeasured = 0;

////////////////////////////////////////SETUP////////////////////////////////////////////
void setup()
{
  Serial.begin(9600);
  
  Wire.begin();                         // Initialise la communication I2C

  u8g2.begin();                         // Initialise l'affichage
  u8g2.setFontMode(1);                  // Activation du mode de police transparente
  u8g2.setFontDirection(0);             // Configuration de la direction de la police (0: gauche vers la droite, 1: droite vers la gauche)
  
  for (int i = 0; i < NumLED; i++)
  {
    pinMode(LED[i], OUTPUT);            // Configure les broches des LED en sortie
    analogWrite(LED[i], 0);             // Initialise la luminosite des LED a 0
  }

  for (int i = 0; i < NumBTN; i++)
  {
    pinMode(BTN[i], INPUT);             // Configure les broches des boutons en entree
  }

  pinMode(KEY, INPUT);
  pinMode(TMP, INPUT);

  pinMode(URTRIG, OUTPUT);                   // A low pull on pin COMP/TRIG
  digitalWrite(URTRIG, HIGH);                // Set to HIGH
  pinMode(URECHO, INPUT);                    // Sending Enable PWM mode command
}

////////////////////////////////////////LOOP/////////////////////////////////////////////
void loop()
{
  keyState = digitalRead(KEY);
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_profont15_tf);

  switch (keyState)
  {
    case false:
      initialization();
      displayDate();          // Affiche la date et l'heure sur l'ecran
      displaySensorTMP(10);
      wakeUp();
      ledOFF(0);
      ledOFF(1);
      ledOFF(2);
      ledOFF(3);
      break;

    case true:
      settings();
      ledON(0); 
      ledON(1); 
      ledON(2); 
      ledON(3); 
      break;
  }

  u8g2.sendBuffer();
  delay(10);
}

////////////////////////////////////////Fonctions////////////////////////////////////////

/*---------------------------------------Conversions--------------------------------*/
byte decToBcd(byte val) { return ((val / 10 * 16) + (val % 10)); }
byte bcdToDec(byte val) { return ((val / 16 * 10) + (val % 16)); }


/*---------------------------------------Clic---------------------------------------*/
bool clic(int num) 
{
  int buttonState = digitalRead(BTN[num]);    // Lecture de l'état du bouton correspondant au numéro

  if (buttonState != lastButtonState[num]) 
  {
    lastDebounceTime[num] = millis();         // Enregistre le temps de rebond
    
    if (buttonState == HIGH) 
    {
      lastButtonState[num] = buttonState;     // Met à jour l'état précédent du bouton
      return true;                            // Renvoie vrai si le bouton est pressé
    }
  }

  lastButtonState[num] = buttonState;         // Met à jour l'état précédent du bouton
  return false;                               // Renvoie faux si le bouton n'est pas pressé
}

/*---------------------------------------Long_Clic----------------------------------*/
bool longClic(int num) 
{
  int buttonState = digitalRead(BTN[num]);    // Lecture de l'état du bouton correspondant au numéro

  if (buttonState != lastButtonState[num]) 
  {
    lastDebounceTime[num] = millis();         // Enregistre le temps de rebond
    
    if (buttonState == HIGH) 
    {
      lastButtonState[num] = buttonState;     // Met à jour l'état précédent du bouton
      buttonPressTime[num] = millis();        // Enregistre le temps de la pression du bouton
      return true;                            // Renvoie vrai si le bouton est pressé
    }
  }
  else if (buttonState == HIGH && (millis() - buttonPressTime[num]) >= 1000)  // Si le bouton est maintenu enfoncé pendant plus d'une seconde, effectuer des incréments de 1
  {
    
    lastButtonState[num] = buttonState;       // Met à jour l'état précédent du bouton
    
    static unsigned long lastDelayTime = 0;
    if ((millis() - lastDelayTime) >= 200)
    {
      lastDelayTime = millis();
      return true;
    }
  }

  lastButtonState[num] = buttonState;         // Met à jour l'état précédent du bouton
  return false;                               // Renvoie faux si le bouton n'est pas pressé
}

/*---------------------------Afficher sur l'ecran la date et l'heure----------------*/
void displayDate()
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS); // Demarre la communication avec le module RTC
  Wire.write(0);                              // Envoie l'adresse de depart pour lire les donnees
  Wire.endTransmission();                     // Termine la transmission
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);    // Demande la lecture de 7 octets de donnees depuis le module RTC

  second = bcdToDec(Wire.read() & 0x7f);      // Bits 7-0 : Secondes (valeurs de 00 a 59)
  minute = bcdToDec(Wire.read());             // Minutes (valeurs de 00 a 59)
  hour = bcdToDec(Wire.read() & 0x3f);        // Bits 7-6 : 12/24 heures, Bits 5-0 : Heures (valeurs de 00 a 23 ou de 00 a 12 selon le format)
  dayWeek = bcdToDec(Wire.read());            // Jour de la semaine (valeurs de 1 a 7, 1 pour dimanche)
  day = bcdToDec(Wire.read());                // Jour (valeurs de 01 a 31)
  month = bcdToDec(Wire.read());              // Mois (valeurs de 01 a 12)
  year = bcdToDec(Wire.read());               // Annee (valeurs de 00 a 99)

  u8g2.setCursor(0, 10);
  u8g2.print("Hour: ");
  u8g2.print(hour < 10 ? "0" : "");
  u8g2.print(hour);
  u8g2.print(":");
  u8g2.print(minute < 10 ? "0" : "");
  u8g2.print(minute);
  u8g2.print(":");
  u8g2.print(second < 10 ? "0" : "");
  u8g2.print(second);
  
  u8g2.setCursor(0, 25);
  u8g2.print("Date: ");
  u8g2.print(day < 10 ? "0" : "");
  u8g2.print(day);
  u8g2.print("/");
  u8g2.print(month < 10 ? "0" : "");
  u8g2.print(month);
  u8g2.print("/20");
  u8g2.print(year < 10 ? "0" : "");
  u8g2.print(year);
  
  const char *days[] = {"NA", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
  u8g2.setCursor(0, 40);
  u8g2.print("Day: ");
  u8g2.print(days[dayWeek]);
}

/*--------------------------Ecran Reglages------------------------------------------*/
void settings()
{
  update=true;

  if (clic(0)) move++;
  if (clic(1)) move--;
  if (move > 9) move = 0;
  if (move < 0) move = 9;
  
  switch (move)
  {
    case 0:
      handling(second, "sec", 59, 0);
      break;
    case 1:
      handling(minute, "min", 59, 0);
      break;
    case 2:
      handling(hour, "hour", 23, 0);
      break;
    case 3:
      handling(dayWeek, "dayW", 7, 1);
      break;
    case 4:
      handlingDay();
      break;
    case 5:
      handlingMonth();
      break;
    case 6:
      handlingYear();
      break;
    case 7:
      handling(minuteUP, "minUP", 59, 0);
      break;
    case 8:
      handling(hourUP, "hourUP", 23, 0);
      break;
    case 9:
      switchAlarm();
      break;
  }
}

/*------------------------------Mise à jour de la date et de l'heure---------------*/
void initialization()
{
  move = 0;

  if (update)
  {
    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    Wire.write(0);
    Wire.write(decToBcd(second));
    Wire.write(decToBcd(minute));
    Wire.write(decToBcd(hour));
    Wire.write(decToBcd(dayWeek));
    Wire.write(decToBcd(day));
    Wire.write(decToBcd(month));
    Wire.write(decToBcd(year));
    Wire.endTransmission();
    update = false;
  }
}

/*------------------------------Allumage progressif de la LED-----------------------*/
void ledON(int num)
{
  digitalWrite(LED[num], HIGH);
}

/*------------------------------Eteinte progressive de la LED-----------------------*/
void ledOFF(int num)
{
  digitalWrite(LED[num], LOW);
}

/*------------------------Implementation des donnees--------------------------------*/
void handling(int& data, const char* title, int max, int min)
{
  if (longClic(2)) {data++;}
  if (longClic(3)) {data--;}
  if (data>max) data=min;
  if (data<min) data=max;
  u8g2.setCursor(0, 10);  // Positionne le curseur a la position (0, 10)

  u8g2.print("Setting ");
  u8g2.print(title);
  u8g2.print(":");

  u8g2.print(data);
}

/*------------------------Implementation des donnees (jours)-------------------------*/
void handlingDay()
{
  bool isLeapYear = (year %4 == 0 && year %100 != 0) || (year %400 == 0);

  if (longClic(2)) {day++;}
  if (longClic(3)) {day--;}
  if ((month == 4 || month == 6 || month == 9 || month == 11) && (day > 30)) day=0;
  if ((month == 4 || month == 6 || month == 9 || month == 11) && (day < 0)) day=30;
  if ((month == 1 || month == 3 || month == 5 || month == 7 || month == 9 || month == 11) && (day > 31)) day=0;
  if ((month == 1 || month == 3 || month == 5 || month == 7 || month == 9 || month == 11) && (day < 0)) day=31;
  if ((month == 2) && (day > 28)) day=0;
  if ((month == 2) && (day < 0)) day=28;
  if (month == 2 && isLeapYear && day > 29) day = 0;
  if (month == 2 && !isLeapYear && day > 28) day = 0;
  if (month == 2 && isLeapYear && day < 0) day = 29;
  if (month == 2 && !isLeapYear && day < 0) day = 28;

  u8g2.setCursor(0, 10);                // Positionne le curseur a la position (0, 10)
      
  u8g2.print("Setting ");
  u8g2.print("day");
  u8g2.print(":");

  u8g2.print(day);
}

/*------------------------Implementation des donnees (mois)-------------------------*/
void handlingMonth()
{
  bool isLeapYear = (year %4 == 0 && year %100 != 0) || (year %400 == 0);

  if (longClic(2)) {month++;}
  if (longClic(3)) {month--;}
  if (month>12) month=1;
  if (month<1) month=12;

  
  if ((month == 4 || month == 6 || month == 9 || month == 11) && (day > 30)) day=30;  //Cas ou dans 1 mois, il n'y a que 30 jours
  

  if (month == 2 && isLeapYear && day > 29) day = 29;                                 //Cas de Fevrier dans les annees bissextiles (29 jours)
  if (month == 2 && !isLeapYear && day > 28) day = 28;                                //Cas de Fevrier hors annees bissextiles (28 jours)

  u8g2.setCursor(0, 10);                                                              // Positionne le curseur a la position (0, 10)
      
  u8g2.print("Setting ");
  u8g2.print("month");
  u8g2.print(":");

  u8g2.print(month);
}

/*------------------------Implementation des donnees (annees)----------------------*/
void handlingYear()
{
  bool isLeapYear = (year %4 == 0 && year %100 != 0) || (year %400 == 0);

  if (longClic(2)) {year++;}
  if (longClic(3)) {year++;}
  if (year>99) year=0;
  if (year<0) year=99;

  if (month == 2 && isLeapYear && day > 29) day = 29;           // Cas de Fevrier dans les annees bissextiles (29 jours)
  if (month == 2 && !isLeapYear && day > 28) day = 28;          // Cas de Fevrier hors annees bissextiles (28 jours)

  u8g2.setCursor(0, 10);                                        // Positionne le curseur a la position (0, 10)
      
  u8g2.print("Setting ");
  u8g2.print("year");
  u8g2.print(":");

  u8g2.print(year);
}

/*------------------------Activation/Desactivation de l'alarme----------------------*/
void switchAlarm()
{
  if (clic(2)) 
  {
    update = true;
    alarm = !alarm;  // Utilisez l'opérateur d'affectation simple pour basculer l'état de l'alarme
  }

  u8g2.setCursor(0, 10);  // Positionne le curseur à la position (0, 10)
  u8g2.print("Setting alarm");
  u8g2.print(":");
  u8g2.print(alarm ? " ON" : " OFF");  // Utilisez une condition ternaire pour afficher "ON" ou "OFF" en fonction de l'état
}

/*----------------------------------Temperature-------------------------------------*/
float displaySensorTMP(int sample)
{
  int val[sample] = {0};                                        // Tableau pour stocker les valeurs du capteur de temperature

  for (int i = 0; i < sample; i++)
  {
    val[i] = analogRead(TMP);                                   // Lecture de la valeur du capteur de temperature
  }

  float average = 0.0;
  for (int i = 0; i < sample; i++)
  {
    average += val[i];                                          // Calcul de la somme des valeurs
  }
  average /= sample;                                            // Calcul de la moyenne en divisant la somme par le nombre de valeurs

  float degC = (((average / 1024.0) * 5.0) - 0.5) * 100;        // Conversion de la valeur en degres Celsius
  int degCInt = static_cast<int>(degC);                         // Conversion de la valeur en entier

  u8g2.setCursor(0, 55);                                        // Positionne le curseur a la position (0, 55)
  u8g2.print("Temp: ");
  u8g2.print(degCInt);
  u8g2.print(" C");

  return degC;                                                  // Retourne la valeur moyenne de la temperature
}

/*---------------------------------URM37+Réveil-------------------------------------------*/
void wakeUp()  
{
  if (alarm)
  {
    u8g2.setCursor(90, 55);
    u8g2.print(hourUP < 10 ? "0" : "");
    u8g2.print(hourUP);
    u8g2.print(":");
    u8g2.print(minuteUP < 10 ? "0" : "");
    u8g2.print(minuteUP);

    if ((second == 0) && (minute == minuteUP) && (hour == hourUP) || (UP == true))
    {
      UP = true;
      tone(BUZZER, 600);
      digitalWrite(URTRIG, LOW);
      digitalWrite(URTRIG, HIGH);  

      unsigned long LowLevelTime = pulseIn(URECHO, LOW) ;
      DistanceMeasured = LowLevelTime / 50;
      if (DistanceMeasured <= 10)
      {
        UP=false;
        noTone(BUZZER);
      }
    }
    else
    {
      UP = false;
      noTone(BUZZER);
      digitalWrite(URTRIG, LOW);
    }
  }
  else
  {
    u8g2.setCursor(90, 55);
    u8g2.print("OFF");
  }
}