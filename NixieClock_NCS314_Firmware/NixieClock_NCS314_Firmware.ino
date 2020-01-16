/* BUILDXYZ port for GRA&AFCH NCS314 kit firmware:

Original firmware can be found here: https://github.com/afch/NixeTubesShieldNCS314

This firmware is still quite messy and could use a complete rewrite.

Updates include:
- General code refactoring
- Move most configuration parameters to NixieConfig.h
- Remove IR Remote functionality
- Enable/Disable GPS in config file
- Enable/Disable Debugging in config file
- Added some more tunes in config file
- Tube timer feature with on/off timer to turn tube off at night
- Temperature LED feature to reflect room temperature in LED color
- Enable/Disable Colons in config file
- Remove IR functionality
- Remove hours offset in menu
- Bugfix: Timer3 interrupt being used by tone library not allowing red LED PWM on D9, opt for block tone library 

*/

#include "NixieShield.h"

bool TubesOn = true;
bool transactionInProgress = false; //antipoisoning transaction

byte data[12];
byte addr[8];
int celsius, fahrenheit;

bool RTC_present;

// DS18S20 Temperature chip
OneWire ds(TEMP_PIN);
bool TempPresent = false;

String stringToDisplay = "808808"; // Content of this string will be displayed on tubes (must be 6 chars length)
int menuPosition = 0; 

byte blinkMask = B00000000;     //bit mask for blinking digits (1 - blink, 0 - constant light)
int blankMask = B00000000;      //bit mask for digits (1 - off, 0 - on)

byte dotPattern = B00000000;    //bit mask for separating dots (1 - on, 0 - off)
//B10000000 - upper dots
//B01000000 - lower dots

int RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year, RTC_day_of_week;

/*                            +----------+--------+-------+--------+--------+-----------+----+----+----+---------+-----+-------+------+----+----+----+----------+---------+-----------+-----------+-----------+----------+----------+-----------+
//                            |     0    |    1   |   2   |   3    |    4   |     5     | 6  | 7  | 8  |    9    | 10  | 11    | 12   | 13 | 14 | 15 |    16    |    17   |    18     |     19    |    20     |  21      |    22    |    23     |
//                            +----------+--------+-------+--------+--------+-----------+----+----+----+---------+-----+-------+------+----+----+----+----------+---------+-----------+-----------+-----------+----------+----------+-----------+
//                            |   Time   |  Date  | Alarm |  12/24 |   Temp | TubeTimer | HR | MM | SS | DateFmt | Day | Month | Year | HH | MM | SS | Alarm_on | HourFMT |  TempFMT  | TT_OFF_HH | TT_OFF_MM | TT_ON_HH | TT_OH_MM | TT_Enable */
byte parent[SETTINGS_COUNT] =     { NP,      NP,       NP,     NP,      NP,      NP,       0,  0,    0,       1,    1,     1,      1,    2,   2,   2,     2,         3,        4,           5,         5,         5,           5,          5 };
byte firstChild[SETTINGS_COUNT] =  { 6,       9,       13,     17,      18,      19,       0,  0,    0,      NC,    0,     0,      0,    0,   0,   0,     0,         0,        0,           0,         0,         0,           0,          0 };
byte lastChild[SETTINGS_COUNT] =   { 8,      12,       16,     17,      18,      23,       0,  0,    0,      NC,    0,     0,      0,    0,   0,   0,     0,         0,        0,           0,         0,         0,           0,          0 };
byte value[SETTINGS_COUNT] =       { 0,       0,       0,      0,        0,       0,       0,  0,    0,   EU_DF,    0,     0,      0,    0,   0,   0,     0,         24,       0,           0,         0,         0,           0,          0 };
int16_t maxValue[SETTINGS_COUNT] = { 0,       0,       0,      0,        0,       0,       23, 59,   59,   US_DF,   31,    12,     99,   23,  59,  59,    1,         24,   FAHRENHEIT,      23,        59,        23,          59,         1 };
int16_t minValue[SETTINGS_COUNT] = { 0,       0,       0,      12,       0,       0,       00, 00,   00,   EU_DF,   1,     1,      00,   00,  00,  00,    0,         12,    CELSIUS,        00,        00,        00,          00,         0 };

byte blinkPattern[SETTINGS_COUNT] =  {  
  B00000000, //0
  B00000000, //1
  B00000000, //2
  B00000000, //3
  B00000000, //4
  B00000000, //5
  B00000011, //6
  B00001100, //7
  B00110000, //8
  B00111111, //9
  B00000011, //10
  B00001100, //11
  B00110000, //12
  B00000011, //13
  B00001100, //14
  B00110000, //15
  B11000000, //16
  B00001100, //17
  B00111111, //18
  B00000011, //19
  B00001100, //20
  B00000011, //21
  B00001100, //22
  B00001100  //23 - TT_Enable
};

bool editMode = false;
bool BlinkUp = false;
bool BlinkDown = false;
bool RGBLedsOn = false;  // set from EEPROM later

long downTime = 0;
long upTime = 0;
const long settingDelay = 150;
unsigned long enteringEditModeTime = 0;

//buttons pins declarations
ClickButton modeButton(SET_PIN, LOW, CLICKBTN_PULLUP);
ClickButton upButton(UP_PIN, LOW, CLICKBTN_PULLUP);
ClickButton downButton(DOWN_PIN, LOW, CLICKBTN_PULLUP);

byte RedLight = 255;
byte GreenLight = 0;
byte BlueLight = 0;

#ifdef LEGACY_LEDS
unsigned long prevTime4FireWorks = 0; //time of last RGB changed
int rotator = 0; //index in array with RGB "rules" (increse by one on each 255 cycles)
int cycle = 0; //cycles counter

int fireforks[] = {0, 0, 1, //1
                   -1, 0, 0, //2
                   0, 1, 0, //3
                   0, 0, -1, //4
                   1, 0, 0, //5
                   0, -1, 0
                  }; //array with RGB rules (0 - do nothing, -1 - decrese, +1 - increse

#endif

#ifdef TEMP_LEDS
FadeLed redLed(RED_LED_PIN);
FadeLed greenLed(GREEN_LED_PIN);
FadeLed blueLed(BLUE_LED_PIN);

unsigned long prevTime_temp_leds = 0;
unsigned long prevTime_temp_leds_step = 0;
float f_temp = 0;
#endif

int functionDownButton = 0;
int functionUpButton = 0;
bool LEDsLock = false;

//antipoisoning transaction
bool modeChangedByUser = false;
unsigned long modesChangePeriod = TIMEMODEPERIOD;  // Initial value to 60000
//end of antipoisoning transaction

Rtttl Rtttl(BUZZER_PIN);

#ifdef GPS_ENABLE
bool GPS_sync_flag = false;
#endif

