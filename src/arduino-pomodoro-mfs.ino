// ------------------------------------------------------------
// Project: Arduino Pomodoro Timer
// Hardware:  Multi Function Shield (MFS), DS1302 RTC module
// Author: https://github.com/beschasny
// License:   MIT (see LICENSE file in repository)
// ------------------------------------------------------------
//
// Description:
//   Pomodoro timer for Arduino Uno R3 using a Multi Function
//   Shield (buttons, display, buzzer) and a DS1302 RTC module.
//   Features include sprint tracking, user settings, menu system,
//   statistics, and more.
//
// Versioning:
//   For version history, see git tags/releases on GitHub.
// ------------------------------------------------------------

// ------------------------------------------------------------
// Includes
// ------------------------------------------------------------

#include <MultiFuncShield.h> // https://github.com/beschasny/MultiFuncShield-Library
#include <Ds1302.h>          // https://github.com/Treboada/Ds1302
#include <EEPROM.h>
#include <avr/sleep.h>

// ------------------------------------------------------------
// Pinouts
// ------------------------------------------------------------

// DS1302 RTC module
const byte rtcRstPin                          = 5;
const byte rtcClkPin                          = 9;
const byte rtcDatPin                          = 6;
Ds1302 rtc(rtcRstPin, rtcClkPin, rtcDatPin);
// Buzzer
const byte buzzerPin                          = 3;
// Interruption
const byte isrPin                             = 2;

// ------------------------------------------------------------
// Enumeration types and related variables
// ------------------------------------------------------------

// States
enum OperatingStates {
  STANDBY,
  COUNTING,
  PAUSED,
  SLEEPING,
  DATETIMESETUP,
  CLEARING,
  ERROR
};
byte operatingState                           = STANDBY;
byte errorCode                                = 0;
unsigned long lastPausedBeep                  = 0;

struct States {
  bool isDisplayEnabled                       : 1;
  bool isSleeping                             : 1;
  bool isTempModeButtonPressed                : 1;
  bool isSprintPaused                         : 1;
  bool inSubmenu                              : 1;
  bool inStatisticsSubmenu                    : 1;
  bool inStatisticsDetailsSubmenu             : 1;
  bool inConfigSubmenu                        : 1;
};
States state;

// Menu modes
enum MainMenuModes {
  SPRINT,
  STATISTICS,
  CONFIG,
  CLOCK,
  MENU_MODES_COUNT
};
enum StatisticsMenuModes {
  DAILY,
  TOTAL,
  MINIMUM,
  AVERAGE,
  MAXIMUM,
  CLEAR,
  SUBMENU_STATISTICS_MODES_COUNT
};
enum StatisticsPeriods {
  YEARLY,
  MONTHLY,
  WEEKLY,
  STATISTICS_PERIODS_COUNT
};
enum ConfigMenuModes {
  LCD,
  BUZZ,
  TUNE,
  CUE,
  LEDS,
  COUNTED_DOW,
  COUNT_UNITS,
  HERO,
  SLEEP,
  BUILD,
  SUBMENU_CONFIG_MODES_COUNT
};
enum CountedDoW {
  FIVE_DAYS,
  SIX_DAYS,
  SEVEN_DAYS,
  NOT_0_DAYS,
  COUNTED_DOW_COUNT
};
enum DateTimeModes {
  HOURS_MINUTES,
  DAY_MONTH,
  YEAR,
  DATETIME_MODES_COUNT
};

byte mainMenuMode                             = SPRINT;
byte configMenuMode                           = LCD;
byte statisticsMenuMode                       = DAILY;
byte statisticsPeriod                         = YEARLY;
byte dateTimeMode                             = HOURS_MINUTES;

// ------------------------------------------------------------
// String constants
// ------------------------------------------------------------

// Menu items
// Use special characters to display letter combinations
const char mainMenuItems[][6] PROGMEM         = {
  "run>", // Run sprint
  "data", // Statistics data
  "conf", // Configuration settings
  "cloc"  // Clock and calendar
};
const char statisticsMenuItems[][6] PROGMEM   = {
  "da{u", // Daily statistics
  "totl", // Total sprints
  "#iin", // Minimum
  "#iid", // Middle (Average, mean value)
  "#iax", // Maximum
  "clr"   // Clear all statistics data
};
const char statisticsPeriods[][6] PROGMEM     = {
  "365d", // Last year
  " 30d", // Last month
  "  7d"  // Last week
};
const char configMenuItems[][6] PROGMEM       = {
  "1cd",   // Display settings
  "buzz",  // Buzzer
  "tunz",  // Tune
  "cuz",   // Cue
  "f}cr",  // Onboard flicker LEDs
  "cnt.d", // Counted DoW
  "cnt.u", // Counting units
  "hzro",  // Hero mode
  "hbrn",  // Sleep (hibernation)
  "bu{d"   // Build version
};
const char countedDoWItems[][6] PROGMEM       = {
  "1-5d",  // Mon-Fri
  "1-6d",  // Mon-Sat
  "1-7d",  // All days
  "not0"   // All non-zero days
};

// Alerts with terminator string
const char alertSuccess[][6] PROGMEM          = {"nicz", "job!", ""};
const char alertClearStatistics[][6] PROGMEM  = {"clr", "a|", "data", ""};

// ------------------------------------------------------------
// Global variables and configuration
// ------------------------------------------------------------

// Countdown and statistics
byte currentInterval                          = 0;
byte tenths                                   = 0;
char seconds                                  = 0;
char minutes                                  = 0;
uint8_t sprintCounter                         = 0;
uint16_t currentDayOfYear                     = 1;

// Using int with x10 multiplier to simulate float and reduce memory usage
int statisticsArray[SUBMENU_STATISTICS_MODES_COUNT - 1][STATISTICS_PERIODS_COUNT];

// Configs
#define VERSION                               111
#define CONFIG_SIGNATURE                      0x42 // 66
#define CONFIG_ADDRESS                        1015
#define EMPTY_VALUE                           0xFF
#define SPRINTS_END_BYTE                      0xED // 237

struct Config {
  byte signature;                                  // Signature to detect if EEPROM is initialized
  byte version;                                    // Version to detect updates
  byte displayBrightness                      : 4; // 0-3 (max -> min)
  bool buzzEnabled                            : 1; // true/false
  byte tune                                   : 4; // 0-10
  bool cueEnabled                             : 1; // true/false
  bool ledsEnabled                            : 1; // true/false
  byte countingDoWs                           : 4; // 0-3 (5d, 6d, 7d, not 0)
  byte countingUnits                          : 1; // 0-1
  bool heroModeEnabled                        : 1; // true/false
  bool muteModeEnabled                        : 1; // true/false
  // Future settings must be added here at the end
  // to preserve EEPROM bitfield layout
};
Config config;

// Animation
unsigned long animationStartTime              = 0;
unsigned long animationPrevFrameTime          = 0;
int animationIndex = 0;
unsigned long lastSprintToggleTime            = 0;
bool showSprint                               = true;

// Characters used for cycle animation
const char* animatedCharacters[]              = {"~", ">", "1", "v", "_", "e", "I", "<"};
const int animatedCharactersSize              = sizeof(animatedCharacters) / sizeof(animatedCharacters[0]);
static bool showStaticInfo                    = false;

// ------------------------------------------------------------
// Setup
// ------------------------------------------------------------

