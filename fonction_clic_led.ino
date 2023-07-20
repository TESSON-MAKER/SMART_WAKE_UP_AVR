const int LED[] = {2, 3, 4, 5, 6};
const int BTN[] = {22, 23, 24, 25, 26};

const int NumLED = sizeof(LED) / sizeof(LED[0]);
const int NumBTN = sizeof(BTN) / sizeof(BTN[0]);

int lastButtonState[NumBTN] = {LOW}; // Tableau pour stocker l'etat precedent de chaque bouton
const int debounceDelay = 50;        // Delai en millisecondes pour le temps de rebond

////////////////////////////////////////SETUP////////////////////////////////////////////

void setup()
{
  for (int i = 0; i < NumLED; i++)
  {
    pinMode(LED[i], OUTPUT); // Configure les broches des LED en sortie
  }

  for (int i = 0; i < NumBTN; i++)
  {
    pinMode(BTN[i], INPUT); // Configure les broches des boutons en entree
  }
}

////////////////////////////////////////LOOP/////////////////////////////////////////////

void loop()
{
  static bool ledOn = false; // Variable pour stocker l'état de la LED (allumée ou éteinte)

  if (clic(0)) // Vérifie si le bouton 22 a été cliqué (le tableau commence à l'index 0)
  {
    if (!ledOn) // Si la LED est éteinte
    {
      digitalWrite(LED[0], HIGH); // Allume la LED à la pin 2 (correspondant à LED[0])
      ledOn = true; // Met à jour l'état de la LED
    }
    else // Si la LED est allumée
    {
      digitalWrite(LED[0], LOW); // Éteint la LED
      ledOn = false; // Met à jour l'état de la LED
    }

    delay(1000); // Attend une seconde pour éviter les rebonds du bouton
  }
}

////////////////////////////////////////Fonctions////////////////////////////////////////

/*---------------------------------------Clic---------------------------------------*/

bool clic(int num)
{
  int buttonState = digitalRead(BTN[num]);  // Lecture de l'etat du bouton correspondant au numero
  if (buttonState != lastButtonState[num])
  {
    delay(debounceDelay);                   // Delai de rebond pour eviter les faux declenchements
    buttonState = digitalRead(BTN[num]);    // Lecture de l'etat du bouton a nouveau
    if (buttonState == HIGH)
    {
      lastButtonState[num] = buttonState;   // Met a jour l'etat precedent du bouton
      return true;                          // Renvoie vrai si le bouton est presse
    }
  }
  lastButtonState[num] = buttonState; // Met a jour l'etat precedent du bouton
  return false;                       // Renvoie faux si le bouton n'est pas presse
}