/*******************************************************************************************************
  Init Program
*******************************************************************************************************/
void setup()
{
  Wire.begin();

  #ifdef DEBUG
  Serial.begin(115200);
  #endif

  #if (defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)) && defined(GPS_SYNC_ENABLE)
  Serial1.begin(9600);
  #endif
  #if (defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)) && defined(NTP_SYNC_ENABLE)
  Serial1.begin(9600);
  #endif

  init_EEPROM();

  #ifdef LEGACY_LEDS
  pinMode(RedLedPin, OUTPUT);
  pinMode(GreenLedPin, OUTPUT);
  pinMode(BlueLedPin, OUTPUT);
  #else
  FadeLed::setInterval(TEMP_LED_STEP);
  redLed.setTime(TEMP_LED_FADE_TIME, true);
  greenLed.setTime(TEMP_LED_FADE_TIME, true);
  blueLed.setTime(TEMP_LED_FADE_TIME, true);
  #endif

  // SPI setup
  initSPI();

  //buttons pins inits
  pinMode(SET_PIN,  INPUT_PULLUP);
  pinMode(UP_PIN,  INPUT_PULLUP);
  pinMode(DOWN_PIN,  INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  // TODO move to NixieSheild.h
  //buttons objects inits
  modeButton.debounceTime   = DEBOUNCE_MS;   // Debounce timer in ms
  modeButton.multiclickTime = 30;  // Time limit for multi clicks
  modeButton.longClickTime  = 2000; // time until "held-down clicks" register

  upButton.debounceTime   = DEBOUNCE_MS;   // Debounce timer in ms
  upButton.multiclickTime = 30;  // Time limit for multi clicks
  upButton.longClickTime  = 2000; // time until "held-down clicks" register

  downButton.debounceTime   = DEBOUNCE_MS;   // Debounce timer in ms
  downButton.multiclickTime = 30;  // Time limit for multi clicks
  downButton.longClickTime  = 2000; // time until "held-down clicks" register

  doTest();

  #ifdef LEGACY_LEDS
  if (LEDsLock == 1) { setLEDsFromEEPROM(); }
  #endif

  getRTCTime();
  byte prevSeconds = RTC_seconds;
  unsigned long RTC_ReadingStartTime = millis();
  RTC_present = true;
  while (prevSeconds == RTC_seconds)
  {
    getRTCTime();
    if ((millis() - RTC_ReadingStartTime) > 3000)
    {
      #ifdef DEBUG
      Serial.println(" [!] RTC not responding");
      #endif
      RTC_present = false;
      break;
    }
  }
  
  setTime(RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year);
}

/***************************************************************************************************************
  MAIN Programm
***************************************************************************************************************/
void loop() {
  if (((millis() % RTC_SYNC) == 0) && (RTC_present)) //synchronize with RTC every 10 seconds
  {
    getRTCTime();
    setTime(RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year);

    #ifdef DEBUG
    Serial.print("[i] RTC Sync: ");
    Serial.print("\n\tHours: ");
    Serial.print(RTC_hours);
    Serial.print(" Minutes: ");
    Serial.print(RTC_minutes);
    Serial.print(" Seconds: ");
    Serial.print(RTC_seconds);
    Serial.print(" Day: ");
    Serial.print(RTC_day);
    Serial.print(" Month: ");
    Serial.print(RTC_month);
    Serial.print(" Year: ");
    Serial.println(RTC_year);
    #endif
  }

  // Do GPS Sync
  #if (defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)) && defined(GPS_ENABLE)
  GetDataFromSerial1();

  if ((millis() % 10001) == 0) //synchronize with GPS every 10 seconds
  {
    SyncWithGPS();
    #ifdef DEBUG
    Serial.println(F("Sync from GPS"));
    #endif
  }
  #endif

  // Do NTP sync
  #if (defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)) && defined(NTP_SYNC_ENABLE)
  GetNTPData();
  if ((millis() - prev_ntp_sync > NTP_SYNC_INTERVAL) && ntp_sync_pending)
  {
       #ifdef DEBUG
    Serial.print("[*] Sync from NTP server: ");
    Serial.println(ntp_buffer);
    #endif
  
    ntp_rtcupdate();

    ntp_sync_pending = false;
    prev_ntp_sync = millis();
  }
  #endif

  // Blocking call to updatemelody() other song timming is off 
  while(true){ 
   if (!Rtttl.updateMelody()){ break; }
  } 
  
  // *** Do Legacy LED animation ***
  #ifdef LEGACY_LEDS
  if ((millis() - prevTime4FireWorks) > 5)
  {
    rotateFireWorks(); //change color (by 1 step)
    prevTime4FireWorks = millis();
  }
  #endif

  // *** Do Temperature LED animation ***
  #ifdef TEMP_LEDS

  FadeLed::update();

  if ((millis() - prevTime_temp_leds) > TEMP_LEDS_INTERVAL && TubesOn){
    prevTime_temp_leds = millis();
    f_temp = getTemperature(value[DegreesFormatIndex]) / 10;

    #ifdef DEBUG
    Serial.println("[i] Temperature LEDs triggered");
    Serial.print("\t Temp: ");
    Serial.println(f_temp);
    Serial.print("\t Array Index: ");
    Serial.println(constrain(map(f_temp * TEMP_LEDS_SCALAR, TEMP_LEDS_COLD, TEMP_LEDS_HOT, 0, 255), 0, 255));
    #endif

    // Get new color temperature
    get_colortemp(&colorTemp, constrain(map(f_temp * TEMP_LEDS_SCALAR, TEMP_LEDS_COLD, TEMP_LEDS_HOT, 0, 255), 0, 255));

    #ifdef DEBUG
    Serial.print("\t R: ");
    Serial.print(colorTemp.red, HEX);
    Serial.print("\t G: ");
    Serial.print(colorTemp.green, HEX);
    Serial.print("\t B: ");
    Serial.println(colorTemp.blue, HEX);
    #endif

    redLed.set(colorTemp.red);
    greenLed.set(colorTemp.green);
    blueLed.set(colorTemp.blue);
  }
  if((millis() - prevTime_temp_leds) > TEMP_LEDS_TIME_ON){
    redLed.set(0);
    greenLed.set(0);
    blueLed.set(0);
  }
  #endif

  if ((menuPosition == TimeIndex) || (modeChangedByUser == false)) { modesChanger(); }
  doIndication(NOT_TESTING);

  modeButton.Update();
  upButton.Update();
  downButton.Update();

  // Check for menu "editMode" timeout
  if (editMode == false) { blinkMask = B00000000; } 
  else if ((millis() - enteringEditModeTime) > MENU_TIMEOUT)
  {
    editMode = false;
    //menuPosition = firstChild[menuPosition];
    menuPosition = 0; // Back to time display
    blinkMask = blinkPattern[menuPosition];

    #ifdef DEBUG
    Serial.println("[i] menu timeout reached");
    #endif
  }

  // Mode button: Short Press -> cycle through edit modes -------------------------------------------------------------
  if (modeButton.clicks > 0) 
  {
    modeChangedByUser = true;
    TubesOn = true;                 // Turn display on if in tubes are currrntly off

    #ifdef BEEP_ENABLE
    TimerFreeTone(BUZZER_PIN, 1000, BEEP_DURATION);
    #endif

    menuPosition++;

    #if defined (__AVR_ATmega328P__)
      if (menuPosition == TimeZoneIndex) menuPosition++;  // skip TimeZone for Arduino Uno
    #endif

    // Wrap around top level menu if we are not in editMode
    if ((menuPosition > LAST_PARENT) && !editMode)  { menuPosition = TimeIndex; }

    #ifdef DEBUG
    Serial.println("[i] Short Click pressed");
    Serial.print("\tMenu Position: ");
    Serial.println(menuPosition);
    Serial.print("\tValue: ");
    Serial.println(value[menuPosition]);
    #endif

    // should move!
    blinkMask = blinkPattern[menuPosition];
    
    // we are in editMode: if not in the parent menu AND the previous menuposition was the last child within the sub menu, exit from edit mode
    if ((parent[menuPosition - 1] != NO_PARENT) && ((menuPosition -1) == (lastChild[parent[menuPosition - 1]]))) {
      if ((parent[menuPosition] == 1) && (!isValidDate()))
      {
        menuPosition = DateDayIndex;
        return;
      }

      // Exit edit mode
      editMode = false;
      #ifdef DEBUG
      Serial.println("[i] Exiting edit mode");
      Serial.println(value[TubeTimerOffHourIndex]);
      #endif

      // Set menu back to parent menu
      menuPosition = parent[menuPosition - 1];

      // set menu back to time mode
      //menuPosition = TimeIndex;

      if (menuPosition == TimeIndex) { setTime(value[TimeHoursIndex], value[TimeMintuesIndex], value[TimeSecondsIndex], day(), month(), year()); }
      if (menuPosition == DateIndex) 
      {
        #ifdef DEBUG
        Serial.print("Day:");
        Serial.println(value[DateDayIndex]);
        Serial.print("Month:");
        Serial.println(value[DateMonthIndex]);
        #endif

        setTime(hour(), minute(), second(), value[DateDayIndex], value[DateMonthIndex], 2000 + value[DateYearIndex]);
        EEPROM.write(DATEFORMAT_EEPROM_ADDR, value[DateFormatIndex]);
      }
      if (menuPosition == AlarmIndex) {
        EEPROM.write(ALARMTIME_EEPROM_ADDR, value[AlarmHourIndex]);
        EEPROM.write(ALARMTIME_EEPROM_ADDR + 1, value[AlarmMinuteIndex]);
        EEPROM.write(ALARMTIME_EEPROM_ADDR + 2, value[AlarmSecondIndex]);
        EEPROM.write(ALARMARMED_EEPROM_ADDR, value[Alarm01]);
      };
      if (menuPosition == hModeIndex) { EEPROM.write(HOURFORMAT_EEPROM_ADDR, value[hModeValueIndex]); }
      if (menuPosition == TemperatureIndex) { EEPROM.write(DEGFORMAT_EEPROM_ADDR, value[DegreesFormatIndex]); }
      if (menuPosition == TubeTimerIndex)
      { 
        EEPROM.write(TT_OFF_HH_EEPROM_ADDR, value[TubeTimerOffHourIndex]); 
        EEPROM.write(TT_OFF_MM_EEPROM_ADDR, value[TubeTimerOffMinIndex]); 
        EEPROM.write(TT_ON_HH_EEPROM_ADDR, value[TubeTimerOnHourIndex]); 
        EEPROM.write(TT_ON_MM_EEPROM_ADDR, value[TubeTimerOnMinIndex]); 
        EEPROM.write(TT_ENABLE_EEPROM_ADDR, value[TubeTimerEnable]); 
        #ifdef DEBUG
        Serial.println("[i] Tube Timer EEPROM values stored in EEPROM");
        Serial.println(value[TubeTimerOffHourIndex]);
        #endif
      }

      setRTCDateTime(hour(), minute(), second(), day(), month(), year() % 1000, 1);

      return;
    } //exit from edit mode

    // set value from current blinkmask (Not sure why this is required yet)    
    if ((menuPosition != DateFormatIndex) && (menuPosition != DateDayIndex) && (menuPosition != TubeTimerEnable) && (menuPosition != TubeTimerOnHourIndex)){  
      value[menuPosition] = extractDigits(blinkMask);
    }

  } // end if short press

  // Mode button long press: Enter/Exit edit mode and save setting ---------------------------------------------------- 
  if (modeButton.clicks < 0)
  {

    #ifdef BEEP_ENABLE
    TimerFreeTone(BUZZER_PIN, 1000, BEEP_DURATION);
    #endif

    // Any button press will turn the tubes back on
    if (!TubesOn){ TubesOn = true; }

    // We where not in a edit mode previously -> enter "Timekeeping Mode" 
    if (!editMode)
    {
      enteringEditModeTime = millis();
      //temporary enabled 24 hour format while settings
      if (menuPosition == TimeIndex) stringToDisplay = PreZero(hour()) + PreZero(minute()) + PreZero(second()); 
    }

    editMode = !editMode; // Toggle editMode

    #ifdef DEBUG
    if (editMode){
      Serial.print("[i] Entered edit mode @ ");
      Serial.println(enteringEditModeTime); }
    else { Serial.println("i] Exiting mode"); } 
    #endif

    // We where in date mode previously -> "Set Date"
    if (menuPosition == DateIndex) 
    {
      #ifdef DEBUG
      Serial.println("DateEdit");
      #endif

      value[DateDayIndex] =  day();
      value[DateMonthIndex] = month();
      value[DateYearIndex] = year() % 1000;
      if (value[DateFormatIndex] == EU_DateFormat) stringToDisplay=PreZero(value[DateDayIndex])+PreZero(value[DateMonthIndex])+PreZero(value[DateYearIndex]);
        else stringToDisplay=PreZero(value[DateMonthIndex])+PreZero(value[DateDayIndex])+PreZero(value[DateYearIndex]);
      
      #ifdef DEBUG
      Serial.print("str=");
      Serial.println(stringToDisplay);
      #endif
    }
    menuPosition = firstChild[menuPosition];

    // We where in alarm mode previously -> set alarm enabled (Enables Alarm just by entering into TubeTimer menu)
    if (menuPosition == AlarmHourIndex) {
      value[Alarm01] = 1; 
      dotPattern = B10000000;
    }

    // We where in TubeTimer mode previously -> set TubeTimer ON (Enables TubeTimer just by entering into TubeTimer menu)
    if (menuPosition == TubeTimerOffHourIndex) {
      value[TubeTimerEnable] = 1; 
      dotPattern = B10000000;
    }

    blinkMask = blinkPattern[menuPosition];
    if ((menuPosition != DegreesFormatIndex) && (menuPosition != DateFormatIndex)) {
      value[menuPosition] = extractDigits(blinkMask);
    }

    #ifdef DEBUG
    Serial.print("[i] Menu Position: ");
    Serial.println(menuPosition);
    Serial.print("[i] Value: ");
    Serial.println(value[menuPosition]);
    #endif
  } // end if long press
  //------------------------------------------------------------------------------------------------------------------------
  // up button checks
  if (upButton.clicks != 0) functionUpButton = upButton.clicks;

  if (upButton.clicks > 0)
  {
    modeChangedByUser = true;

    #ifdef BEEP_ENABLE
    TimerFreeTone(BUZZER_PIN, 1000, BEEP_DURATION);
    #endif

    // Any button press will turn the tubes back on
    if (!TubesOn){ TubesOn = true; }

    incrementValue();
    if (!editMode)
    {
      LEDsLock = false;
      EEPROM.write(LEDLOCK_EEPROM_ADDR, 0);
    }
  }

  if (functionUpButton == -1 && upButton.depressed == true)
  {
    BlinkUp = false;
    if (editMode == true)
    {
      if ( (millis() - upTime) > settingDelay)
      {
        upTime = millis();// + settingDelay;
        incrementValue();
      }
    }
  } else BlinkUp = true;
  
  // Down button checks
  if (downButton.clicks != 0) functionDownButton = downButton.clicks;

  if (downButton.clicks > 0)
  {
    modeChangedByUser = true; 

    #ifdef BEEP_ENABLE
    TimerFreeTone(BUZZER_PIN, 1000, BEEP_DURATION);
    #endif

    // Any button press will turn the tubes back on
    if (!TubesOn){ TubesOn = true; }

    dicrementValue();

    #ifdef LEGACY_LEDS
    if (!editMode)
    {
      LEDsLock = true;
      EEPROM.write(LEDLOCK_EEPROM_ADDR, 1);
      EEPROM.write(LEDREDVAL_EEPROM_ADDR, RedLight);
      EEPROM.write(LEDGREENVAL_EEPROM_ADDR, GreenLight);
      EEPROM.write(LEDBLUEVAL_EEPROM_ADDR, BlueLight);

      #ifdef DEBUG
      Serial.println(F("Store to EEPROM:"));
      Serial.print(F("RED="));
      Serial.println(RedLight);
      Serial.print(F("GREEN="));
      Serial.println(GreenLight);
      Serial.print(F("Blue="));
      Serial.println(BlueLight);
      #endif
    }
    #endif
  }

  if (functionDownButton == -1 && downButton.depressed == true)
  {
    BlinkDown = false;
    if (editMode == true)
    {
      if ( (millis() - downTime) > settingDelay)
      {
        downTime = millis();// + settingDelay;
        dicrementValue();
      }
    }
  } else BlinkDown = true;

  if (!editMode)
  {
    if (upButton.clicks < 0)
    {
      #ifdef BEEP_ENABLE
      TimerFreeTone(BUZZER_PIN, 1000, BEEP_DURATION);
      #endif

      #ifdef LEGACY_LEDS
      RGBLedsOn = true;
      EEPROM.write(RGB_EEPROM_ADDR, 1);
      setLEDsFromEEPROM();

      #ifdef DEBUG
      Serial.println("[i] RGB: ON");
      #endif

      #endif
    }
    if (downButton.clicks < 0)
    {
      #ifdef BEEP_ENABLE
      TimerFreeTone(BUZZER_PIN, 1000, BEEP_DURATION);
      #endif
      
      RGBLedsOn = false;
      EEPROM.write(RGB_EEPROM_ADDR, 0);

      #ifdef DEBUG
      Serial.println("RGB: OFF");
      #endif
    }
  }

  // Display Switch case ----------------------------------------------------------------------------------------------
  switch (menuPosition)
  {
    case TimeIndex: //time mode (this is the main display mode showing time)
      if (!transactionInProgress) { stringToDisplay = updateDisplayString(); }
      #ifdef COLON_ENABLE
      doDotBlink();
      #else
      dotPattern = B00000000;
      #endif
      blankMask = B00000000;
      break;

    case DateIndex: //date mode
      if (!transactionInProgress) { stringToDisplay = updateDateString(); }
      dotPattern = B11000000; //turn on upper and lower dots
      blankMask = B00000000;
      break;

    case AlarmIndex: //alarm mode
      stringToDisplay = PreZero(value[AlarmHourIndex]) + PreZero(value[AlarmMinuteIndex]) + PreZero(value[AlarmSecondIndex]);
      blankMask = B00000000;
      if (value[Alarm01] == 1) { dotPattern = B10000000; } //turn on upper dots
      else { dotPattern = B00000000; }//turn off upper dots
      break;

    case hModeIndex: //12/24 hours mode
      stringToDisplay = "00" + String(value[hModeValueIndex]) + "00";
      blankMask = B00110011;
      dotPattern = B00000000; //turn off all dots
      break;

    case TemperatureIndex:
      stringToDisplay = "00" + String(value[TemperatureIndex]) + "00";
      blankMask = B00110011;
      dotPattern = B01000000; //turn on lower dots
      break;

    // 6
    case TubeTimerIndex:
      stringToDisplay = PreZero(value[TubeTimerOffHourIndex]) + PreZero(value[TubeTimerOffMinIndex]) + "00";
      blankMask = B00000000;
      dotPattern = B01000000; //turn on lower dots
      break;
    
    // Update tube timer display for ON time
    // 23
    case TubeTimerOnHourIndex:
      stringToDisplay = PreZero(value[TubeTimerOnHourIndex]) + PreZero(value[TubeTimerOnMinIndex]) + "00";
      blankMask = B00000000;
      if (value[TubeTimerEnable] == 1) { dotPattern = B10000000; }  //turn on upper dots
      else { dotPattern = B00000000; }                              //turn off upper dots
      break;
    
    // 25
    case TubeTimerEnable:
      stringToDisplay = "00" + PreZero(value[TubeTimerEnable]) + "00";
      break;

    case DegreesFormatIndex:
      if (!transactionInProgress)
      {
        stringToDisplay = updateTemperatureString(getTemperature(value[DegreesFormatIndex]));
        if (value[DegreesFormatIndex] == CELSIUS)
        {
          blankMask = B00110001;
          dotPattern = B01000000;
        }
        else
        {
          blankMask = B00100011;
          dotPattern = B00000000;
        }
      }

      if (getTemperature(value[DegreesFormatIndex]) < 0) dotPattern |= B10000000;
      else dotPattern &= B01111111;
      break;

      case DateFormatIndex:
        if (value[DateFormatIndex] == EU_DateFormat) 
        {
          stringToDisplay="311299";
          blinkPattern[DateDayIndex]=B00000011;
          blinkPattern[DateMonthIndex]=B00001100;
        }
        else 
        {
          stringToDisplay="123199";
          blinkPattern[DateDayIndex]=B00001100;
          blinkPattern[DateMonthIndex]=B00000011;
        }
        break; 

      case DateDayIndex:
        // skip
        break;

      case DateMonthIndex:
        // Skip
        break;

      case DateYearIndex:
        if (value[DateFormatIndex] == EU_DateFormat) stringToDisplay=PreZero(value[DateDayIndex])+PreZero(value[DateMonthIndex])+PreZero(value[DateYearIndex]);
        else stringToDisplay=PreZero(value[DateMonthIndex])+PreZero(value[DateDayIndex])+PreZero(value[DateYearIndex]);
        break;
  } // end switch

  checkAlarmTime();
  checkTubeTimers();

}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// End of main loop()

void init_EEPROM()
{
  // Initialize settings from EEPROM
  if (EEPROM.read(RGB_EEPROM_ADDR) != 0) { RGBLedsOn = true; } else { RGBLedsOn = false; }
  if (EEPROM.read(HOURFORMAT_EEPROM_ADDR) != 12) { value[hModeValueIndex] = 24; } else { value[hModeValueIndex] = 12; }
  if (EEPROM.read(ALARMTIME_EEPROM_ADDR) > 24) { value[AlarmHourIndex] = 0; } else { value[AlarmHourIndex] = EEPROM.read(ALARMTIME_EEPROM_ADDR); }
  if (EEPROM.read(ALARMTIME_EEPROM_ADDR + 1) > 60) { value[AlarmMinuteIndex] = 0; } else { value[AlarmMinuteIndex] = EEPROM.read(ALARMTIME_EEPROM_ADDR + 1); }
  if (EEPROM.read(ALARMTIME_EEPROM_ADDR + 2) > 60) { value[AlarmSecondIndex] = 0; } else { value[AlarmSecondIndex] = EEPROM.read(ALARMTIME_EEPROM_ADDR + 2); }
  if (EEPROM.read(ALARMARMED_EEPROM_ADDR) > 1) { value[Alarm01] = 0; } else { value[Alarm01] = EEPROM.read(ALARMARMED_EEPROM_ADDR); }
  if (EEPROM.read(LEDLOCK_EEPROM_ADDR) > 1) { LEDsLock = false; } else { LEDsLock = EEPROM.read(LEDLOCK_EEPROM_ADDR); }
  if (EEPROM.read(DEGFORMAT_EEPROM_ADDR) > 1) { value[DegreesFormatIndex] = CELSIUS; } else { value[DegreesFormatIndex] = EEPROM.read(DEGFORMAT_EEPROM_ADDR); }
  if (EEPROM.read(DATEFORMAT_EEPROM_ADDR) > 1) { value[DateFormatIndex] = value[DateFormatIndex]; } else { value[DateFormatIndex] = EEPROM.read(DATEFORMAT_EEPROM_ADDR); }

  // Tube timer EEPROM values
  if (EEPROM.read(TT_OFF_HH_EEPROM_ADDR) > 24) { value[TubeTimerOffHourIndex] = 0; } else { value[TubeTimerOffHourIndex] = EEPROM.read(TT_OFF_HH_EEPROM_ADDR); }
  if (EEPROM.read(TT_OFF_MM_EEPROM_ADDR) > 60) { value[TubeTimerOffMinIndex] = 0; } else { value[TubeTimerOffMinIndex] = EEPROM.read(TT_OFF_MM_EEPROM_ADDR); }
  if (EEPROM.read(TT_ON_HH_EEPROM_ADDR) > 24) { value[TubeTimerOnHourIndex] = 0; } else { value[TubeTimerOnHourIndex] = EEPROM.read(TT_ON_HH_EEPROM_ADDR); }
  if (EEPROM.read(TT_ON_MM_EEPROM_ADDR) > 60) { value[TubeTimerOnMinIndex] = 0; } else { value[TubeTimerOnMinIndex] = EEPROM.read(TT_ON_MM_EEPROM_ADDR); }
  if (EEPROM.read(TT_ENABLE_EEPROM_ADDR) > 1) { value[TubeTimerEnable] = 1; } else { value[TubeTimerEnable] = EEPROM.read(TT_ENABLE_EEPROM_ADDR); }

  // Print EEPROM values:
  #ifdef DEBUG
  Serial.println(" ------------ EEPROM Settings ------------");
    #ifdef LEGACY_LEDS
    Serial.print("RGB LEDs: ");
    Serial.println(RGBLedsOn ? "ON" : "OFF");
    Serial.print("LED Color Lock: ");
    Serial.println(LEDsLock ? "ON" : "OFF");
    #endif
  Serial.print("Hour Format: ");
  Serial.println((value[hModeValueIndex] == 12) ? "12" : "24");

  Serial.print("Alarm: ");
  Serial.println(value[Alarm01] ? "ON" : "OFF");
  if(value[Alarm01]){
    Serial.print("\tAlarm Time: ");
    Serial.print(value[AlarmHourIndex]);
    Serial.print(":");
    Serial.print(value[AlarmMinuteIndex]);
    Serial.print(":");
    Serial.println(value[AlarmSecondIndex]);
  }

  Serial.print("Temperature Format: ");
  Serial.println(!value[DegreesFormatIndex] ? "Celsius" : "Fahrenheit");

    
  if ( value[TubeTimerEnable] ){
    Serial.println("Tube Timer: Enabled");
    Serial.print("\tTube Timer Off Time: ");
    Serial.print(value[TubeTimerOffHourIndex]);
    Serial.print(":");
    Serial.println(value[TubeTimerOffMinIndex]);
    Serial.print("\tTube Timer On Time: ");
    Serial.print(value[TubeTimerOnHourIndex]);
    Serial.print(":");
    Serial.println(value[TubeTimerOnMinIndex]);
  }
  else { Serial.println("Tube Timer: Disabled"); }
  #endif
  Serial.println(" -----------------------------------------");
}

// return string representation of int
String PreZero(int digit)
{
  digit = abs(digit);
  if (digit < 10) return String("0") + String(digit);
  else return String(digit);
}

#ifdef LEGACY_LEDS
void rotateFireWorks()
{
  if (!RGBLedsOn)
  {
    analogWrite(RedLedPin, 0 );
    analogWrite(GreenLedPin, 0);
    analogWrite(BlueLedPin, 0);
    return;
  }
  if (LEDsLock) return;
  RedLight = RedLight + fireforks[rotator * 3];
  GreenLight = GreenLight + fireforks[rotator * 3 + 1];
  BlueLight = BlueLight + fireforks[rotator * 3 + 2];
  analogWrite(RedLedPin, RedLight );
  analogWrite(GreenLedPin, GreenLight);
  analogWrite(BlueLedPin, BlueLight);

  #ifdef DEBUG
  // Serial.print("R: ");
  // Serial.print(RedLight);
  // Serial.print(" G: ");
  // Serial.print(GreenLight);
  // Serial.print(" B: ");
  // Serial.println(BlueLight);
  #endif
  
  cycle = cycle + 1;
  if (cycle == 255)
  {
    rotator = rotator + 1;
    cycle = 0;
  }
  if (rotator > 5) rotator = 0;
}
#endif

String updateDisplayString()
{
  static  unsigned long lastTimeStringWasUpdated;
  if ((millis() - lastTimeStringWasUpdated) > 1000)
  {
    lastTimeStringWasUpdated = millis();
    return getTimeNow();
  }
  return stringToDisplay;
}

String getTimeNow()
{
  if (value[hModeValueIndex] == 24) return PreZero(hour()) + PreZero(minute()) + PreZero(second());
  else return PreZero(hourFormat12()) + PreZero(minute()) + PreZero(second());
}

void doTest()
{
  if (!ds.search(addr)) {
    ds.reset_search();
    delay(250);
    #ifdef DEBUG
    Serial.println("[!] Temp sensor not found");
    #endif
  } 
  else { TempPresent = true; }
  
  #ifdef TEMP_LEDS
  for(int i = 0; i < 255; i++){
    get_colortemp(&colorTemp, i);
    analogWrite(RED_LED_PIN, colorTemp.red);
    analogWrite(GREEN_LED_PIN, colorTemp.green);
    analogWrite(BLUE_LED_PIN, colorTemp.blue);
    delay(5);
  }
  #else
  analogWrite(RedLedPin,255);
  delay(1000);
  analogWrite(RedLedPin,0);
  analogWrite(GreenLedPin,255);
  delay(1000);
  analogWrite(GreenLedPin,0);
  analogWrite(BlueLedPin,255);
  delay(1000); 
  analogWrite(BlueLedPin,0);
  #endif

  #ifdef TUBES_8
  String testStringArray[11]={"00000000","11111111","22222222","33333333","44444444","55555555","66666666","77777777","88888888","99999999",""};
  #endif
  #ifdef TUBES_6
  String testStringArray[11]={"000000","111111","222222","333333","444444","555555","666666","777777","888888","999999",""};
  #endif
  
  unsigned int dlay = 200;
  bool test = 1;
  byte strIndex = -1;
  unsigned long startOfTest = millis() + 500; // disable delaying in first iteration
  bool digitsLock = false;

  while (test)
  {
    if (digitalRead(DOWN_PIN) == 0) { digitsLock = true; } 
    if (digitalRead(UP_PIN) == 0) { digitsLock = false; }

   if ((millis() - startOfTest ) > dlay) 
   {
     startOfTest = millis();
     if (!digitsLock) { strIndex = strIndex + 1; }
     if (strIndex==10) { dlay = 1000; }
     if (strIndex>10) { test = false; strIndex = 10; }
     
     stringToDisplay = testStringArray[strIndex];

     #ifdef DEBUG
     Serial.println(stringToDisplay);
     #endif

     doIndication(TESTING);
   }
   delayMicroseconds(2000);
  }; 

  #ifdef SONG_ON_PWRUP
  Rtttl.play(song);
  #endif

  analogWrite(RED_LED_PIN, 0);
  analogWrite(GREEN_LED_PIN, 0);
  analogWrite(BLUE_LED_PIN, 0);

  #ifdef DEBUG
  Serial.println("[i] Stopping Test");
  #endif
}

void doDotBlink()
{
  static unsigned long lastTimeBlink = millis();
  static bool dotState = 0;
  if ((millis() - lastTimeBlink) > COLON_BLINK_INTERVAL)
  {
    lastTimeBlink = millis();
    dotState = !dotState;
    if (dotState) 
    { 
      #ifdef COLON_LOWER_ONLY
      dotPattern = B01000000; 
      #else
      dotPattern = B11000000; 
      #endif
    }
    else { dotPattern = B00000000; }
  }
}

void setRTCDateTime(byte h, byte m, byte s, byte d, byte mon, byte y, byte w)
{
  byte zero = 0x00; //workaround for issue #527
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero); //stop Oscillator

  Wire.write(decToBcd(s));
  Wire.write(decToBcd(m));
  Wire.write(decToBcd(h));
  Wire.write(decToBcd(w));
  Wire.write(decToBcd(d));
  Wire.write(decToBcd(mon));
  Wire.write(decToBcd(y));

  Wire.write(zero); //start

  Wire.endTransmission();

}

