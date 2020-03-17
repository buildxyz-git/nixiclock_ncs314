# BuildXYZ firmware for the NCS314 NixieClock from GRA&AFCH.

Updates include:
- General code refactoring
- Move most configuration parameters to NixieConfig.h
- Remove IR Remote functionality
- Enable/Disable GPS in config file
- Enable/Disable Debugging in config file
- Added more tunes in config file
- Tube timer feature with on/off timer to turn tube off at night
- Temperature LED feature to reflect room temperature in LED color
- Enable/Disable Colons in config file
- Remove IR functionality
- Remove hours offset in menu
- Bugfix: Timer3 interrupt being used by tone library not allowing red LED PWM on D9, opt for block tone library 

# Installation
clone this repo!
`git clone https://github.com/buildxyz-git/nixiclock_ncs314.git`

Install a few libraries:
[TimeZone](https://github.com/JChristensen/Timezone)
[TimerFreeTone](https://bitbucket.org/teckel12/arduino-timer-free-tone/downloads/)

Copy the patched version of RTTTL included in this repo
`NixieClock_NCS314_Firmware> cp -r ./libs/Rtttl [/PATH/TO/YOUR/ARDUINO/LIBRARIES]`

Open the main sketch (NixieClock_NCS314_Firmware.ino) and build!

![BuildXYZ NixieClock Menu](https://www.buildxyz.xyz/wp-content/uploads/NixieClockModeFlow.jpg)
