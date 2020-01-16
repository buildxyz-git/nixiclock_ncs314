# BuildXYZ firmware the NCS314 NixieClock from GRA&AFCH.

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

![BuildXYZ NixieClock Menu](https://www.buildxyz.xyz/wp-content/uploads/NixieClockModeFlow.jpg)