byte decToBcd(byte val) {
  // Convert normal decimal numbers to binary coded decimal
  return ( (val / 10 * 16) + (val % 10) );
}

byte bcdToDec(byte val)  {
  // Convert binary coded decimal to normal decimal numbers
  return ( (val / 16 * 10) + (val % 16) );
}

void getRTCTime()
{
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0x00); //workaround for issue #527
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDRESS, 7);

  RTC_seconds = bcdToDec(Wire.read());
  RTC_minutes = bcdToDec(Wire.read());
  RTC_hours = bcdToDec(Wire.read() & 0b111111); //24 hour time
  RTC_day_of_week = bcdToDec(Wire.read()); //0-6 -> sunday - Saturday
  RTC_day = bcdToDec(Wire.read());
  RTC_month = bcdToDec(Wire.read());
  RTC_year = bcdToDec(Wire.read());
}

// return int from blinkmask
int extractDigits(byte b)
{
  String tmp = "1";
  if (b == B00000011) { tmp = stringToDisplay.substring(0, 2); }
  if (b == B00001100) { tmp = stringToDisplay.substring(2, 4); }
  if (b == B00110000) { tmp = stringToDisplay.substring(4); }  
  #ifdef DEBUG
  Serial.print("[!] Extract digits: ");
  Serial.println(tmp);
  #endif

  return tmp.toInt();
}

