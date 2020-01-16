#include <Rtttl.h>              // https://github.com/spicajames/Rtttl
                                // https://bitbucket.org/teckel12/arduino-timer-free-tone/downloads/
#include <Timezone.h>           // https://github.com/JChristensen/Timezone
#include <Wire.h>
#include <ClickButton.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include <OneWire.h>

#include "NixieClock_NCS314_utils.h"
#include "NixieConfig.h"

#ifdef TEMP_LEDS
#include <FadeLed.h>            // https://github.com/septillion-git/FadeLed
#endif

#define isdigit(n) (n >= '0' && n <= '9')

// Pin definitions

#define BUZZER_PIN              2
#define BLUE_LED_PIN            3
#define GREEN_LED_PIN           6
#define TEMP_PIN                7
#define RED_LED_PIN             9
#define SET_PIN                 PIN_A0
#define DOWN_PIN                PIN_A1
#define UP_PIN                  PIN_A2

#define OCTAVE_OFFSET           0
#define CELSIUS                 0
#define FAHRENHEIT              1

#define DS1307_ADDRESS          0x68

#define TimeIndex               0
#define DateIndex               1
#define AlarmIndex              2
#define hModeIndex              3
#define TemperatureIndex        4
#define TubeTimerIndex          5
#define TimeHoursIndex          6
#define TimeMintuesIndex        7
#define TimeSecondsIndex        8
#define DateFormatIndex         9 
#define DateDayIndex            10
#define DateMonthIndex          11
#define DateYearIndex           12
#define AlarmHourIndex          13
#define AlarmMinuteIndex        14
#define AlarmSecondIndex        15
#define Alarm01                 16
#define hModeValueIndex         17
#define DegreesFormatIndex      18
#define TubeTimerOffHourIndex   19
#define TubeTimerOffMinIndex    20
#define TubeTimerOnHourIndex    21
#define TubeTimerOnMinIndex     22
#define TubeTimerEnable         23

#define FIRST_PARENT            TimeIndex
#define LAST_PARENT             TubeTimerIndex
#define SETTINGS_COUNT          TubeTimerEnable + 1
#define NO_PARENT               0xFF
#define NP                      NO_PARENT
#define NO_CHILD                0
#define NC                      NO_CHILD

#define US_DateFormat           1
#define US_DF                   US_DateFormat
#define EU_DateFormat           0
#define EU_DF                   EU_DateFormat

#define DEBOUNCE_MS             15
#define TEMP_UPDATE_INTERVAL    1000

#define RED                     0
#define GREEN                   1
#define BLUE                    2

#define TESTING                 1
#define NOT_TESTING             0

// EEPROM Addresses
#define RGB_EEPROM_ADDR         0
#define HOURFORMAT_EEPROM_ADDR  1
#define ALARMTIME_EEPROM_ADDR   2 //2, 3, 4
#define ALARMARMED_EEPROM_ADDR  6
#define LEDLOCK_EEPROM_ADDR     7
#define LEDREDVAL_EEPROM_ADDR   8
#define LEDGREENVAL_EEPROM_ADDR 9
#define LEDBLUEVAL_EEPROM_ADDR  10
#define DEGFORMAT_EEPROM_ADDR   11
#define DATEFORMAT_EEPROM_ADDR  12
#define TT_OFF_HH_EEPROM_ADDR   13
#define TT_OFF_MM_EEPROM_ADDR   14
#define TT_ON_HH_EEPROM_ADDR    15
#define TT_ON_MM_EEPROM_ADDR    16
#define TT_ENABLE_EEPROM_ADDR   17

// Tones 
#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_CS1 35
#define NOTE_D1  37
#define NOTE_DS1 39
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_FS1 46
#define NOTE_G1  49
#define NOTE_GS1 52
#define NOTE_A1  55
#define NOTE_AS1 58
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_CS2 69
#define NOTE_D2  73
#define NOTE_DS2 78
#define NOTE_E2  82
#define NOTE_F2  87
#define NOTE_FS2 93
#define NOTE_G2  98
#define NOTE_GS2 104
#define NOTE_A2  110
#define NOTE_AS2 117
#define NOTE_B2  123
#define NOTE_C3  131
#define NOTE_CS3 139
#define NOTE_D3  147
#define NOTE_DS3 156
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349
#define NOTE_DS7 2489
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_FS7 2960
#define NOTE_G7  3136
#define NOTE_GS7 3322
#define NOTE_A7  3520
#define NOTE_AS7 3729
#define NOTE_B7  3951
#define NOTE_C8  4186
#define NOTE_CS8 4435
#define NOTE_D8  4699
#define NOTE_DS8 4978

