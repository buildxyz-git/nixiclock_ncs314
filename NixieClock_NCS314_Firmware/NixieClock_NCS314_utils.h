#include <SPI.h>

#define HV_CE_PIN       10          // Chip enable pin for High voltage driver
#define FPS_LIMIT       16666

#define TESTING                 1
#define NOT_TESTING             0

// Global variable declarations:
extern String stringToDisplay;      // Display string 
extern bool UD, LD;                 // DOTS control;
extern bool BlinkUp;
extern bool BlinkDown;
extern byte blinkMask;              // bit mask for blinking digits (1 - blink, 0 - constant light)
extern int blankMask;               // bit mask for digits (1 - off, 0 - on)
extern byte dotPattern;             // bit mask for separating dots (1 - on, 0 - off)
extern bool TubesOn;                // Tube Timer flag
extern bool transactionInProgress;

extern unsigned int SymbolArray[10];

// Function prototypes:
void initSPI();
void doIndication(int test);
word doEditBlink(int pos);
word blankDigit(int pos);
void spi_update(unsigned long var32);