void injectDigits(byte b, int value)
{
  if (b == B00000011) stringToDisplay = PreZero(value) + stringToDisplay.substring(2);
  if (b == B00001100) stringToDisplay = stringToDisplay.substring(0, 2) + PreZero(value) + stringToDisplay.substring(4);
  if (b == B00110000) stringToDisplay = stringToDisplay.substring(0, 4) + PreZero(value);
}

bool isValidDate()
{
  int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (value[DateYearIndex] % 4 == 0) days[1] = 29;
  if (value[DateDayIndex] > days[value[DateMonthIndex] - 1]) return false;
  else return true;

}

void incrementValue()
{
  // refresh menu timeout
  enteringEditModeTime = millis();
  if (editMode == true)
  {
    switch(menuPosition)
    {
      case hModeValueIndex:
        // if 12/24 hour mode menu position
        value[menuPosition] = value[menuPosition] + 12;
        break;

      case Alarm01:
        // If we are in alarm enable/disable mode
        if (value[menuPosition] == 1) { dotPattern = B10000000; }           //turn on upper dots
        else { dotPattern = B00000000; }                                    //turn off all dots
        break;

      default:
        // Wrap value MAX -> MIN
        if (value[menuPosition] == maxValue[menuPosition]) { value[menuPosition] = minValue[menuPosition]; }
        else { value[menuPosition] = value[menuPosition] + 1; }

      if (menuPosition != DateFormatIndex) { injectDigits(blinkMask, value[menuPosition]); }
      
      #ifdef DEBUG
      Serial.print("[i] Value: ");
      Serial.println(value[menuPosition]);
      #endif
    } // end switch
  } // end if
}