void setup() {
  delay(100);
  // Make random sequence less predictable
  randomSeed(analogRead(A5));
  Serial.begin(9600);

  // Load config on startup
  loadConfig();
  config.muteModeEnabled           = false;

  // Set states
  state.isDisplayEnabled           = true;
  state.isSleeping                 = false;
  state.isTempModeButtonPressed    = false;
  state.isSprintPaused             = false;
  state.inSubmenu                  = false;
  state.inStatisticsSubmenu        = false;
  state.inStatisticsDetailsSubmenu = false;
  state.inConfigSubmenu            = false;

  // Initialize RTC and set date and time (on init or after reset)
  rtc.init();
  if (rtc.isHalted()) {
    Serial.println(F("Error (#01): RTC is halted"));
    errorCode = 1;
    operatingState = ERROR;
  } else {
    // Get current day of year to use as current EEPROM address,
    // also check date and time for sanity
    getCurrentDayOfYear();
  }

  // If tomorrow + 1 cell isn't empty, the cycle is complete or something is wrong
  if (errorCode == 0 && EEPROM.read(currentDayOfYear + 2) != EMPTY_VALUE) {
    Serial.println(F("Error (#03): Already existed record (data not cleared)"));
    operatingState = ERROR;
    errorCode = 3;
  }

  // If no errors check if it is needed to update sprintCounter and skipped days
  if (errorCode == 0) {
    uint8_t savedSprintCounter = EEPROM.read(currentDayOfYear);
    if (savedSprintCounter != EMPTY_VALUE 
      && savedSprintCounter != SPRINTS_END_BYTE 
      && EEPROM.read(currentDayOfYear + 1) == SPRINTS_END_BYTE) {

      sprintCounter = savedSprintCounter;
      Serial.print(F("Debug (sprint): Saved counter: "));
      Serial.println(sprintCounter);
    } else {
      Serial.println(F("Debug (sprint): No saved counter for today"));

      // Check if some days are skipped
      // Get latest saved date
      int latestSavedDayOfYear = 0;
      for (int address = 2; address <= 367; address++) {
        if (EEPROM.read(address) == SPRINTS_END_BYTE) {
          latestSavedDayOfYear = address - 1;
        }
      }
      Serial.print(F("Debug (sprint): Latest saved day of year: "));
      Serial.println(latestSavedDayOfYear);

      int skippedDays = currentDayOfYear - latestSavedDayOfYear;
      if (skippedDays < 0 && latestSavedDayOfYear > 0) {
        // New year begins
        Serial.println(F("Debug (sprint): New year begins"));
        // Get current year
        Ds1302::DateTime now;
        rtc.getDateTime(&now);
        int currentYear = 2000 + now.year;
        int previousYear = currentYear - 1;
        int daysInPreviousYear = isLeapYear(previousYear) ? 366 : 365;

        Serial.println(F("Debug (sprint): Filling skipped days of old year with zeroes"));
        for (int i = latestSavedDayOfYear + 1; i <= daysInPreviousYear; i++) {
          Serial.print(F("Debug (sprint): Set 0 value for cell: "));
          Serial.println(i);
          EEPROM.update(i, 0);
          delay(5);
        }

        Serial.println(F("Debug (sprint): Filling skipped days of new year with zeroes"));
        for (int i = 1; i <= currentDayOfYear; i++) {
          Serial.print(F("Debug (sprint): Set 0 value for cell: "));
          Serial.println(i);
          EEPROM.update(i, 0);
          delay(5);
        }
      } else if (skippedDays > 0 && latestSavedDayOfYear > 0) {
        Serial.println(F("Debug (sprint): Filling skipped days of current year with zeroes"));
        for (int i = 1; i <= skippedDays; i++) {
          int skippedDayOfYear = latestSavedDayOfYear + i;
          Serial.print(F("Debug (sprint): Set 0 value for cell: "));
          Serial.println(skippedDayOfYear);
          // Fill skipped day with 0
          EEPROM.update(skippedDayOfYear, 0);
          delay(5);
        }

      }

      // Save end byte signature
      if (skippedDays != 0 && latestSavedDayOfYear > 0) {
        EEPROM.update(currentDayOfYear + 1, SPRINTS_END_BYTE);
        delay(5);
      }
    }
  }

  Serial.println(F("EEPROM:"));
  for (int address = 0; address <= 1023; address++) {
    uint8_t value = EEPROM.read(address);
    if (value != EMPTY_VALUE) {
      Serial.print(F("Cell "));
      Serial.print(address);
      Serial.print(F(": "));
      Serial.println(value);
      delay(5);
    }
  }
  Serial.println(F("EEPROM: Finished reading"));

  // Initialize MFS
  MFS.initialize();
  MFS.setDisplayBrightness(config.displayBrightness);

  // Set up button press interrupt (using pin D2 as input with internal pull-up)
  pinMode(isrPin, INPUT_PULLUP);
  delay(500);
}

// ------------------------------------------------------------
// Loop
// ------------------------------------------------------------

