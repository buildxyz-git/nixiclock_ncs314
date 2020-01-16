//driver for NCM107+NCT318+NCT818, NCS312, NCS314 HW2.x (registers HV5122)
//driver version 1.1
//1 on register's output will turn on a digit 

//v1.1 Mixed up on/off for dots

#include "NixieClock_NCS314_utils.h"

#define UpperDotsMask 0x80000000
#define LowerDotsMask 0x40000000

unsigned int SymbolArray[10] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 }; // 10 bit positions
bool UD, LD; // DOTS control

void initSPI()
{
  SPI.begin();
  SPI.setDataMode (SPI_MODE2);            // Mode 2 SPI
  SPI.setClockDivider(SPI_CLOCK_DIV8);    // SCK = 16MHz/128= 125kHz
  pinMode(HV_CE_PIN, OUTPUT);
}

void doIndication(int test)
{
  static unsigned long lastTimeInterval1Started;
  unsigned long Var32 = 0;
  long digits = stringToDisplay.toInt(); // i.e "031559" -> 031559

  if ((micros() - lastTimeInterval1Started) < FPS_LIMIT) { return; }

  lastTimeInterval1Started = micros();
  digitalWrite(HV_CE_PIN, LOW); 

  if (TubesOn) {
  //-------- REG 1 ----------------------------------------------------------
  Var32 = 0;
 
  Var32 |= (unsigned long)(SymbolArray[digits % 10] & doEditBlink(5)) << 20; // s2
  digits = digits / 10; // shift digit right

  Var32 |= (unsigned long)(SymbolArray[digits % 10] & doEditBlink(4)) << 10; // s1
  digits = digits / 10;

  Var32 |= (unsigned long) (SymbolArray[digits % 10] & doEditBlink(3));     // m2
  digits = digits / 10;

  if (LD) { Var32 |= LowerDotsMask; }
  else { Var32 &= ~LowerDotsMask; }
  
  if (UD) { Var32 |= UpperDotsMask; }
  else { Var32 &= ~UpperDotsMask; }  

  spi_update(Var32);

 //-------- REG 0 ----------------------------------------------------------
  Var32 = 0;
  
  Var32 |= (unsigned long)(SymbolArray[digits % 10] & doEditBlink(2)) << 20; // m1
  digits = digits / 10;

  Var32 |= (unsigned long)(SymbolArray[digits % 10] & doEditBlink(1)) << 10; // h2
  digits = digits / 10;

  Var32 |= (unsigned long)SymbolArray[digits % 10] & doEditBlink(0);        // h1

  if (LD) { Var32 |= LowerDotsMask; }
  else { Var32 &= ~LowerDotsMask; }
  
  if (UD) { Var32 |= UpperDotsMask; }
  else { Var32 &= ~UpperDotsMask; }  
     
  spi_update(Var32);

  digitalWrite(HV_CE_PIN, HIGH);    
  }
  else {
    spi_update(Var32);
  }
}

word doEditBlink(int pos)
{
  if (!BlinkUp) { return 0xFFFF; }
  if (!BlinkDown) { return 0xFFFF; }
  int lowBit = blinkMask >> pos;
  lowBit = lowBit & B00000001;
  
  static unsigned long lastTimeEditBlink = millis();
  static bool blinkState = false;
  word mask = 0xFFFF;
  static int tmp = 0;//blinkMask;
  if ((millis() - lastTimeEditBlink) > 300) 
  {
    lastTimeEditBlink = millis();
    blinkState = !blinkState;
    if (blinkState) tmp = 0;
      else tmp = blinkMask;
  }
  if (((dotPattern & ~tmp) >> 6) & (1 == 1)) { LD = true; }
  else { LD = false; } 
  if (((dotPattern & ~tmp )>> 7) & (1 == 1)) { UD=true; } 
  else { UD = false; } 
      
  if ((blinkState == true) && (lowBit == 1)) { mask = 0x3C00; } //mask = B00111100;

  return mask;
}

void spi_update(unsigned long var32)
{
  SPI.transfer(var32 >> 24);
  SPI.transfer(var32 >> 16);
  SPI.transfer(var32 >> 8);
  SPI.transfer(var32);
}

word blankDigit(int pos)
{
  int lowBit = blankMask >> pos;
  lowBit = lowBit & B00000001;
  word mask = 0;
  if (lowBit == 1) mask = 0xFFFF;
  return mask;
}