void dicrementValue()
{
  // refresh menu timeout
  enteringEditModeTime = millis();
  if (editMode == true)
  {
    switch (menuPosition)
    {
      case hModeValueIndex:
        // if 12/24 hour mode menu position
        if (menuPosition == hModeValueIndex) { value[menuPosition] = value[menuPosition] - 12; }
        break;

      case Alarm01:
          // If we are in alarm enable/disable mode
          if (value[menuPosition] == 1) { dotPattern = B10000000; }           //turn on upper dots
          else { dotPattern = B00000000; }                                    //turn off all dots
        break;

      default: 
        // Wrap value MIN -> MAX
        if (value[menuPosition] == minValue[menuPosition]) { value[menuPosition] = maxValue[menuPosition]; }
        else { value[menuPosition] = value[menuPosition] - 1; }

      if (menuPosition != DateFormatIndex) { injectDigits(blinkMask, value[menuPosition]); }

      #ifdef DEBUG
      Serial.print("[i] Value: ");
      Serial.println(value[menuPosition]);
      #endif

    } // end switch
  } // end if
}

void checkAlarmTime()
{
static bool Alarm1SecondBlock = false;
static unsigned long lastTimeAlarmTriggired = 0;

  if (value[Alarm01] == 0) return;
  if ((Alarm1SecondBlock == true) && ((millis() - lastTimeAlarmTriggired) > 1000)) Alarm1SecondBlock = false;
  if (Alarm1SecondBlock == true) return;
  if ((hour() == value[AlarmHourIndex]) && (minute() == value[AlarmMinuteIndex]) && (second() == value[AlarmSecondIndex]))
  {
    lastTimeAlarmTriggired = millis();
    Alarm1SecondBlock = true;

    #ifdef DEBUG
    Serial.println("[i] Alarm triggered");
    #endif

    Rtttl.play(song);
  }
}