int notes[] = { 0,
                NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4,
                NOTE_C5, NOTE_CS5, NOTE_D5, NOTE_DS5, NOTE_E5, NOTE_F5, NOTE_FS5, NOTE_G5, NOTE_GS5, NOTE_A5, NOTE_AS5, NOTE_B5,
                NOTE_C6, NOTE_CS6, NOTE_D6, NOTE_DS6, NOTE_E6, NOTE_F6, NOTE_FS6, NOTE_G6, NOTE_GS6, NOTE_A6, NOTE_AS6, NOTE_B6,
                NOTE_C7, NOTE_CS7, NOTE_D7, NOTE_DS7, NOTE_E7, NOTE_F7, NOTE_FS7, NOTE_G7, NOTE_GS7, NOTE_A7, NOTE_AS7, NOTE_B7
              };

// Some function prototypes
void get_colortemp(struct RGB_struct *rgb, int idx);
void doTest();
void getRTCTime();
void GetNTPData();
void ntp_rtcupdate();
float getTemperature (boolean bTempFormat);
void modesChanger();
bool isValidDate();
int extractDigits(byte b);
String PreZero(int digit);
void incrementValue();
void dicrementValue();
void checkAlarmTime();
void checkTubeTimers();
void setLEDsFromEEPROM();
String updateDisplayString();
void doDotBlink();
void setRTCDateTime(byte h, byte m, byte s, byte d, byte mon, byte y, byte w);
String updateDateString();
String updateTemperatureString(float fDegrees);
void setRTCDateTime(byte h, byte m, byte s, byte d, byte mon, byte y, byte w);

// Global variable declarations:
extern String stringToDisplay;
extern bool TubesOn;

#ifdef TEMP_LEDS

struct RGB_struct {
   byte  red;
   byte  green;
   byte  blue;
} colorTemp;