void loop() {
  byte btn = MFS.getButton();
  char formattedString[8];

  switch (operatingState) {
    case STANDBY: {
      if (!state.inSubmenu) {
        // Main menu navigation
        strcpy_P(formattedString, mainMenuItems[mainMenuMode]);
        MFS.write(formattedString);

        if (btn == BUTTON_1_SHORT_RELEASE) {
          // Return to initial state
          mainMenuMode = SPRINT;
        } else if (btn == BUTTON_1_LONG_PRESSED) {
          // Set sleeping state
          Serial.println(F("Debug (sleep): Set sleeping state"));
          operatingState = SLEEPING;
        } else if (btn == BUTTON_2_SHORT_RELEASE) {
          // Cycle to next menu item
          mainMenuMode = (mainMenuMode + 1) % MENU_MODES_COUNT;
        } else if (btn == BUTTON_3_SHORT_RELEASE) {
          if (mainMenuMode == STATISTICS) {
            // Calculate statistics
            calculateStatistics();
          } else if (mainMenuMode == SPRINT) {
            // Prepare to show daily sprints
            startAnimation();
          }

          // Enter submenu
          state.inSubmenu = true;
        }
      } else {
        // Submenus navigation
        switch (mainMenuMode) {
          case SPRINT:
            subMenuSprint(btn);
            break;

          case STATISTICS:
            subMenuStatistics(btn);
            break;

          case CONFIG:
            subMenuConfig(btn);
            break;

          case CLOCK:
            subMenuClock(btn);
            break;
        }
      }
      break;
    }

    case COUNTING: {
      MFS.blinkDisplay(DIGIT_ALL, OFF);
      updateLeds();

      // Temporary toggle buzz, tune or hero mode settings
      if (btn == BUTTON_1_LONG_PRESSED || btn == BUTTON_2_LONG_PRESSED || btn == BUTTON_3_LONG_PRESSED) {
        state.isTempModeButtonPressed = true;
      } else if (btn == BUTTON_1_LONG_RELEASE || btn == BUTTON_2_LONG_RELEASE || btn == BUTTON_3_LONG_RELEASE) {
        state.isTempModeButtonPressed = false;
        delay(1000);
      }

      // Temporary toggle buzz setting
      if (btn == BUTTON_1_LONG_PRESSED) {
        strcpy_P(formattedString, configMenuItems[BUZZ]);
        MFS.write(formattedString);
      } else if (btn == BUTTON_1_LONG_RELEASE) {
        config.buzzEnabled = !config.buzzEnabled;
        if (config.buzzEnabled == 0) {
          MFS.write("off");
        } else {
          MFS.write("on");
        }
        delay(1000);
      } else if (btn == BUTTON_2_LONG_PRESSED) {
        // Temporary toggle tune setting
        strcpy_P(formattedString, configMenuItems[TUNE]);
        MFS.write(formattedString);
      } else if (btn == BUTTON_2_LONG_RELEASE) {
        config.muteModeEnabled = !config.muteModeEnabled;
        if (config.muteModeEnabled == 0) {
          MFS.write("on");
        } else {
          MFS.write("off");
        }
        delay(1000);
      } else if (btn == BUTTON_3_LONG_PRESSED) {
        // Temporary toggle hero mode setting
        strcpy_P(formattedString, configMenuItems[HERO]);
        MFS.write(formattedString);
      } else if (btn == BUTTON_3_LONG_RELEASE) {
        config.heroModeEnabled = !config.heroModeEnabled;
        if (config.heroModeEnabled == 0) {
          MFS.write("off");
        } else {
          MFS.write("on");
        }
        delay(1000);
      } else if (btn == BUTTON_3_SHORT_RELEASE) {
        // Pause counting
        if (config.buzzEnabled) {
          MFS.beep(5, 5, 1);
        }
        // Set Paused state
        operatingState = PAUSED;
        lastPausedBeep = millis();
      } else {
        // Show animation at the beginning of the break
        if ((currentInterval == 5 || currentInterval == 15) && !showStaticInfo) {
          //runAnimation(1500);
          runAnimation(3000);
          return;
        }

        // Continue counting
        tenths++;

        if (tenths == 10) {
          tenths = 0;
          seconds--;

          if (seconds < 0 && minutes > 0) {
            seconds = 59;
            minutes--;

            if (currentInterval == 15 || currentInterval == 5) {
              if (config.buzzEnabled && config.cueEnabled) {
              //if (config.buzzEnabled) {
                MFS.beep(5, 5, 1);
              }
            }
          }

          if (minutes == 0 && seconds == 0) {
            if (currentInterval == 25) {
              // Current sprint finished
              MFS.write("00.00");

              turnOffBlinkAndLeds();

              setInterval(0);
              if (config.buzzEnabled) {
                if (config.tune > 0 && !config.muteModeEnabled) {
                  playMelody(config.tune);
                } else {
                  MFS.beep(5, 5, 3);
                }
              }
              // Increase sprint counter
              sprintCounter++;

              // Saving data to EEPROM
              saveSprint();

              blinkAlert(alertSuccess, 4, 250);
              startAnimation();

              // Prepare breaks
              if (sprintCounter % 4 == 0) {
                setInterval(15);
              } else {
                setInterval(5);
              }
            } else if (currentInterval == 15 || currentInterval == 5) {
              // Break finished, prepare new interval
              if (config.buzzEnabled) {
                MFS.beep(5, 5, 3);
              }
              setInterval(25);
            }
          }

          if (showStaticInfo && !state.isTempModeButtonPressed) {
            // Show countdown value
            sprintf(formattedString, "%02d.%02d", minutes, seconds);
            MFS.write(formattedString);
          }
        }
        delay(100);
      }
      break;
    }

    case PAUSED: {
      // Show latest countdown value
      sprintf(formattedString, "%02d.%02d", minutes, seconds);
      MFS.write(formattedString);

      state.isSprintPaused = true;

      // Beep every minute to remind that sprint is paused
      unsigned long now = millis();
      if (config.buzzEnabled && config.cueEnabled && (now - lastPausedBeep >= 60000)) {
      //if (config.buzzEnabled && (now - lastPausedBeep >= 60000)) {
        MFS.beep(5, 5, 1);
        lastPausedBeep = now;
      }

      // MFS.blinkDisplay(DIGIT_ALL, ON);
      MFS.blinkDisplay(DIGIT_4, ON);
      if (btn == BUTTON_1_LONG_PRESSED && (minutes + seconds) > 0) {
        // Stop timer
        if (config.buzzEnabled) {
          MFS.beep(5, 5, 3);
        }
        setInterval(0);
        state.isSprintPaused = false;

        turnOffBlinkAndLeds();

        // Set StandBy state to exit
        operatingState = STANDBY;
      } else if (btn == BUTTON_3_SHORT_RELEASE && (minutes + seconds) > 0) {
        // Unpause timer
        if (config.buzzEnabled) {
          MFS.beep(5, 5, 2);
        }
        // Set Counting state to continue
        operatingState = COUNTING;
      }

      if (btn == BUTTON_1_SHORT_RELEASE) {

        turnOffBlinkAndLeds();

        state.inSubmenu = false;

        // Set Counting state to continue
        operatingState = STANDBY;
      }
      break;
    }

    case SLEEPING: {
      // Sleeping state, turn off display and LEDs
      MFS.write("");
      turnOffBlinkAndLeds();

      if (!state.isSleeping) {
        sleep();
      }

      if (btn == BUTTON_2_SHORT_RELEASE || btn == BUTTON_1_SHORT_RELEASE) {
        // Set StandBy state to completely wake up
        operatingState = STANDBY;
      }
      break;
    }

    case DATETIMESETUP: {
      setDateTime(btn);
      break;
    }

    case CLEARING: {
      strcpy_P(formattedString, statisticsMenuItems[CLEAR]);
      MFS.write(formattedString);

      if (btn == BUTTON_3_LONG_RELEASE) {
        clearStatistics();
      } else if (btn == BUTTON_1_SHORT_RELEASE) {
        // Set StandBy state to return to Statistics menu
        operatingState = STANDBY;
      }

      break;
    }

    case ERROR: {
      sprintf(formattedString, "Er.%02d", errorCode);
      MFS.write(formattedString);

      if (errorCode == 1 || errorCode == 2) {
        if (btn == BUTTON_3_LONG_RELEASE) {
          btn = 0;
          operatingState = DATETIMESETUP;
        }
      }

      if (errorCode == 3) {
        if (btn == BUTTON_3_LONG_RELEASE) {
          operatingState = STANDBY;
          errorCode = 0;
        }
      }

      break;
    }
  }
}

// ------------------------------------------------------------
// Helper functions
// ------------------------------------------------------------

// ---- Config Handling ----

// Load config from EEPROM or initialize with defaults
void loadConfig() {
  EEPROM.get(CONFIG_ADDRESS, config);

  // Check if first run or invalid config
  if (config.signature != CONFIG_SIGNATURE) {
    Serial.println(F("EEPROM: CONFIG_SIGNATURE not found, seems like first run"));

    initConfig();
    saveConfig();
  } else if (config.version != VERSION) {
    Serial.println(F("EEPROM: Version is changed, doing migration"));
    // Version is changed, store old config values to keep it
    Config oldConfig;
    EEPROM.get(CONFIG_ADDRESS, oldConfig);

    initConfig();

    // Copy old fields to new config (if fields still exist)
    config.displayBrightness = oldConfig.displayBrightness;
    config.buzzEnabled       = oldConfig.buzzEnabled;
    config.tune              = oldConfig.tune;
    config.cueEnabled        = oldConfig.cueEnabled;
    config.ledsEnabled       = oldConfig.ledsEnabled;
    config.countingDoWs      = oldConfig.countingDoWs;
    config.countingUnits     = oldConfig.countingUnits;
    config.heroModeEnabled   = oldConfig.heroModeEnabled;
    config.muteModeEnabled   = oldConfig.muteModeEnabled;

    // Assign new fields here
    // config.futureSetting  = true;

    config.signature         = CONFIG_SIGNATURE;
    config.version           = VERSION;

    saveConfig();
  }
}

// Save config to EEPROM
void saveConfig() {
  Serial.println(F("EEPROM: Save config"));
  EEPROM.put(CONFIG_ADDRESS, config);
  delay(5);
}

// Initialize config with defaults
void initConfig() {
  // Set default values starting from CONFIG_ADDRESS
  Serial.println(F("EEPROM: Set default values"));
  config.signature           = CONFIG_SIGNATURE;
  config.version             = VERSION;
  config.displayBrightness   = 3;
  config.buzzEnabled         = true;
  config.tune                = 1;
  config.cueEnabled          = false;
  config.ledsEnabled         = true;
  config.countingDoWs        = 0;
  config.countingUnits       = 1;
  config.heroModeEnabled     = false;
  config.muteModeEnabled     = false;
}

// ---- Date and Time Operations ----