// Tube timer functions:
void checkTubeTimers()
{
  static bool TubeTimer_MinuteBlock = false;          // Block seconds to stop trigggering
  static unsigned long lastTime_TT_Triggired = 0;

  if (value[TubeTimerEnable] == 1)
  {
    if (TubeTimer_MinuteBlock == true)
    { 
      // Reset Second block after a second has elapsed, else exit now
      if ((millis() - lastTime_TT_Triggired) > 60100) { TubeTimer_MinuteBlock = false; }
      else { return; }
    }

    // Check OFF timer (do not turn off tubes while in editMode)
    if ((hour() == value[TubeTimerOffHourIndex]) && (minute() == value[TubeTimerOffMinIndex] && !editMode))
    {
      lastTime_TT_Triggired = millis();
      TubeTimer_MinuteBlock = true;

      TubesOn = false;
    
      #ifdef DEBUG
      Serial.println("[!] TubeTimer OFF Triggered");
      #endif
    }

    // Check ON timer
    else if ((hour() == value[TubeTimerOnHourIndex]) && (minute() == value[TubeTimerOnMinIndex]))
    {
      lastTime_TT_Triggired = millis();
      TubeTimer_MinuteBlock = true;

      TubesOn = true;
    
      #ifdef DEBUG
      Serial.println("[!] TubeTimer ON Triggered");
      #endif
    }
  }
}

void setLEDsFromEEPROM()
{
  #ifdef LEGACY_LEDS
  analogWrite(RED_LED_PIN, EEPROM.read(LEDREDVAL_EEPROM_ADDR));
  analogWrite(GREEN_LED_PIN, EEPROM.read(LEDGREENVAL_EEPROM_ADDR));
  analogWrite(BLUE_LED_PIN, EEPROM.read(LEDBLUEVAL_EEPROM_ADDR));
  #endif

  #ifdef DEBUG
  Serial.println("[i] LED values loaded from EEPROM");
  Serial.print("\tRed: ");
  Serial.println(EEPROM.read(LEDREDVAL_EEPROM_ADDR));
  Serial.print("\tGreen: ");
  Serial.println(EEPROM.read(LEDGREENVAL_EEPROM_ADDR));
  Serial.print("\tBlue: ");
  Serial.println(EEPROM.read(LEDBLUEVAL_EEPROM_ADDR));
  #endif
}