long templed_array[256] = {
0x000000FF, 0x000101FE, 0x000202FD, 0x000303FC, 0x000404FB, 0x000505FA, 0x000606F9, 0x000707F8,
0x000808F7, 0x000909F6, 0x000A0AF5, 0x000B0BF4, 0x000C0CF3, 0x000D0DF2, 0x000E0EF1, 0x000F0FF0,
0x001010EF, 0x001111EE, 0x001212ED, 0x001313EC, 0x001414EB, 0x001515EA, 0x001616E9, 0x001717E8,
0x001818E7, 0x001919E6, 0x001A1AE5, 0x001B1BE4, 0x001C1CE3, 0x001D1DE2, 0x001E1EE1, 0x001F1FE0,
0x002020DF, 0x002121DE, 0x002222DD, 0x002323DC, 0x002424DB, 0x002525DA, 0x002626D9, 0x002727D8,
0x002828D7, 0x002929D6, 0x002A2AD5, 0x002B2BD4, 0x002C2CD3, 0x002D2DD2, 0x002E2ED1, 0x002F2FD0,
0x003030CF, 0x003131CE, 0x003232CD, 0x003333CC, 0x003434CB, 0x003535CA, 0x003636C9, 0x003737C8,
0x003838C7, 0x003939C6, 0x003A3AC5, 0x003B3BC4, 0x003C3CC3, 0x003D3DC2, 0x003E3EC1, 0x003F3FC0,
0x004040BF, 0x004141BE, 0x004242BD, 0x004343BC, 0x004444BB, 0x004545BA, 0x004646B9, 0x004747B8,
0x004848B7, 0x004949B6, 0x004A4AB5, 0x004B4BB4, 0x004C4CB3, 0x004D4DB2, 0x004E4EB1, 0x004F4FB0,
0x005050AF, 0x005151AE, 0x005252AD, 0x005353AC, 0x005454AB, 0x005555AA, 0x005656A9, 0x005757A8,
0x005858A7, 0x005959A6, 0x005A5AA5, 0x005B5BA4, 0x005C5CA3, 0x005D5DA2, 0x005E5EA1, 0x005F5FA0,
0x0060609F, 0x0061619E, 0x0062629D, 0x0063639C, 0x0064649B, 0x0065659A, 0x00666699, 0x00676798,
0x00686897, 0x00696996, 0x006A6A95, 0x006B6B94, 0x006C6C93, 0x006D6D92, 0x006E6E91, 0x006F6F90,
0x0070708F, 0x0071718E, 0x0072728D, 0x0073738C, 0x0074748B, 0x0075758A, 0x00767689, 0x00777788,
0x00787887, 0x00797986, 0x007A7A85, 0x007B7B84, 0x007C7C83, 0x007D7D82, 0x007E7E81, 0x007F7F80,
0x00807F7F, 0x00817E7E, 0x00827D7D, 0x00837C7C, 0x00847B7B, 0x00857A7A, 0x00867979, 0x00877878,
0x00887777, 0x00897676, 0x008A7575, 0x008B7474, 0x008C7373, 0x008D7272, 0x008E7171, 0x008F7070,
0x00906F6F, 0x00916E6E, 0x00926D6D, 0x00936C6C, 0x00946B6B, 0x00956A6A, 0x00966969, 0x00976868,
0x00986767, 0x00996666, 0x009A6565, 0x009B6464, 0x009C6363, 0x009D6262, 0x009E6161, 0x009F6060,
0x00A05F5F, 0x00A15E5E, 0x00A25D5D, 0x00A35C5C, 0x00A45B5B, 0x00A55A5A, 0x00A65959, 0x00A75858,
0x00A85757, 0x00A95656, 0x00AA5555, 0x00AB5454, 0x00AC5353, 0x00AD5252, 0x00AE5151, 0x00AF5050,
0x00B04F4F, 0x00B14E4E, 0x00B24D4D, 0x00B34C4C, 0x00B44B4B, 0x00B54A4A, 0x00B64949, 0x00B74848,
0x00B84747, 0x00B94646, 0x00BA4545, 0x00BB4444, 0x00BC4343, 0x00BD4242, 0x00BE4141, 0x00BF4040,
0x00C03F3F, 0x00C13E3E, 0x00C23D3D, 0x00C33C3C, 0x00C43B3B, 0x00C53A3A, 0x00C63939, 0x00C73838,
0x00C83737, 0x00C93636, 0x00CA3535, 0x00CB3434, 0x00CC3333, 0x00CD3232, 0x00CE3131, 0x00CF3030,
0x00D02F2F, 0x00D12E2E, 0x00D22D2D, 0x00D32C2C, 0x00D42B2B, 0x00D52A2A, 0x00D62929, 0x00D72828,
0x00D82727, 0x00D92626, 0x00DA2525, 0x00DB2424, 0x00DC2323, 0x00DD2222, 0x00DE2121, 0x00DF2020,
0x00E01F1F, 0x00E11E1E, 0x00E21D1D, 0x00E31C1C, 0x00E41B1B, 0x00E51A1A, 0x00E61919, 0x00E71818,
0x00E81717, 0x00E91616, 0x00EA1515, 0x00EB1414, 0x00EC1313, 0x00ED1212, 0x00EE1111, 0x00EF1010,
0x00F00F0F, 0x00F10E0E, 0x00F20D0D, 0x00F30C0C, 0x00F40B0B, 0x00F50A0A, 0x00F60909, 0x00F70808,
0x00F80707, 0x00F90606, 0x00FA0505, 0x00FB0404, 0x00FC0303, 0x00FD0202, 0x00FE0101, 0x00FF0000,
};
#endif


#ifdef NTP_SYNC_ENABLE

#define NTP_BUFFER_LENGTH   100
#define NTP_SYNC_INTERVAL   1000 * 5

char ntp_buffer[NTP_BUFFER_LENGTH];
byte ntp_position = 0;
unsigned long prev_ntp_sync = 0;
bool ntp_sync_pending = false;

struct sNTP_DATE_TIME
{
  byte ntp_seconds;
  byte ntp_minutes;
  byte ntp_hours;
  byte ntp_day;
  byte ntp_month;
  byte ntp_year;
};

sNTP_DATE_TIME ntp_date_time;

TimeChangeRule *tcr;        // pointer to the time change rule, use to get the TZ abbrev

// US Pacific Time Zone (Las Vegas, Los Angeles)
// for more examples on your timezone: https://github.com/JChristensen/Timezone
TimeChangeRule usPDT = {"PDT", Second, Sun, Mar, 2, -420};
TimeChangeRule usPST = {"PST", First, Sun, Nov, 2, -480};
Timezone myTZ(usPDT, usPST);

#endif

// GPS variables
#ifdef GPS_SYNC_ENABLE
#define GPS_BUFFER_LENGTH 83
char GPS_Package[GPS_BUFFER_LENGTH];
byte GPS_position=0;

struct GPS_DATE_TIME
{
  byte GPS_hours;
  byte GPS_minutes;
  byte GPS_seconds;
  byte GPS_day;
  byte GPS_mounth;
  int GPS_year; 
  bool GPS_Valid_Data=false;
  unsigned long GPS_Data_Parsed_time;
};

GPS_DATE_TIME GPS_Date_Time;
#endif