// Check if the year is leap
bool isLeapYear(uint16_t year) {
  return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

// Calculate day of week according to ISO 8601 (1 = Monday, 7 = Sunday)
uint8_t calculateDOW(uint16_t year, uint8_t month, uint8_t day) {
  if (month < 3) {
    month += 12;
    year--;
  }
  uint8_t dow = (day + 2 * month + 3 * (month + 1) / 5 + year + year / 4 - year / 100 + year / 400 + 1) % 7;

  return dow == 0 ? 7 : dow;
}

// Get current day number
void getCurrentDayOfYear() {
  Ds1302::DateTime now;
  rtc.getDateTime(&now);
  Serial.println(F("Debug (RTC): Delay before getting current day of year"));
  delay(50);

  if (isRtcDateTimeSane(now)) {
    uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint16_t dayOfYear = now.day;

    // Add days from previous months
    for (uint8_t i = 0; i < now.month - 1; i++) {
      dayOfYear += daysInMonth[i];
    }

    // Add +1 for leap year if month is after February
    if (now.month > 2 && isLeapYear(2000 + now.year)) {
      dayOfYear++;
    }

    currentDayOfYear = dayOfYear;
    Serial.print(F("Debug (sprint): Current day of year: "));
    Serial.println(currentDayOfYear);
  } else {
    Serial.println(F("Error (#02): RTC isn't sane, invalid datetime"));
    errorCode = 2;
    operatingState = ERROR;
  }
}

// Check RTC date and time for sanity
bool isRtcDateTimeSane(Ds1302::DateTime now) {
  Serial.println(F("Debug (RTC): calling isRtcDateTimeSane()"));
  Serial.println(F("Debug (RTC): Now:")); delay(10);
  Serial.print(2000 + now.year); delay(10);
  Serial.print(F("-")); delay(10);
  Serial.print(now.month); delay(10);
  Serial.print(F("-")); delay(10);
  Serial.print(now.day); delay(10);
  Serial.print(F(" ")); delay(10);
  Serial.print(now.hour); delay(10);
  Serial.print(F(":")); delay(10);
  Serial.print(now.minute); delay(10);
  Serial.print(F(":")); delay(10);
  Serial.println(now.second); delay(10);

  // Check year range
  if (now.year < 25 || now.year > 50) return false;

  // Check month
  if (now.month < 1 || now.month > 12) return false;

  // Check day of month
  uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (isLeapYear(2000 + now.year) && now.month == 2) daysInMonth[1] = 29;

  if (now.day < 1 || now.day > daysInMonth[now.month - 1]) return false;

  // Check hours, minutes, seconds
  if (now.hour > 23 || now.minute > 59 || now.second > 59) return false;

  return true;
}

// Set new date and time
void setDateTime(byte btn) {
  // currentField: 0-9, each number corresponds to a digit:
  // 0 = hour tens, 1 = hour units, 2 = minute tens, 3 = minute units,
  // 4 = day tens, 5 = day units, 6 = month tens, 7 = month units,
  // 8 = year tens, 9 = year units
  static uint8_t currentField = 0;
  static uint8_t hourTens, hourUnits;
  static uint8_t minuteTens, minuteUnits;
  static uint8_t dayTens, dayUnits;
  static uint8_t monthTens, monthUnits;
  static uint8_t yearTens, yearUnits;
  static bool initialized = false;
  char formattedString[8];

  if (!initialized) {
    switch (dateTimeMode) {
      case HOURS_MINUTES: currentField = 0; break;
      case DAY_MONTH: currentField = 4; break;
      case YEAR: currentField = 8; break;
    }

    // Initialize digits from RTC
    Ds1302::DateTime now;
    rtc.getDateTime(&now);

    if (isRtcDateTimeSane(now)) {
      Serial.println(F("Debug (RTC): Sane, get current values"));
      hourTens = now.hour / 10;
      hourUnits = now.hour % 10;
      minuteTens = now.minute / 10;
      minuteUnits = now.minute % 10;
      dayTens = now.day / 10;
      dayUnits = now.day % 10;
      monthTens = now.month / 10;
      monthUnits = now.month % 10;
      yearTens = now.year / 10;
      yearUnits = now.year % 10;
    } else {
      // Fallback default 12:30 20.05.25
      Serial.println(F("Debug (RTC): Not sane, set fallback values"));
      hourTens = 1; hourUnits = 2;
      minuteTens = 3; minuteUnits = 0;
      dayTens = 2; dayUnits = 0;
      monthTens = 0; monthUnits = 5;
      yearTens = 2; yearUnits = 5;
    }
    initialized = true;
  }

  // Handle buttons
  switch (btn) {
    case BUTTON_1_SHORT_RELEASE:
    case BUTTON_3_LONG_PRESSED:
      if (btn == BUTTON_3_LONG_PRESSED) {
        // Save new values to RTC
        Serial.println(F("Debug (RTC): Save new values"));
        Ds1302::DateTime dt;
        dt.hour = hourTens * 10 + hourUnits;
        dt.minute = minuteTens * 10 + minuteUnits;
        dt.day = dayTens * 10 + dayUnits;
        dt.month = monthTens * 10 + monthUnits;
        dt.year = yearTens * 10 + yearUnits;
        dt.second = 0;
        dt.dow = calculateDOW(2000 + dt.year, dt.month, dt.day);
        rtc.setDateTime(&dt);
      }
      initialized = false;
      operatingState = STANDBY;
      errorCode = 0;
      MFS.blinkDisplay(DIGIT_ALL, OFF);
      return;

    case BUTTON_2_SHORT_RELEASE:
      // Increment current digit with clamping
      switch (currentField) {
        case 0: // hour tens (0-2)
          hourTens = (hourTens + 1) % 3;
          // clamp hourUnits max 3 if tens=2
          if (hourTens == 2 && hourUnits > 3) hourUnits = 3;
          break;
        case 1: // hour units (0-9 or 0-3 if tens=2)
          hourUnits = (hourUnits + 1) % 10;
          if (hourTens == 2 && hourUnits > 3) hourUnits = 0;
          break;
        case 2: // minute tens (0-5)
          minuteTens = (minuteTens + 1) % 6;
          break;
        case 3: // minute units (0-9)
          minuteUnits = (minuteUnits + 1) % 10;
          break;
        case 4: // day tens (0-3)
          dayTens = (dayTens + 1) % 4;
          // Clamp dayUnits based on dayTens (max 31 days)
          if (dayTens == 3 && dayUnits > 1) dayUnits = 1;
          break;
        case 5: // day units (0-9, max 1 if tens=3)
          dayUnits = (dayUnits + 1) % 10;
          if (dayTens == 3 && dayUnits > 1) dayUnits = 0;
          break;
        case 6: // month tens (0-1)
          monthTens = (monthTens + 1) % 2;
          // clamp month max 12
          if (monthTens == 1 && monthUnits > 2) monthUnits = 2;
          break;
        case 7: // month units (0-9, max 2 if tens=1)
          monthUnits = (monthUnits + 1) % 10;
          if (monthTens == 1 && monthUnits > 2) monthUnits = 0;
          break;
        case 8: // year tens (0-9)
          yearTens = (yearTens + 1) % 10;
          break;
        case 9: // year units (0-9)
          yearUnits = (yearUnits + 1) % 10;
          break;
      }
      break;

    case BUTTON_3_SHORT_RELEASE:
      currentField = (currentField + 1) % 10;
      break;
  }

  // Format: HH:MM or DD.MM or 20YY for year
  if (currentField <= 3) {
    sprintf(formattedString, "%d%d.%d%d", hourTens, hourUnits, minuteTens, minuteUnits);
  } else if (currentField <= 7) {
    sprintf(formattedString, "%d%d.%d%d", dayTens, dayUnits, monthTens, monthUnits);
  } else {
    sprintf(formattedString, "20%d%d", yearTens, yearUnits);
  }
  MFS.write(formattedString);

  // Blink only the current digit
  MFS.blinkDisplay(DIGIT_ALL, OFF);
  // Map currentField to actual digit positions on display (0..3)
  if (currentField <= 3) {
    MFS.blinkDisplay(1 << currentField, ON);
  } else if (currentField <= 7) {
    MFS.blinkDisplay(1 << (currentField - 4), ON);
  } else {
    MFS.blinkDisplay(1 << (currentField - 6), ON);
  }
}

// ---- Sprints Handling ----

// Set new sprint interval
void setInterval(int value) {
  currentInterval = value;
  minutes = value;
  seconds = 0;

  // Reset break if Hero mode is enabled
  if (config.heroModeEnabled && (value == 5 || value == 15)) {
    minutes = 0;
    seconds = 1;
  }
}

// Save current sprint counter according to current day of year
void saveSprint() {
  uint8_t initialDayOfYear = currentDayOfYear;
  // Update current day of year
  getCurrentDayOfYear();

  // If tomorrow + 1 cell isn't empty, the cycle is complete or something is wrong
  if (errorCode == 0 && EEPROM.read(currentDayOfYear + 2) != EMPTY_VALUE) {
    Serial.println(F("Error (#03): Already existed record (data not cleared)"));
    operatingState = ERROR;
    errorCode = 3;
  }

  if (errorCode == 0) {
    if (currentDayOfYear == initialDayOfYear) {
      Serial.println(F("Debug (sprint): Same day"));
    } else {
      Serial.println(F("Debug (sprint): New day"));
      sprintCounter = 1;
    }

    Serial.print(F("Debug (sprint): Save current counter "));
    Serial.print(sprintCounter);
    Serial.print(F(" into cell "));
    Serial.println(currentDayOfYear);

    // Save sprint counter
    EEPROM.update(currentDayOfYear, sprintCounter);
    delay(5);

    // Clear end byte signatures (if any)
    for (int address = 2; address <= 367; address++) {
      if (EEPROM.read(address) == SPRINTS_END_BYTE && address != currentDayOfYear + 1) {
        Serial.print(F("Debug (sprint): Clear stray SPRINTS_END_BYTE signature from cell "));
        Serial.println(address);
        EEPROM.update(address, EMPTY_VALUE);
        delay(5);
      }
    }

    // Save end byte signature
    EEPROM.update(currentDayOfYear + 1, SPRINTS_END_BYTE);
    delay(5);

  }
}

// ---- Statistics Handling ----

// Calculate and collect statistics data
void calculateStatistics() {
  // Get current day of week
  Ds1302::DateTime now;
  rtc.getDateTime(&now);
  int startDow = (now.dow - (currentDayOfYear - 1) % 7 + 7) % 7;
  int currentYear = 2000 + now.year;

  getStatisticsByPeriod(currentDayOfYear - 365, currentDayOfYear, startDow, YEARLY, currentYear);
  getStatisticsByPeriod(currentDayOfYear - 30, currentDayOfYear, startDow, MONTHLY, currentYear);
  getStatisticsByPeriod(currentDayOfYear - 7, currentDayOfYear, startDow, WEEKLY, currentYear);
}

// Get statistics data by period
void getStatisticsByPeriod(int startDay, int endDay, int startDow, int period, uint16_t year) {
  float multiplier = (config.countingUnits == 1) ? 4.16 : 10;
  int totalSprints = 0, totalDays = 0, maximum = 0, minimum = 100;

  int daysInCurrentYear = isLeapYear(year) ? 366 : 365;
  int daysInPreviousYear = isLeapYear(year - 1) ? 366 : 365;

  // Process previous year if startDay is negative
  if (startDay < 1) {
    int previousYearStart = daysInPreviousYear + startDay;
    for (int i = previousYearStart; i <= daysInPreviousYear; i++) {
      // Previous year stored at 365+index
      uint8_t sprintCounter = EEPROM.read(i + 365);
      if (sprintCounter == EMPTY_VALUE) continue;

    // Skip days with zero sprintCounter (if configured)
    if (config.countingDoWs == NOT_0_DAYS && sprintCounter == 0) continue;

    // Calculate Day of Week (0 = Sunday, 6 = Saturday)
    int dow = (startDow + (i - 1)) % 7;

    // Skip specific days based on configuration
    if ((config.countingDoWs == FIVE_DAYS && (dow == 0 || dow == 6)) || // Skip weekends
        (config.countingDoWs == SIX_DAYS && dow == 0)) continue;        // Skip Sundays

      // Accumulate statistics
      totalSprints += sprintCounter;
      totalDays++;
      if (sprintCounter > maximum) maximum = sprintCounter;
      if (sprintCounter < minimum) minimum = sprintCounter;
    }
    // Reset to start of current year
    startDay = 1;
  }

  // Process the current year
  for (int i = startDay; i <= endDay; i++) {
    uint8_t sprintCounter = EEPROM.read(i);
    if (sprintCounter == EMPTY_VALUE) continue;

    // Skip days with zero sprintCounter (if configured)
    if (config.countingDoWs == NOT_0_DAYS && sprintCounter == 0) continue;

    // Calculate Day of Week (0 = Sunday, 6 = Saturday)
    int dow = (startDow + (i - 1)) % 7;

    // Skip specific days based on configuration
    if ((config.countingDoWs == FIVE_DAYS && (dow == 0 || dow == 6)) || // Skip weekends
        (config.countingDoWs == SIX_DAYS && dow == 0)) continue;        // Skip Sundays

    // Accumulate statistics
    totalSprints += sprintCounter;
    totalDays++;
    if (sprintCounter > maximum) maximum = sprintCounter;
    if (sprintCounter < minimum) minimum = sprintCounter;
  }

  // Store statistics if we have any data
  if (totalDays > 0) {
    statisticsArray[TOTAL][period]   = multiplier * totalSprints;
    statisticsArray[MINIMUM][period] = multiplier * minimum;
    statisticsArray[AVERAGE][period] = multiplier * ((float)totalSprints / totalDays);
    statisticsArray[MAXIMUM][period] = multiplier * maximum;
  } else {
    statisticsArray[TOTAL][period] = 0;
    statisticsArray[MINIMUM][period] = 0;
    statisticsArray[AVERAGE][period] = 0;
    statisticsArray[MAXIMUM][period] = 0;
  }
}

// Clear statistics cells
void clearStatistics() {
  Serial.println("EEPROM: Clearing all statistics");
  for (uint16_t i = 0; i < 367; i++) {
    if (EEPROM.read(i) != EMPTY_VALUE) {
      Serial.print("EEPROM: Clearing cell: ");
      Serial.println(i);
      EEPROM.update(i, EMPTY_VALUE);
      delay(5);
    }
  }

  operatingState = STANDBY;

  // Return to main menu
  mainMenuMode = SPRINT;

  statisticsMenuMode = DAILY;
  state.inSubmenu = false;
}

// ---- Submenus Handling ----

// Sprint submenu
void subMenuSprint(byte btn) {
  if (!showStaticInfo) {

    runAnimation(3000);
    return;
  }

  if (!state.isSprintPaused) {
    // Show initial interval
    MFS.write("25.00");
  } else {
    updateLeds();
    operatingState = PAUSED;
    lastPausedBeep = millis();
  }

  if (btn == BUTTON_3_SHORT_RELEASE) {
    // Set Counting state to start new sprint
    operatingState = COUNTING;

    if (config.buzzEnabled) {
      MFS.beep(5, 5, 3);
    }

    if (!state.isSprintPaused) {
      setInterval(25);
    } else {
      state.isSprintPaused = false;
    }
  } else if (btn == BUTTON_1_SHORT_RELEASE) {
    turnOffBlinkAndLeds();

    // Return to main menu
    state.inSubmenu = false;
  }
}

// Statistics submenu
void subMenuStatistics(byte btn) {
  char formattedString[8];
  // Hours or runs (sprints)
  char countingUnit = (config.countingUnits == 1) ? 'h' : 'r';

  if (!state.inStatisticsSubmenu) {
    // Statistics menu navigation
    strcpy_P(formattedString, statisticsMenuItems[statisticsMenuMode]);
    MFS.write(formattedString);

    if (btn == BUTTON_2_SHORT_RELEASE) {
      // Cycle to next menu item
      statisticsMenuMode = (statisticsMenuMode + 1) % SUBMENU_STATISTICS_MODES_COUNT;
    }

    if (statisticsMenuMode == CLEAR && btn == BUTTON_3_SHORT_RELEASE) {
      blinkAlert(alertClearStatistics, 4, 250);
      operatingState = CLEARING;
    } else if (statisticsMenuMode != CLEAR && btn == BUTTON_3_SHORT_RELEASE) {
      // Submenu navigation flag
      state.inStatisticsSubmenu = true;
    } else if (btn == BUTTON_1_SHORT_RELEASE) {
      // Return to main menu
      state.inSubmenu = false;
      statisticsMenuMode = DAILY;
    }
  } else {
    // Statistics submenu navigation
    if (statisticsMenuMode == DAILY) {
      // Daily sprints
      runAnimation(0);
    } else if (statisticsMenuMode == TOTAL || statisticsMenuMode == MINIMUM || statisticsMenuMode == AVERAGE || statisticsMenuMode == MAXIMUM) {
      if (!state.inStatisticsDetailsSubmenu) {
        strcpy_P(formattedString, statisticsPeriods[statisticsPeriod]);
        MFS.write(formattedString);

        if (btn == BUTTON_2_SHORT_RELEASE) {
          // Cycle to next period item
          statisticsPeriod = (statisticsPeriod + 1) % STATISTICS_PERIODS_COUNT;
        } else if (btn == BUTTON_3_SHORT_RELEASE) {
          // Statistics details submenu navigation flag
          state.inStatisticsDetailsSubmenu = true;
        }
      } else {
        float value = statisticsArray[statisticsMenuMode][statisticsPeriod] / 10.0;
        if (statisticsMenuMode == TOTAL) {
          snprintf(formattedString, sizeof(formattedString), "%3d%c", (int)value, countingUnit);
        } else {
          // Cant use default sprintf here because of float
          // Need to convert float to string to combine with unit
          dtostrf(value, 4, 1, formattedString);
          snprintf(formattedString, sizeof(formattedString), "%4s%c", formattedString, countingUnit);
        }

        MFS.write(formattedString);
      }
    }

    if (btn == BUTTON_1_SHORT_RELEASE) {
      if (!state.inStatisticsDetailsSubmenu) {
        // Return to main menu
        state.inStatisticsSubmenu = false;
        statisticsPeriod = YEARLY;
      } else {
        // Return to prev menu
        state.inStatisticsDetailsSubmenu = false;
      }
    }
  }
}

// Config submenu
void subMenuConfig(byte btn) {
  char formattedString[8];
  if (!state.inConfigSubmenu) {
    // Config menu navigation
    strcpy_P(formattedString, configMenuItems[configMenuMode]);
    MFS.write(formattedString);

    if (btn == BUTTON_2_SHORT_RELEASE) {
      // Cycle to next menu item
      configMenuMode = (configMenuMode + 1) % SUBMENU_CONFIG_MODES_COUNT;
    }

    // Skip Tune and Cue menu when disabled buzzer
    if (configMenuMode == TUNE && !config.buzzEnabled) {
      configMenuMode = (configMenuMode + 2);
    }

    if (configMenuMode == SLEEP && btn == BUTTON_3_SHORT_RELEASE) {
      // Set sleeping state
      operatingState = SLEEPING;
    } else if (configMenuMode != SLEEP && btn == BUTTON_3_SHORT_RELEASE) {
      // Submenu navigation flag
      state.inConfigSubmenu = true;
    } else if (btn == BUTTON_1_SHORT_RELEASE) {
      // Return to main menu
      state.inSubmenu = false;
      configMenuMode = LCD;
    }
  } else {
    // Config submenu navigation
    if (configMenuMode == LCD) {
      // Display settings
      // Switch from min to max brightness
      switch (config.displayBrightness) {
        case 0:
          MFS.write("brt.4");
          MFS.writeLeds(LED_ALL, ON);
          break;

        case 1:
          MFS.write("brt.3");
          MFS.writeLeds(LED_ALL, OFF);
          MFS.writeLeds((LED_4 | LED_3 | LED_2), ON);
          break;

        case 2:
          MFS.write("brt.2");
          MFS.writeLeds(LED_ALL, OFF);
          MFS.writeLeds((LED_4 | LED_3), ON);
          break;

        case 3:
          MFS.write("brt.1");
          MFS.writeLeds(LED_ALL, OFF);
          MFS.writeLeds(LED_4, ON);
          break;

        case 4:
          if (state.isDisplayEnabled) {
            MFS.write("off");
            MFS.writeLeds(LED_ALL, OFF);            
          } else {
            MFS.write("");
          }
          break;
      }

      if (state.isDisplayEnabled) {
        if (btn == BUTTON_2_SHORT_RELEASE) {
          // Cycle to next brightness level
          config.displayBrightness = (config.displayBrightness + 4) % 5;
          MFS.setDisplayBrightness(config.displayBrightness);
        } else if (btn == BUTTON_3_SHORT_RELEASE || btn == BUTTON_1_SHORT_RELEASE) {
          // Turn LEDs OFF
          MFS.writeLeds(LED_ALL, OFF);

          if (btn == BUTTON_3_SHORT_RELEASE) {
            if (config.displayBrightness < 4) {
              // Save new value if it differs from stored before
              saveConfig();
            } else {
              // Turn off the display
              state.isDisplayEnabled = false;
            }
          } else if (btn == BUTTON_1_SHORT_RELEASE) {
            // Leave config value unchanged
            loadConfig();
            MFS.setDisplayBrightness(config.displayBrightness);
          }

          if (state.isDisplayEnabled) {
            // Return to Config menu
            state.inConfigSubmenu = false;
          }
        }
      } else {
        if (btn == BUTTON_1_SHORT_RELEASE || btn == BUTTON_2_SHORT_RELEASE || btn == BUTTON_3_SHORT_RELEASE) {
          state.isDisplayEnabled = true;
          config.displayBrightness = 3;

          // Return to Config menu
          state.inConfigSubmenu = false;
        }
      }
    } else if (configMenuMode == BUZZ) {
      // Buzz setting
      if (config.buzzEnabled) {
        MFS.write("on");
      } else {
        MFS.write("off");
      }

      if (btn == BUTTON_2_SHORT_RELEASE) {
        // Switch Buzz config value
        config.buzzEnabled = !config.buzzEnabled;
      } else if (btn == BUTTON_3_SHORT_RELEASE || btn == BUTTON_1_SHORT_RELEASE) {
        // Save or cancel
        toggleConfig(btn);
      }
    } else if (configMenuMode == TUNE) {
      // Tune setting
      if (config.tune > 0 && config.tune < 9) {
        sprintf(formattedString, "tn.%02d", config.tune);
        MFS.write(formattedString);
      } else if (config.tune == 9) {
        MFS.write("rnd");
      } else {
        MFS.write("off");
      }

      if (btn == BUTTON_2_SHORT_RELEASE) {
        // Cycle to next tune
        config.tune = (config.tune + 1) % 10;
      } else if (btn == BUTTON_3_SHORT_RELEASE || btn == BUTTON_1_SHORT_RELEASE) {
        if (btn == BUTTON_3_SHORT_RELEASE && config.tune > 0 && config.tune < 9) {
          playMelody(config.tune);
        }

        // Save or cancel
        toggleConfig(btn);
      }
    } else if (configMenuMode == CUE) {
      // Cue setting
      if (config.cueEnabled) {
        MFS.write("on");
      } else {
        MFS.write("off");
      }

      if (btn == BUTTON_2_SHORT_RELEASE) {
        // Switch Cue config value
        config.cueEnabled = !config.cueEnabled;
      } else if (btn == BUTTON_3_SHORT_RELEASE || btn == BUTTON_1_SHORT_RELEASE) {
        // Save or cancel
        toggleConfig(btn);
      }
    } else if (configMenuMode == LEDS) {
      // LEDs setting
      if (config.ledsEnabled) {
        MFS.write("on");
        MFS.writeLeds(LED_ALL, ON);
      } else {
        MFS.write("off");
        MFS.writeLeds(LED_ALL, OFF);
      }

      if (btn == BUTTON_2_SHORT_RELEASE) {
        // Switch LEDs config value
        config.ledsEnabled = !config.ledsEnabled;
      } else if (btn == BUTTON_3_SHORT_RELEASE || btn == BUTTON_1_SHORT_RELEASE) {
        // Turn LEDs OFF
        MFS.writeLeds(LED_ALL, OFF);

        // Save or cancel
        toggleConfig(btn);
      }
    } else if (configMenuMode == COUNTED_DOW) {
      // Counting days setting 
      strcpy_P(formattedString, countedDoWItems[config.countingDoWs]);
      MFS.write(formattedString);

      // Switch mode
      if (btn == BUTTON_2_SHORT_RELEASE) {
        // Cycle to next setting mode
        config.countingDoWs = (config.countingDoWs + 1) % COUNTED_DOW_COUNT;
      } else if (btn == BUTTON_3_SHORT_RELEASE || btn == BUTTON_1_SHORT_RELEASE) {
        // Save or cancel
        toggleConfig(btn);
      }
    } else if (configMenuMode == COUNT_UNITS) {
      // Counting units setting 
      if (config.countingUnits == 0) {
        MFS.write("run");
      } else {
        MFS.write("hour");
      }

      // Switch mode
      if (btn == BUTTON_2_SHORT_RELEASE) {
        // Switch between different units
        config.countingUnits = !config.countingUnits;
      } else if (btn == BUTTON_3_SHORT_RELEASE || btn == BUTTON_1_SHORT_RELEASE) {
        // Save or cancel
        toggleConfig(btn);
      }
    } else if (configMenuMode == HERO) {
      // Hero mode setting
      if (config.heroModeEnabled) {
        MFS.write("on");
      } else {
        MFS.write("off");
      }

      if (btn == BUTTON_2_SHORT_RELEASE) {
        // Switch Hero mode config value
        config.heroModeEnabled = !config.heroModeEnabled;
      } else if (btn == BUTTON_3_SHORT_RELEASE || btn == BUTTON_1_SHORT_RELEASE) {
        // Save or cancel
        toggleConfig(btn);
      }
    } else if (configMenuMode == BUILD) {
      // Build version
      int versionMajor = VERSION / 100;
      int versionMinor = (VERSION / 10) % 10;
      int versionPatch = VERSION % 10;

      snprintf(formattedString, sizeof(formattedString), "v.%d.%d.%d", versionMajor, versionMinor, versionPatch);
      MFS.write(formattedString);

      if (btn == BUTTON_3_SHORT_RELEASE || btn == BUTTON_1_SHORT_RELEASE) {
        // Save or cancel
        toggleConfig(btn);
      }
    }
  }
}

// Toggle config value inside menu
void toggleConfig(byte btn) {
  if (btn == BUTTON_3_SHORT_RELEASE) {
    // Save changes
    saveConfig();
  } else if (btn == BUTTON_1_SHORT_RELEASE) {
    // Revert changes
    loadConfig();
  }

  // Return to config menu
  state.inConfigSubmenu = false;
}

// Date and Time submenu
void subMenuClock(byte btn) {
  char formattedString[8];
  // Show current time
  Ds1302::DateTime now;
  rtc.getDateTime(&now);

  switch (dateTimeMode) {
    case HOURS_MINUTES:
      sprintf(formattedString, "%02d.%02d", now.hour, now.minute);
      break;
    case DAY_MONTH:
      sprintf(formattedString, "%02d.%02d", now.day, now.month);
      break;
    case YEAR:
      sprintf(formattedString, "20%02d", now.year);
      break;
  }
  MFS.write(formattedString);

  if (btn == BUTTON_1_SHORT_RELEASE) {
    // Return to main menu
    state.inSubmenu = false;
    dateTimeMode = HOURS_MINUTES;
  } else if (btn == BUTTON_2_SHORT_RELEASE) {
    // Switch mode
    dateTimeMode = (dateTimeMode + 1) % 3;
  } else if (btn == BUTTON_3_SHORT_RELEASE) {
    // Switch operating state
    operatingState = DATETIMESETUP;
  }
}

// ---- Sleep Mode ----

// Interrupt Service Routine (ISR) to wake up
void wakeUpISR() {
  Serial.println(F("Debug (sleep): Waking up"));
  // Update sleep state
  state.isSleeping = false;
}

// Sleep mode with reduced power consumption
void sleep() {
  Serial.println(F("Debug (sleep): Preparing to sleep"));
  Serial.flush();
  state.isSleeping = true;

  // Wake up on falling edge (button press)
  attachInterrupt(digitalPinToInterrupt(2), wakeUpISR, FALLING);

  // Set sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  // Enter sleep mode, waiting for interrupt
  sleep_mode();

  // After waking up, disable sleep mode
  sleep_disable();

  detachInterrupt(digitalPinToInterrupt(2));

  while (digitalRead(2) == LOW) {
    Serial.println(F("Debug (sleep): Waiting for pin 2 to be released"));
    delay(500);
  }

  Serial.println(F("Debug (sleep): Device is awake"));
  operatingState = STANDBY;
  state.isSleeping = false;
}

// ---- Animation Handling ----

// Function to call once when entering the menu
void startAnimation() {
  animationStartTime     = millis();
  animationPrevFrameTime = animationStartTime;
  animationIndex     = 0;
  showStaticInfo = false;

  lastSprintToggleTime = 0;
  showSprint = false;
}

// Function to call every loop to drive the animation
void runAnimation(unsigned long duration) {
  unsigned long now = millis();
  char formattedString[8];

  // If within the animation window
  if (duration == 0 || now - animationStartTime < duration) {
    // Draw one frame on each 100 ms tick
    if (now - animationPrevFrameTime >= 100) {
      animationPrevFrameTime = now;

      // Alternate display every second: even seconds = counter, odd = hours
      if (now - lastSprintToggleTime >= 1500) {
        lastSprintToggleTime = now;
        showSprint = !showSprint;
      }

      if (showSprint) {
        // Show sprint counter
        sprintf(formattedString, "%s %02d", animatedCharacters[animationIndex], sprintCounter);
      } else {
        // Show animated hours as float with 'h'
        float hours = sprintCounter * 25.0 / 60.0;

        if (hours >= 10.0) {
          // Show rounded integer
          int roundedHours = (int)hours;
          snprintf(formattedString, sizeof(formattedString), "%s%2dh", animatedCharacters[animationIndex], roundedHours);
        } else {
          // Show one decimal place
          char hoursStr[4];
          dtostrf(hours, 3, 1, hoursStr);
          snprintf(formattedString, sizeof(formattedString), "%s%3sh", animatedCharacters[animationIndex], hoursStr);
        }
      }

      MFS.write(formattedString);
      if (++animationIndex >= animatedCharactersSize) animationIndex = 0;
    }
  } else {
    // Time is up
    showStaticInfo = true;
  }
}

// Blink text alert
void blinkAlert(const char messages[][6], uint8_t blinkCount, uint16_t blinkDelay) {
  char formattedString[8];
  uint8_t i = 0;

  while (true) {
    // Read the message from PROGMEM
    strcpy_P(formattedString, messages[i]);

    // Check if we reached the end (empty string)
    if (formattedString[0] == '\0') break;

    // Blink the message
    for (uint8_t j = 0; j < blinkCount; j++) {
      MFS.write(formattedString);
      MFS.writeLeds(LED_ALL, (j % 2 == 0) ? ON : OFF);
      delay(blinkDelay);
    }

    // Move to the next message
    i++;
  }

  turnOffBlinkAndLeds();
}

// ---- LEDs Handling ----

// Update LEDs state
void updateLeds() {
  byte ledMask      = LED_ALL;
  byte ledBlinkMask = LED_ALL;
  int ledBlinkValue = OFF;

  switch (sprintCounter % 4) {
    // If nothing completed or completed 4th, 8th, etc. sprint
    case 0:
      if (currentInterval == 15) {
        // If long break, no need for blinking
        ledMask = (LED_1 | LED_2 | LED_3 | LED_4);
        ledBlinkValue = OFF;
      } else {
        // Usual interval with current sprint blinking
        ledMask = LED_1;
        ledBlinkMask = LED_1;
        ledBlinkValue = ON;
      }
      break;

    // If completed 1st, 5th, 9th, etc. sprint
    case 1:
      if (currentInterval == 5) {
        // If short break interval, no need for blinking
        ledMask = LED_1;
        ledBlinkValue = OFF;
      } else {
        // Usual interval with current sprint blinking
        ledMask = (LED_1 | LED_2);
        ledBlinkMask = LED_2;
        ledBlinkValue = ON;
      }
      break;

    // If completed 2nd, 6th, 10th, etc. sprint
    case 2:
      if (currentInterval == 5) {
        // If short break interval, no need for blinking
        ledMask = (LED_1 | LED_2);
        ledBlinkValue = OFF;
      } else {
        // Usual interval with current sprint blinking
        ledMask = (LED_1 | LED_2 | LED_3);
        ledBlinkMask = LED_3;
        ledBlinkValue = ON;
      }
      break;

    // If completed 3nd, 7th, 11th, etc. sprint
    case 3:
      if (currentInterval == 5) {
        // If short break interval, no need for blinking
        ledMask = (LED_1 | LED_2 | LED_3);
        ledBlinkValue = OFF;
      } else {
        // Usual interval with current sprint blinking
        ledMask = (LED_1 | LED_2 | LED_3 | LED_4);
        ledBlinkMask = LED_4;
        ledBlinkValue = ON;
      }
      break;
  }

  // Turn OFF all LEDs and turn ON according to masks
  MFS.writeLeds(LED_ALL, OFF);
  MFS.writeLeds(ledMask, ON);
  MFS.blinkLeds(ledBlinkMask, ledBlinkValue);
}

// Disable LEDs
void turnOffBlinkAndLeds() {
  MFS.writeLeds(LED_ALL, OFF);
  MFS.blinkLeds(LED_ALL, OFF);
  MFS.blinkDisplay(DIGIT_ALL, OFF);
}

// ---- Melodies Handling ----

// Notes and their frequencies
const int NOTE_A3 = 220, NOTE_AS3 = 233, NOTE_B3 = 247, NOTE_E3 = 165, NOTE_F3 = 175, NOTE_FS3 = 185, NOTE_G3 = 196, NOTE_GS3 = 208,
          NOTE_AS4 = 466, NOTE_B4 = 494, NOTE_C4 = 262, NOTE_CS4 = 277, NOTE_D4 = 294, NOTE_DS4 = 311, NOTE_E4 = 330, NOTE_F4 = 349, NOTE_FS4 = 370, NOTE_G4 = 392, NOTE_GS4 = 415, NOTE_A4 = 440,
          NOTE_C5 = 523, NOTE_D5 = 587, NOTE_E5 = 659, REST = 0;

// Henry Mancini - Baby Elephant Walk (intro)
const int melody1[] PROGMEM = {
  NOTE_C4, -8,
  NOTE_E4, 16,
  NOTE_G4, 8,
  NOTE_C5, 8,
  NOTE_E5, 8,
  NOTE_D5, 8,
  NOTE_C5, 8,
  NOTE_A4, 8,
  NOTE_FS4, 8,
  NOTE_G4, 8
};

// Henry Mancini - Baby Elephant Walk (outro)
const int melody2[] PROGMEM = {
  NOTE_C5, 4,
  NOTE_C5, 4,
  NOTE_AS4, 16,
  NOTE_C5, 16,
  NOTE_AS4, 16,
  NOTE_G4, 16,
  NOTE_F4, 8,
  NOTE_DS4, 8,
  NOTE_FS4, 4,
  NOTE_FS4, 4,
  NOTE_F4, 16,
  NOTE_G4, 16,
  NOTE_F4, 16,
  NOTE_DS4, 16,
  NOTE_C4, 8,
  NOTE_G4, 8,
  NOTE_AS4, 8,
  NOTE_C5, 8,
};

// Tetris
const int melody3[] PROGMEM = {
  NOTE_E5, 4,
  NOTE_B4, 8,
  NOTE_C5, 8,
  NOTE_D5, 4,
  NOTE_C5, 8,
  NOTE_B4, 8,
  NOTE_A4, 4
};

// Elderbrook & Vintage Culture - Run
const int melody4[] PROGMEM = {
  NOTE_AS4, 16,
  NOTE_AS4, 16, REST, 16,
  NOTE_AS4, 16, REST, 16,
  NOTE_GS4, 16, REST, 16,
  NOTE_GS4, 16, REST, 16,
  NOTE_GS4, 16, REST, 16,
  NOTE_GS4, 16,
  NOTE_AS4, 16, REST, 16,
  NOTE_G4, 16, REST, 16,

  NOTE_DS4, 16,
  NOTE_DS4, 16, REST, 16,
  NOTE_DS4, 16, REST, 16,
  NOTE_DS4, 16, REST, 16,
  NOTE_DS4, 16, REST, 16,
  NOTE_DS4, 16, REST, 16,
  NOTE_DS4, 16,
  NOTE_F4, 16, REST, 16,
  NOTE_D4, 16, REST, 16
};

// K.Flay - Not In California
const int melody5[] PROGMEM = {
  NOTE_GS4, 6,
  NOTE_FS4, 6,
  NOTE_GS4, 6,
  NOTE_FS4, 6,
  NOTE_E4, 6,
  NOTE_B3, 3,
  NOTE_E4, 2,
  NOTE_E4, 6,
  NOTE_DS4, 6,
  NOTE_CS4, 3
};

// Lana Del Ray - Summertime Sadness
const int melody6[] PROGMEM = {
  NOTE_CS4, 10, REST, 15,
  NOTE_CS4, 10, REST, 15,
  NOTE_CS4, 10, REST, 15,
  NOTE_E4, 6,
  NOTE_FS4, 6,
  NOTE_E4, 3,
  NOTE_CS4, 10, REST, 15,
  NOTE_CS4, 10, REST, 15,
  NOTE_CS4, 4, REST, 15,
  NOTE_FS4, 3,
  NOTE_E4, 3
};

// Coldplay - Clocks
const int melody7[] PROGMEM = {
  NOTE_DS4, 10,
  NOTE_AS3, 10,
  NOTE_G3, 10,
  NOTE_DS4, 10,
  NOTE_AS3, 10,
  NOTE_G3, 10,
  NOTE_DS4, 10,
  NOTE_AS3, 10,
  NOTE_CS4, 10,
  NOTE_AS3, 10,
  NOTE_F3, 10,
  NOTE_CS4, 10,
  NOTE_AS3, 10,
  NOTE_F3, 10,
  NOTE_CS4, 10,
  NOTE_AS3, 10
};

// Metallica - Master Of Puppets
const int melody8[] PROGMEM = {
  NOTE_E3, 12,
  NOTE_E3, 12,
  NOTE_E4, 12,
  NOTE_E3, 12,
  NOTE_E3, 12,
  NOTE_DS4, 12,
  NOTE_E3, 12,
  NOTE_E3, 12,
  NOTE_D4, 6,
  NOTE_CS4, 6,
  NOTE_C4, 3,
  
  NOTE_E3, 12,
  NOTE_E3, 12,
  NOTE_B3, 12,
  NOTE_E3, 12,
  NOTE_E3, 12,
  NOTE_AS3, 12,
  NOTE_E3, 12,
  NOTE_E3, 12,
  NOTE_A3, 12,
  NOTE_E3, 12,
  NOTE_GS3, 12,
  NOTE_E3, 12,
  NOTE_G3, 12,
  NOTE_E3, 12,
  NOTE_FS3, 12,
  NOTE_E3, 12,
};

const int* const melodies[] PROGMEM = {
  melody1,
  melody2,
  melody3,
  melody4,
  melody5,
  melody6,
  melody7,
  melody8
};

const uint16_t melodyLengths[] PROGMEM = {
  sizeof(melody1) / sizeof(melody1[0]),
  sizeof(melody2) / sizeof(melody2[0]),
  sizeof(melody3) / sizeof(melody3[0]),
  sizeof(melody4) / sizeof(melody4[0]),
  sizeof(melody5) / sizeof(melody5[0]),
  sizeof(melody6) / sizeof(melody6[0]),
  sizeof(melody7) / sizeof(melody7[0]),
  sizeof(melody8) / sizeof(melody8[0]),
};

// Play melody with disco lights
void playMelody(byte melodyId) {
  if (melodyId == 9) {
    // Randomize melodies
    melodyId = random(1, 9);
  }
  melodyId--;

  // Get pointer to melody array
  const int* melody;
  memcpy_P(&melody, &melodies[melodyId], sizeof(melody));

  // Get length
  uint16_t length;
  memcpy_P(&length, &melodyLengths[melodyId], sizeof(length));

  const int wholenote = (60000 * 4) / 128;
  uint8_t ledIndex = 1;
  int direction = 1;

  for (int i = 0; i < length; i += 2) {
    int note = pgm_read_word_near(melody + i);
    int divider = pgm_read_word_near(melody + i + 1);
    int duration = (divider > 0) ? (wholenote / divider) : (wholenote / abs(divider)) * 1.25;

    // Turn off all LEDs and turn on the current one
    MFS.writeLeds(LED_ALL, OFF);
    // Shift bit to control specific LED
    MFS.writeLeds(1 << (ledIndex - 1), ON);

    if (note != REST) {
      tone(buzzerPin, note, duration);

      // Update LED index with ping-pong effect
      ledIndex += direction;
      if (ledIndex == 4 || ledIndex == 1) {
        // Change direction
        direction *= -1;
      }
    } else {
      MFS.beep(0, 0, 0);
    }

    delay(duration);
    noTone(buzzerPin);
    MFS.beep(0, 0, 0);
  }

  // Turn off all LEDs at the end
  MFS.writeLeds(LED_ALL, OFF);
  MFS.beep(0, 0, 0);
}