void modesChanger()
{
  if (editMode == true) { return; }
  static unsigned long lastTimeModeChanged = millis();
  static unsigned long lastTimeAntiPoisoningIterate = millis();
  static int transnumber = 0;

  if ((millis() - lastTimeModeChanged) > modesChangePeriod)
  {
    lastTimeModeChanged = millis();
    
    if (transnumber > 2) { transnumber = 0; }

    // Change menuPosition (display mode) and interval for the next time this parent if is executed 
    if (transnumber == 0) {
      menuPosition = DateIndex;
      #ifdef DEBUG
      Serial.println("[d] menuPosition: DateIndex"); 
      #endif
      modesChangePeriod = DATEMODEPERIOD;
    }
    if (transnumber == 1) {
      menuPosition = DegreesFormatIndex; //TemperatureIndex;
      #ifdef DEBUG
      Serial.println("[d] menuPosition: TempIndex"); 
      #endif
      modesChangePeriod = DATEMODEPERIOD;
      if (!TempPresent) { transnumber = 2; }
    }
    if (transnumber == 2) {
      menuPosition = TimeIndex;
      #ifdef DEBUG
      Serial.println("[d] menuPosition: TimeIndex"); 
      #endif
      modesChangePeriod = TIMEMODEPERIOD;
    }

    if (modeChangedByUser == true) { 
      menuPosition = TimeIndex; 
      #ifdef DEBUG
      Serial.println("[d] menuPosition: TimeIndex"); 
      #endif
      }

    modeChangedByUser = false;
    transnumber++;
  }

  // if less than 2 sec since mode changed
  if ((millis() - lastTimeModeChanged) < 2000)
  {
    if ((millis() - lastTimeAntiPoisoningIterate) > ANITPOISON_RATE)
    {
      lastTimeAntiPoisoningIterate = millis();
      if (TempPresent) {
        if (menuPosition == TimeIndex) stringToDisplay = antiPoisoning2(updateTemperatureString(getTemperature(value[DegreesFormatIndex])), getTimeNow());
        if (menuPosition == DateIndex) stringToDisplay = antiPoisoning2(getTimeNow(), PreZero(day()) + PreZero(month()) + PreZero(year() % 1000) );
        if (menuPosition == TemperatureIndex) stringToDisplay = antiPoisoning2(PreZero(day()) + PreZero(month()) + PreZero(year() % 1000), updateTemperatureString(getTemperature(value[DegreesFormatIndex])));
      } 
      else {
        if (menuPosition == TimeIndex) stringToDisplay = antiPoisoning2(PreZero(day()) + PreZero(month()) + PreZero(year() % 1000), getTimeNow());
        if (menuPosition == DateIndex) stringToDisplay = antiPoisoning2(getTimeNow(), PreZero(day()) + PreZero(month()) + PreZero(year() % 1000) );
      }
      #ifdef DEBUG
      Serial.println("[d] modesChanger string: " + stringToDisplay);
      #endif
    }
  } // end if
  else { transactionInProgress = false; }
}

String antiPoisoning2(String fromStr, String toStr)
{
  static byte toDigits[6];
  static byte currentDigits[6];
  static byte iterationCounter = 0;

  if (!transactionInProgress)
  {
    transactionInProgress = true;
    blankMask = B00000000;
    for (int i = 0; i < 6; i++)
    {
      currentDigits[i] = fromStr.substring(i, i + 1).toInt();
      toDigits[i] = toStr.substring(i, i + 1).toInt();
    }
  }
  for (int i = 0; i < 6; i++)
  {
    if (iterationCounter < 10) currentDigits[i]++;
    else if (currentDigits[i] != toDigits[i]) currentDigits[i]++;
    if (currentDigits[i] == 10) currentDigits[i] = 0;
  }
  iterationCounter++;
  if (iterationCounter == 20)
  {
    iterationCounter = 0;
    transactionInProgress = false;
  }
  String tmpStr;
  for (int i = 0; i < 6; i++){ tmpStr += currentDigits[i]; }

  return tmpStr;
}

String updateDateString()
{
  static unsigned long lastTimeDateUpdate = millis()+1001;
  static String DateString = PreZero(day()) + PreZero(month()) + PreZero(year() % 1000);
  static byte prevoiusDateFormatWas=value[DateFormatIndex];
  if (((millis() - lastTimeDateUpdate) > 1000) || (prevoiusDateFormatWas != value[DateFormatIndex]))
  {
    lastTimeDateUpdate = millis();
    if (value[DateFormatIndex]==EU_DateFormat) DateString = PreZero(day()) + PreZero(month()) + PreZero(year() % 1000);
      else DateString = PreZero(month()) + PreZero(day()) + PreZero(year() % 1000);
  }
  return DateString;
}

float getTemperature (boolean bTempFormat)
{
  byte TempRawData[2];
  ds.reset();
  ds.write(0xCC); //skip ROM command
  ds.write(0x44); //send make convert to all devices
  ds.reset();
  ds.write(0xCC); //skip ROM command
  ds.write(0xBE); //send request to all devices

  TempRawData[0] = ds.read();
  TempRawData[1] = ds.read();
  int16_t raw = (TempRawData[1] << 8) | TempRawData[0];
  if (raw == -1) { raw = 0; }

  float celsius = (float)raw / 16.0;
  float fDegrees;

  if (!bTempFormat) { fDegrees = celsius * 10; }
  else { fDegrees = (celsius * 1.8 + 32.0) * 10; }

  return fDegrees;
}

#if (defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)) && defined(GPS_SYNC_ENABLE)
void SyncWithGPS()
{
  static unsigned long Last_Time_GPS_Sync=0;
  static bool GPS_Sync_Flag=0;
  //byte HoursOffset=2;
  if (GPS_Sync_Flag == 0) 
  {
    if ((millis()-GPS_Date_Time.GPS_Data_Parsed_time)>3000) 
    {
      #ifdef DEBUG
      Serial.println("Parsed data to old"); 
      #endif

      return;
    }

    #ifdef DEBUG
    Serial.println("Updating time...");
    Serial.println(GPS_Date_Time.GPS_hours);
    Serial.println(GPS_Date_Time.GPS_minutes);
    Serial.println(GPS_Date_Time.GPS_seconds);
    #endif
    
    setTime(GPS_Date_Time.GPS_hours, GPS_Date_Time.GPS_minutes, GPS_Date_Time.GPS_seconds, GPS_Date_Time.GPS_day, GPS_Date_Time.GPS_mounth, GPS_Date_Time.GPS_year % 1000);
    adjustTime(value[HoursOffsetIndex] * 3600);
    setRTCDateTime(hour(), minute(), second(), day(), month(), year() % 1000, 1);
    GPS_Sync_Flag = 1;
    Last_Time_GPS_Sync=millis();
  }
    else
    {
      if (((millis())-Last_Time_GPS_Sync) > 1800000) GPS_Sync_Flag=0;
        else GPS_Sync_Flag=1;
    }
}

