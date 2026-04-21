#include "Particle.h"
#include "MD_MAX72xx.h" // Include the MD_MAX72xx library for MAX7219/7221
#include "MD_Parola.h"  // Include the MD_Parola library for text display
#include "SPI.h"
#include "fonts.h"

// Define the number of devices (modules) in the chain
#define MAX_DEVICES 4

// Define the hardware type (check MD_MAX72xx.h for other types if needed)
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

// Define the pins connected to the MAX7219
#define CLK_PIN D1  // Clock pin
#define DATA_PIN D2 // Data In pin
#define CS_PIN D0   // Chip Select pin

#define BUZZER_PIN D3 // Buzzer pin

String displayText = "Akshay";

unsigned long lastUpdate = 0;
int lastMinute = -1;

int alarmHour = -1;
int alarmMinute = -1;
unsigned long buzzerStartTime = 0;
bool buzzerActive = false;

int setDisplayText(String newText)
{
  displayText = newText;
  return 1;
}

// Set alarm via cloud function — format "HH:MM" (24-hour)
int setAlarm(String timeStr)
{
  int h = timeStr.substring(0, 2).toInt();
  int m = timeStr.substring(3, 5).toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59)
    return -1;
  alarmHour = h;
  alarmMinute = m;
  return 1;
}

void playBuzzer()
{
  tone(BUZZER_PIN, 1000); // 1 kHz tone
  buzzerStartTime = millis();
  buzzerActive = true;
}

void playMelody()
{
  // "I  -  LOVE  -  YOU"  (original motif)
  //  G5    E5 G5 C6     G5  E5 C5
  int notes[]     = { 784, 0,  659, 784, 1047, 0,  784, 659, 523 };
  int durations[] = { 500, 80, 180, 180, 400,  80, 200, 200, 500 };
  int count = sizeof(notes) / sizeof(notes[0]);

  for (int i = 0; i < count; i++)
  {
    if (notes[i] > 0)
      tone(BUZZER_PIN, notes[i], durations[i]);
    delay(durations[i] + 40);
  }
  noTone(BUZZER_PIN);
}

int triggerBuzzer(String arg)
{
  if (arg == "melody")
    playMelody();
  else
    playBuzzer();
  return 1;
}

// Create a new MD_Parola object (software SPI)
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

void setup()
{
  pinMode(BUZZER_PIN, OUTPUT);
  P.begin();
  P.setFont(font7Seg);
  Time.zone(5.5);
  Particle.variable("displayText", displayText);
  Particle.function("setText", setDisplayText);
  Particle.function("setAlarm", setAlarm);
  Particle.function("triggerBuzzer", triggerBuzzer);
}

void loop()
{
  if (buzzerActive && millis() - buzzerStartTime >= 2000)
  {
    noTone(BUZZER_PIN);
    buzzerActive = false;
  }

  int currentMinute = Time.minute();
  if (currentMinute != lastMinute)
  {
    char timeBuffer[9];
    int hour = Time.hourFormat12();
    int minute = Time.minute();
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hour, minute);
    displayText = String(timeBuffer);
    lastMinute = currentMinute;
    P.displayClear();
    P.displayReset();
    P.displayText(displayText.c_str(), PA_CENTER, P.getSpeed(), 1000, PA_PRINT, PA_NO_EFFECT);

    if (alarmHour == Time.hour() && alarmMinute == currentMinute)
      playBuzzer();
  }

  P.displayAnimate();
}