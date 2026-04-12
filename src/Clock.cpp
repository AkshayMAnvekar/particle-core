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

String displayText = "Akshay"; // The text to display and expose as a cloud variable

unsigned long lastUpdate = 0;
int lastMinute = -1;

// Function to update displayText from the cloud
int setDisplayText(String newText)
{
  displayText = newText;
  return 1;
}

// Create a new MD_Parola object (software SPI)
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

void setup()
{
  P.begin();
  P.setFont(font7Seg);
  Time.zone(5.5);
  Particle.variable("displayText", displayText); // Register the cloud variable
  Particle.function("setText", setDisplayText);  // Register the cloud function
}

void loop()
{
  // if (P.displayAnimate())
  //   P.displayText(displayText, PA_CENTER, P.getSpeed(), 1000, PA_SCROLL_LEFT, PA_SCROLL_LEFT);

  int currentMinute = Time.minute();
  if (currentMinute != lastMinute)
  {
    char timeBuffer[9];
    int hour = Time.hourFormat12();
    int minute = Time.minute();
    // const char *ampm = Time.isAM() ? "AM" : "PM";
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hour, minute);
    displayText = String(timeBuffer);
    lastMinute = currentMinute;
    P.displayClear();
    P.displayReset();
    P.displayText(displayText.c_str(), PA_CENTER, P.getSpeed(), 1000, PA_PRINT, PA_NO_EFFECT);
  }

  P.displayAnimate();
}