bool GPS_Parse_DateTime()
{
  bool GPSsignal=false;
  if (!((GPS_Package[0]   == '$')
       &&(GPS_Package[3] == 'R')
       &&(GPS_Package[4] == 'M')
       &&(GPS_Package[5] == 'C'))) {return false;}
       else {
          #ifdef DEBUG
          Serial.println("[*] RMC!!!");
          #endif
       }
  
  int hh=(GPS_Package[7]-48)*10+GPS_Package[8]-48;
  int mm=(GPS_Package[9]-48)*10+GPS_Package[10]-48;
  int ss=(GPS_Package[11]-48)*10+GPS_Package[12]-48;

  #ifdef DEBUG
  /* 
  Serial.print("hh: ");
  Serial.println(hh);
  Serial.print("mm: ");
  Serial.println(mm);
  Serial.print("ss: ");
  Serial.println(ss); 
  */
  #endif

  byte GPSDatePos=0;
  int CommasCounter=0;
  for (int i = 12; i < GPS_BUFFER_LENGTH ; i++)  
  {
    if (GPS_Package[i] == ',')
    {
      CommasCounter++; 
      if (CommasCounter==8) 
        {
          GPSDatePos=i+1;
          break;
        }
    }
  }
  
  int dd=(GPS_Package[GPSDatePos]-48)*10+GPS_Package[GPSDatePos+1]-48;
  int MM=(GPS_Package[GPSDatePos+2]-48)*10+GPS_Package[GPSDatePos+3]-48;
  int yyyy=2000+(GPS_Package[GPSDatePos+4]-48)*10+GPS_Package[GPSDatePos+5]-48;

  #ifdef DEBUG
  /* 
  Serial.print("dd: "); 
  Serial.println(dd);
  Serial.print("MM: ");
  Serial.println(MM);
  Serial.print("yyyy: ");
  Serial.println(yyyy);
  */
  #endif

  //if ((hh<0) || (mm<0) || (ss<0) || (dd<0) || (MM<0) || (yyyy<0)) return false;
  if ( !inRange( yyyy, 2018, 2038 ) ||
    !inRange( MM, 1, 12 ) ||
    !inRange( dd, 1, 31 ) ||
    !inRange( hh, 0, 23 ) ||
    !inRange( mm, 0, 59 ) ||
    !inRange( ss, 0, 59 ) ) return false;
    else 
    {
      GPS_Date_Time.GPS_hours=hh;
      GPS_Date_Time.GPS_minutes=mm;
      GPS_Date_Time.GPS_seconds=ss;
      GPS_Date_Time.GPS_day=dd;
      GPS_Date_Time.GPS_mounth=MM;
      GPS_Date_Time.GPS_year=yyyy;
      GPS_Date_Time.GPS_Data_Parsed_time=millis();

      #ifdef DEBUG
      //Serial.println("Precision TIME HAS BEEN ACCURED!!!!!!!!!");
      #endif

      //GPS_Package[0]=0x0A;
      return 1;
    }
}

uint8_t ControlCheckSum()
{
  uint8_t  CheckSum = 0, MessageCheckSum = 0;   // check sum
  uint16_t i = 1;                // 1 sybol left from '$'

  while (GPS_Package[i]!='*')
  {
    CheckSum^=GPS_Package[i];
    if (++i == GPS_BUFFER_LENGTH) 
    {
      #ifdef DEBUG
      Serial.println("End of the line");
      #endif

      return 0;
    } // end of line not found
  }

  if (GPS_Package[++i]>0x40) MessageCheckSum=(GPS_Package[i]-0x37)<<4;  // ASCII codes to DEC convertation 
  else                  MessageCheckSum=(GPS_Package[i]-0x30)<<4;  
  if (GPS_Package[++i]>0x40) MessageCheckSum+=(GPS_Package[i]-0x37);
  else                  MessageCheckSum+=(GPS_Package[i]-0x30);
  
  if (MessageCheckSum != CheckSum) 
  {
    #ifdef DEBUG
    Serial.println("wrong checksum");
    #endif
    return 0;
  } // wrong checksum

  #ifdef DEBUG
  Serial.println("Checksum is ok");
  #endif

  return 1; // all ok!
}
#endif

#ifdef NTP_SYNC_ENABLE
void GetNTPData()
{
  if (Serial1.available() > 0) {
    byte ntp_in_byte = Serial1.read();
    switch (ntp_in_byte)
    {
      case 0x0A:  // Newline charactar
        ntp_buffer[ntp_position] = 0x00;
        ntp_position = 0;
        ntp_sync_pending = true;
        break;
      
      case 'E':
        #ifdef DEBUG
        Serial.println("[!] Error While parsing NTP string");
        #endif
        ntp_position = 0;
        ntp_sync_pending = false;
        break;
      
      case 'U':
        // Skip
        break;

      case ' ':
        // Skip
        break;

      default:
        ntp_buffer[ntp_position] = ntp_in_byte;
        ntp_position++;
        break;
    } // end switch
  } // end if
}

void ntp_rtcupdate()
{
  // Convert UNIX timestamp to lastTime_TT_Triggired
  unsigned long utc_time = atol(ntp_buffer);
  unsigned long local_time = myTZ.toLocal(utc_time, &tcr);

  // Update the system time
  setTime(hour(local_time), minute(local_time), second(local_time), day(local_time), month(local_time), year(local_time) % 1000);

  #ifdef DEBUG
  Serial.print("[i] UTC Time: ");
  Serial.print(utc_time);
  Serial.print(" Local Time: ");
  Serial.print(local_time);
  Serial.print(" > ");
  Serial.print(hour(local_time));
  Serial.print(":");
  Serial.print(minute(local_time));
  Serial.print(":");
  Serial.print(second(local_time));
  Serial.print("  Date: ");
  Serial.print(month(local_time));
  Serial.print("/");
  Serial.print(day(local_time));
  Serial.print("/");
  Serial.println(year(local_time));
  #endif

  // Update the RTC
  setRTCDateTime(hour(), minute(), second(), day(), month(), year() % 1000, 1);
}

int dstOffset (unsigned long unixTime)
{
  // from: https://forum.arduino.cc/index.php?topic=197637.0
  //Receives unix epoch time and returns seconds of offset for local DST
  //Valid thru 2099 for US only, Calculations from "http://www.webexhibits.org/daylightsaving/i.html"

  time_t t = unixTime;
  int beginDSTDay = (14 - (1 + year(t) * 5 / 4) % 7);  
  int beginDSTMonth=3;
  int endDSTDay = (7 - (1 + year(t) * 5 / 4) % 7);
  int endDSTMonth=11;
  if (((month(t) > beginDSTMonth) && (month(t) < endDSTMonth))
    || ((month(t) == beginDSTMonth) && (day(t) > beginDSTDay))
    || ((month(t) == beginDSTMonth) && (day(t) == beginDSTDay) && (hour(t) >= 2))
    || ((month(t) == endDSTMonth) && (day(t) < endDSTDay))
    || ((month(t) == endDSTMonth) && (day(t) == endDSTDay) && (hour(t) < 1)))
    return (3600);  //Add back in one hours worth of seconds - DST in effect
  else
    return (0);  //NonDST
}
#endif

String updateTemperatureString(float fDegrees)
{
  static  unsigned long lastTimeTemperatureString=millis()+1100;
  static String strTemp = "000000";
  //static String strTemp = "0000";

  if ((millis() - lastTimeTemperatureString) > TEMP_UPDATE_INTERVAL)
  {
    lastTimeTemperatureString = millis();
    int iDegrees = round(fDegrees);
    if (value[DegreesFormatIndex] == CELSIUS)
    {
      //strTemp = "0" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 1000) strTemp = "" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 100) strTemp = "0" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 10) strTemp = "00" + String(abs(iDegrees)) + "0";
    }
    else
    {
      //strTemp = "0" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 1000) strTemp = "00" + String(abs(iDegrees)/10) + "00";
      if (abs(iDegrees) < 100) strTemp = "000" + String(abs(iDegrees)/10) + "00";
      if (abs(iDegrees) < 10) strTemp = "0000" + String(abs(iDegrees)/10) + "00";
    }

    #ifdef TUBES_8
      strTemp= ""+strTemp+"00";
    #endif

    #ifdef DEBUG
    Serial.print("[i] Updating temperature: ");
    Serial.println(strTemp);
    #endif
  } // end if
  return strTemp;
}


boolean inRange( int no, int low, int high) {
  if ( no < low || no > high ) 
  {
    #ifdef DEBUG
    Serial.println("[!] Not in range: ");
    Serial.println(String(no) + ":" + String (low) + "-" + String(high));
    #endif

    return false;
  }
  return true;
}

#ifdef TEMP_LEDS
void get_colortemp(struct RGB_struct *rgb, int idx)
{
  long color = templed_array[idx];

  rgb->red = (byte)((color & 0xFF0000) >> 16);
  rgb->green = (byte)((color & 0xFF00) >> 8);
  rgb->blue = (byte)(color & 0xFF);
}
#endif
