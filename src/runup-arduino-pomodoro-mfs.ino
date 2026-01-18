// -----------------------------------------------------------------
// Project:  runUp - Arduino Pomodoro Timer
// Hardware: Arduino UNO R3 (or compatible), Multifunction Shield,
//           DS1302 RTC module
// Author:   https://github.com/beschasny
// License:  MIT (see LICENSE file in repository)
// -----------------------------------------------------------------
//
// Description:
//   Pomodoro timer for Arduino Uno R3 using a Multifunction
//   Shield (buttons, display, buzzer) and a DS1302 RTC module.
//   Features include managing Pomodoro sprint sessions,
//   statistics tracking, customizable settings, and more.
//
// Versioning:
//   For version history, see Git tags/releases on GitHub.
// -----------------------------------------------------------------

// -----------------------------------------------------------------
// Includes
// -----------------------------------------------------------------

// Multifunction Shield library (modified)
// https://github.com/beschasny/MultiFuncShield-Library
#include <MultiFuncShield.h>

// Arduino Library for RTCs
// https://github.com/Makuna/Rtc
#include <RtcDS1302.h>

// Arduino built-in libraries
#include <EEPROM.h>
#include <avr/sleep.h>

// -----------------------------------------------------------------
// Pinouts
// -----------------------------------------------------------------

// DS1302 RTC module
const byte rtcRstPin                          = 5;
const byte rtcClkPin                          = 9;
const byte rtcDatPin                          = 6;

ThreeWire myWire(rtcDatPin, rtcClkPin, rtcRstPin);
RtcDS1302<ThreeWire> Rtc(myWire);

// Buzzer
const byte buzzerPin                          = 3;

// Interruption
const byte isrPin                             = 2;

// -----------------------------------------------------------------
// Enumeration types and related variables
// -----------------------------------------------------------------

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
  bool inStatisticsDailySubmenu               : 1;
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
  NOW,
  DAILY,
  TOTAL,
  MINIMUM,
  AVERAGE,
  MAXIMUM,
  CLEAR,
  SUBMENU_STATISTICS_MODES_COUNT
};
enum StatisticsPeriods {
  ROLL365D,
  ROLL100D,
  ROLL60D,
  ROLL30D,
  ROLL7D,
  MONTH01,
  MONTH02,
  MONTH03,
  MONTH04,
  MONTH05,
  MONTH06,
  MONTH07,
  MONTH08,
  MONTH09,
  MONTH10,
  MONTH11,
  MONTH12,
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
  INFO,
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
byte statisticsMenuMode                       = NOW;
byte statisticsPeriod                         = ROLL365D;
byte dateTimeMode                             = HOURS_MINUTES;

// -----------------------------------------------------------------
// String constants
// -----------------------------------------------------------------

// Menu items
// Use special characters to display letter combinations
const char mainMenuItems[][6] PROGMEM         = {
  "run>", // Run sprint
  "data", // Statistics data
  "conf", // Configuration settings
  "cloc"  // Clock and calendar
};
const char statisticsMenuItems[][6] PROGMEM   = {
  "no#i", // Today
  "da{u", // Daily statistics
  "totl", // Total sprints
  "#iin", // Minimum
  "#iid", // Middle (Average, mean value)
  "#iax", // Maximum
  "clr"   // Clear all statistics data
};
const char statisticsPeriods[][6] PROGMEM     = {
  "365d", // 365 days
  "100d", // 100 days
  " 60d", // 60 days
  " 30d", // 30 days
  "  7d", // 7 days
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
  "bu{d",  // Build version
  "info"   // Author information
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

// -----------------------------------------------------------------
// Global variables and configuration
// -----------------------------------------------------------------

// Countdown and statistics
byte currentInterval                          = 0;
byte tenths                                   = 0;
char seconds                                  = 0;
char minutes                                  = 0;
uint8_t sprintCounter                         = 0;
uint8_t currentDayOfWeek                      = 1;
uint16_t currentDayOfYear                     = 1;
uint8_t currentMonth                          = 1;
uint16_t inputDayOfYear                       = 1;
uint16_t currentYear                          = 2025;

// Using int with x10 multiplier to simulate float and reduce memory usage
int statisticsArray[SUBMENU_STATISTICS_MODES_COUNT - 1][STATISTICS_PERIODS_COUNT];

// Configs
#define VERSION                               120
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

// Forward declaration of melodies size
extern const uint8_t melodiesSize;

// -----------------------------------------------------------------
// Setup
// -----------------------------------------------------------------

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
  state.inStatisticsDailySubmenu   = false;
  state.inStatisticsDetailsSubmenu = false;
  state.inConfigSubmenu            = false;

  // Initialize RTC
  Rtc.Begin();

  // Check and get date and time (if not first run)
  if (operatingState != DATETIMESETUP) {
    if (!Rtc.IsDateTimeValid()) {
      Serial.println(F("Error (#01): RTC is halted"));
      errorCode = 1;
      operatingState = ERROR;
    } else {
      // Get current day of year to use as current EEPROM address,
      // also check date and time for sanity
      getCurrentDate();

      // Ensure tomorrow's cell is empty or can be safely overwritten
      validateNextDayCell();
    }
  }

  // If no errors check if it is needed to update sprintCounter and skipped days
  if (errorCode == 0 && operatingState == STANDBY) {
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
        RtcDateTime now = Rtc.GetDateTime();

        int currentYear = now.Year();
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

// -----------------------------------------------------------------
// Loop
// -----------------------------------------------------------------

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
            // Prepare to show today sprints
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

// -----------------------------------------------------------------
// Helper functions
// -----------------------------------------------------------------

// ---- Config Handling ----

// Load config from EEPROM or initialize with defaults
void loadConfig() {
  EEPROM.get(CONFIG_ADDRESS, config);

  // Check if first run or invalid config
  if (config.signature != CONFIG_SIGNATURE) {
    Serial.println(F("EEPROM: CONFIG_SIGNATURE not found, seems like first run"));

    initConfig();
    saveConfig();
    operatingState = DATETIMESETUP;
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

// Save config to EEPROM
void saveConfig() {
  Serial.println(F("EEPROM: Save config"));
  EEPROM.put(CONFIG_ADDRESS, config);
  delay(5);
}

// ---- Date and Time Operations ----

// Check if year is leap
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

// Get current date (DoY, DoW, month and year)
void getCurrentDate() {
  RtcDateTime now = Rtc.GetDateTime();
  Serial.println(F("Debug (RTC): Delay before getting current day of year"));
  delay(50);

  if (isRtcDateTimeSane(now)) {
    uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint16_t dayOfYear = now.Day();

    // Add days from previous months
    for (uint8_t i = 0; i < now.Month() - 1; i++) {
      dayOfYear += daysInMonth[i];
    }

    // Add +1 for leap year if month is after February
    if (now.Month() > 2 && isLeapYear(now.Year())) {
      dayOfYear++;
    }

    currentDayOfYear = dayOfYear;
    Serial.print(F("Debug (sprint): Current day of year: "));
    Serial.println(currentDayOfYear);

    uint8_t dow = now.DayOfWeek();           // 0=Sun
    currentDayOfWeek = (dow == 0) ? 7 : dow; // Sun -> 7

    Serial.print(F("Debug (sprint): Current DoW: "));
    Serial.println(currentDayOfWeek);

    currentMonth = now.Month();
    Serial.print(F("Debug (sprint): Current month: "));
    Serial.println(currentMonth);

    currentYear = now.Year();
    Serial.print(F("Debug (sprint): Current year: "));
    Serial.println(currentYear);
  } else {
    Serial.println(F("Error (#02): RTC isn't sane, invalid datetime"));
    errorCode = 2;
    operatingState = ERROR;
  }
}

// Get requested date from day of year
void getDateFromDayOfYear(uint16_t inputDayOfYear, char *formattedString) {
  // Determine year for requested day
  uint16_t year = (inputDayOfYear <= currentDayOfYear) ? currentYear : currentYear - 1;

  uint16_t day = inputDayOfYear;
  uint8_t month = 1;

  // Compute month and day
  while (day > getDaysInMonth(year, month)) {
      day -= getDaysInMonth(year, month);
      month++;
  }

  sprintf(formattedString, "%02u.%02u", day, month);
}

// Get day-of-year range for given month
void getMonthDayOfYearRange(uint16_t year, uint8_t month, int &startDay, int &endDay) {
    startDay = 1;
    for (uint8_t m = 1; m < month; m++) {
        startDay += getDaysInMonth(year, m);
    }

    endDay = startDay + getDaysInMonth(year, month) - 1;
}

// Get number of days in given month
uint8_t getDaysInMonth(uint16_t year, uint8_t month) {
    static const uint8_t dim[12] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (month == 2 && isLeapYear(year)) return 29;

    return dim[month - 1];
}

// Check RTC date and time for sanity
bool isRtcDateTimeSane(const RtcDateTime& now) {
  Serial.println(F("Debug (RTC): calling isRtcDateTimeSane()"));
  Serial.print(F("Debug (RTC): Now: "));
  Serial.print(now.Year());
  Serial.print(F("-"));
  Serial.print(now.Month());
  Serial.print(F("-"));
  Serial.print(now.Day());
  Serial.print(F(" "));
  Serial.print(now.Hour());
  Serial.print(F(":"));
  Serial.print(now.Minute());
  Serial.print(F(":"));
  Serial.println(now.Second());

  // Check year range
  if (now.Year() < 2025 || now.Year() > 2050) return false;

  // Check month
  if (now.Month() < 1 || now.Month() > 12) return false;

  // Check day of month
  if (now.Day() < 1 || now.Day() > getDaysInMonth(now.Year(), now.Month())) return false;

  // Check hours, minutes, seconds
  if (now.Hour() > 23 || now.Minute() > 59 || now.Second() > 59) return false;

  return true;
}

// Set new date and time
void setDateTime(byte btn) {
  // currentField: 0-9, each number corresponds to digit:
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
      case DAY_MONTH: currentField = 4;     break;
      case YEAR: currentField = 8;          break;
    }

    // Initialize digits from RTC
    RtcDateTime now = Rtc.GetDateTime();

    if (isRtcDateTimeSane(now)) {
      Serial.println(F("Debug (RTC): Sane, get current values"));

      hourTens = now.Hour() / 10;
      hourUnits = now.Hour() % 10;

      minuteTens = now.Minute() / 10;
      minuteUnits = now.Minute() % 10;

      dayTens = now.Day() / 10;
      dayUnits = now.Day() % 10;

      monthTens = now.Month() / 10;
      monthUnits = now.Month() % 10;

      yearTens  = (now.Year() % 100) / 10;
      yearUnits = (now.Year() % 100) % 10;
    } else {
      // Fallback default 12:30 31.12.25
      Serial.println(F("Debug (RTC): Not sane, set fallback values"));

      hourTens = 1;
      hourUnits = 2;

      minuteTens = 3;
      minuteUnits = 0;

      dayTens = 3;
      dayUnits = 1;

      monthTens = 1;
      monthUnits = 2;

      yearTens = 2;
      yearUnits = 5;
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

        uint8_t hour   = hourTens   * 10 + hourUnits;
        uint8_t minute = minuteTens * 10 + minuteUnits;
        uint8_t day    = dayTens    * 10 + dayUnits;
        uint8_t month  = monthTens  * 10 + monthUnits;
        uint16_t year  = 2000 + (yearTens * 10 + yearUnits);
        uint8_t second = 0;

        RtcDateTime dt(year, month, day, hour, minute, second);
        Rtc.SetDateTime(dt);
      }
      initialized = false;
      operatingState = STANDBY;
      errorCode = 0;

      // Update current date
      getCurrentDate();

      // Clear end byte signatures (if any)
      for (int address = 2; address <= 367; address++) {
        if (EEPROM.read(address) == SPRINTS_END_BYTE && address != currentDayOfYear + 1) {
          Serial.print(F("Debug (RTC): Clear stray SPRINTS_END_BYTE signature from cell "));
          Serial.println(address);

          EEPROM.update(address, EMPTY_VALUE);
        }
      }

      // Update sprintCounter
      if (EEPROM.read(currentDayOfYear) != EMPTY_VALUE) {
        sprintCounter = EEPROM.read(currentDayOfYear);
      } else {
        sprintCounter = 0;
      }

      // Save end byte signature
      EEPROM.update(currentDayOfYear + 1, SPRINTS_END_BYTE);

      MFS.blinkDisplay(DIGIT_ALL, OFF);
      return;

    case BUTTON_2_SHORT_RELEASE:
      // Increment current digit with clamping
      switch (currentField) {
        case 0: // Hour tens (0-2)
          hourTens = (hourTens + 1) % 3;
          // Clamp hourUnits max 3 if tens=2
          if (hourTens == 2 && hourUnits > 3) hourUnits = 3;

          break;
        case 1: // Hour units (0-9 or 0-3 if tens=2)
          hourUnits = (hourUnits + 1) % 10;
          if (hourTens == 2 && hourUnits > 3) hourUnits = 0;

          break;
        case 2: // Minute tens (0-5)
          minuteTens = (minuteTens + 1) % 6;

          break;
        case 3: // Minute units (0-9)
          minuteUnits = (minuteUnits + 1) % 10;

          break;
        case 4: // Day tens (0-3)
          dayTens = (dayTens + 1) % 4;

          // Clamp dayUnits based on dayTens (max 31 days)
          if (dayTens == 3 && dayUnits > 1) dayUnits = 1;

          break;
        case 5: // Day units (0-9, max 1 if tens=3)
          dayUnits = (dayUnits + 1) % 10;

          // Clamp based on tens
          if (dayTens == 0 && dayUnits == 0) dayUnits = 1; // 01..09
          if (dayTens == 3 && dayUnits > 1) dayUnits = 0;  // 30, 31

          break;
        case 6: // Month tens (0-1)
          monthTens = (monthTens + 1) % 2;
          // Clamp month max 12
          if (monthTens == 1 && monthUnits > 2) monthUnits = 2;
 
          break;
        case 7: // Month units (0-9, max 2 if tens=1)
          monthUnits = (monthUnits + 1) % 10;

          // Clamp based on tens
          if (monthTens == 0 && monthUnits == 0) monthUnits = 1; // 01..09
          if (monthTens == 1 && monthUnits > 2) monthUnits = 0;  // 10, 11, 12

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

  // Blink only current digit
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

// Ensure tomorrow's cell is empty or can be safely overwritten
void validateNextDayCell() {
  // Look ahead to check if cycle is complete or data invalid
  byte nextValue = EEPROM.read(currentDayOfYear + 2);

  if (errorCode == 0 && nextValue != EMPTY_VALUE) {
    if (nextValue >= 0 && nextValue < 58) {
      Serial.println(F("Debug (sprint): Cycle is complete, going to overwrite data"));
    } else {
      Serial.println(F("Error (#03): Already existed non-standard record (data not cleared)"));
      operatingState = ERROR;
      errorCode = 3;
    }
  }
}

// Save current sprint counter according to current day of year
void saveSprint() {
  uint16_t initialDayOfYear = currentDayOfYear;
  // Update current date
  getCurrentDate();

  // Ensure tomorrow's cell is empty or can be safely overwritten
  validateNextDayCell();

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
  // Get current date
  getCurrentDate();

  int startDow = (currentDayOfWeek - (currentDayOfYear - 1) % 7 + 7) % 7;
  inputDayOfYear = currentDayOfYear;

  int endDay = currentDayOfYear - 1;
  if (endDay < 1) endDay = 1;

  // Rolling intervals (365, 100, 60, 30, 7 days)
  getStatisticsByPeriod(currentDayOfYear - 365, endDay, startDow, ROLL365D, currentYear);
  getStatisticsByPeriod(currentDayOfYear - 100, endDay, startDow, ROLL100D, currentYear);
  getStatisticsByPeriod(currentDayOfYear - 60, endDay, startDow, ROLL60D, currentYear);
  getStatisticsByPeriod(currentDayOfYear - 30, endDay, startDow, ROLL30D, currentYear);
  getStatisticsByPeriod(currentDayOfYear - 7, endDay, startDow, ROLL7D, currentYear);

  // Monthly intervals
  for (uint8_t month = 1; month <= 12; month++) {
    int startDayMonth, endDayMonth;

    getMonthDayOfYearRange(currentYear, month, startDayMonth, endDayMonth);
    getStatisticsByPeriod(startDayMonth, endDayMonth, startDow, MONTH01 + (month - 1), currentYear);
  }
}

// Get statistics data by period
void getStatisticsByPeriod(int startDay, int endDay, int startDow, int period, uint16_t year) {
  float multiplier = (config.countingUnits == 1) ? 4.16 : 10;
  int totalSprints = 0, totalDays = 0, maximum = 0, minimum = 100;

  int daysInCurrentYear  = isLeapYear(year)     ? 366 : 365;
  int daysInPreviousYear = isLeapYear(year - 1) ? 366 : 365;

  // Calculate DoW of Jan 1 of previous year
  int prevStartDow = (startDow - (daysInPreviousYear % 7) + 7) % 7;
  if (prevStartDow == 0) prevStartDow = 7;

  for (int day = startDay; day <= endDay; day++) {
    int idx;
    int dow;

    if (day >= 1) {
      // Current year
      idx = day;
      dow = (startDow + (day - 1)) % 7;
      if (dow == 0) dow = 7;
    } else {
      // Previous year
      // Wrap to last year's end
      int mappedDay = day + daysInPreviousYear;
      idx = mappedDay;
      dow = (prevStartDow + (mappedDay - 1)) % 7;
      if (dow == 0) dow = 7;
    }

    uint8_t sprintCounter = EEPROM.read(idx);

    // Skip days with empty value or zero sprintCounter (if configured)
    if (sprintCounter == EMPTY_VALUE) continue;
    if (sprintCounter == SPRINTS_END_BYTE) continue;
    if (config.countingDoWs == NOT_0_DAYS && sprintCounter == 0) continue;

    // Skip specific days based on configuration
    if ((config.countingDoWs == FIVE_DAYS && dow >= 6) ||
        (config.countingDoWs == SIX_DAYS  && dow == 7))
        continue;

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
    statisticsArray[TOTAL][period]   = 0;
    statisticsArray[MINIMUM][period] = 0;
    statisticsArray[AVERAGE][period] = 0;
    statisticsArray[MAXIMUM][period] = 0;
  }
}

// Clear statistics cells
void clearStatistics() {
  Serial.println(F("EEPROM: Clearing all statistics"));
  for (uint16_t i = 0; i <= 367; i++) {
    if (EEPROM.read(i) != EMPTY_VALUE) {
      Serial.print(F("EEPROM: Clearing cell: "));
      Serial.println(i);
      EEPROM.update(i, EMPTY_VALUE);
    }
  }

  // Show success message
  MFS.write("donz");
  delay(2500);

  operatingState = STANDBY;

  // Return to main menu
  mainMenuMode = SPRINT;

  statisticsMenuMode = NOW;
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
  char formattedStringAdditional[8];
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
      statisticsMenuMode = NOW;
    }
  } else {
    // Statistics submenu navigation
    if (statisticsMenuMode == NOW) {
      // Today sprints
      runAnimation(0);
    } else if (statisticsMenuMode == DAILY) {
      // Daily sprints
      if (!state.inStatisticsDailySubmenu) {
        getDateFromDayOfYear(inputDayOfYear, formattedString);
        getDateFromDayOfYear(currentDayOfYear, formattedStringAdditional);

        if (strcmp(formattedString, formattedStringAdditional) == 0) {
          inputDayOfYear--;

          if (inputDayOfYear < 1) {
            inputDayOfYear = isLeapYear(currentYear - 1) ? 366 : 365;
          }

          getDateFromDayOfYear(inputDayOfYear, formattedString);
        }

        MFS.write(formattedString);

        if (btn == BUTTON_2_SHORT_RELEASE) {
          // Move to previous date
          inputDayOfYear--;

          if (inputDayOfYear < 1) {
            inputDayOfYear = isLeapYear(currentYear - 1) ? 366 : 365;

            if (inputDayOfYear <= currentDayOfYear) {
                inputDayOfYear = currentDayOfYear + 1;
            }
          }
        } else if (btn == BUTTON_2_LONG_PRESSED) {
          // Fast move to previous date
          inputDayOfYear = inputDayOfYear - 10;

          if (inputDayOfYear < 10) {
            inputDayOfYear = isLeapYear(currentYear - 1) ? 366 : 365;

            if (inputDayOfYear <= currentDayOfYear) {
                inputDayOfYear = currentDayOfYear + 10;
            }
          }
        } else if (btn == BUTTON_3_SHORT_RELEASE) {
          // Statistics daily submenu navigation flag
          state.inStatisticsDailySubmenu = true;
        }
      } else {
        uint8_t eepromValue = EEPROM.read(inputDayOfYear);

        float multiplier = (countingUnit == 'h') ? 0.416 : 1;
        float calculatedValue = 0;

        if (eepromValue != EMPTY_VALUE && eepromValue != SPRINTS_END_BYTE) {
          calculatedValue = multiplier * eepromValue;
        }

        if (countingUnit == 'r') {
          snprintf(formattedString, sizeof(formattedString), "%3d%c", (int)calculatedValue, countingUnit);
        } else {
          // Cant use default sprintf here because of float
          // Need to convert float to string to combine with unit
          dtostrf(calculatedValue, 4, 1, formattedString);
          snprintf(formattedString, sizeof(formattedString), "%4s%c", formattedString, countingUnit);            
        }

        MFS.write(formattedString);
      }

    } else if (statisticsMenuMode != CLEAR) {
      if (!state.inStatisticsDetailsSubmenu) {
        // Show current statistics period
        if (statisticsPeriod <= ROLL7D) {
          // Rolling interval (365/100/60/30/7 days)
          if (statisticsPeriod == ROLL365D) {
              uint16_t days = isLeapYear(currentYear) ? 366 : 365;
              sprintf(formattedString, "%3ud", days);
          } else {
              strcpy_P(formattedString, statisticsPeriods[statisticsPeriod]);
          }
        } else {
          // Monthly interval
          uint8_t menuMonth = statisticsPeriod - MONTH01 + 1;

          if (menuMonth == currentMonth) {
            // Skip current (incomplete) month
            formattedString[0] = '\0';
          } else {
            uint16_t displayYear = currentYear;

            if (menuMonth > currentMonth) {
              // Data for months later than current month exists only for previous year
              displayYear--;
            }

            sprintf(formattedString, "%02d.%02d", menuMonth, displayYear % 100);
          }
        }

        if (formattedString[0] != '\0') {
          MFS.write(formattedString);
        }

        if (btn == BUTTON_2_SHORT_RELEASE) {
          // Cycle to next period item, skipping current (incomplete) month
          do {
              statisticsPeriod = (statisticsPeriod + 1) % STATISTICS_PERIODS_COUNT;
          } while (
              statisticsPeriod >= MONTH01 && statisticsPeriod <= MONTH12 &&
              (statisticsPeriod - MONTH01 + 1) == currentMonth
          );

        } else if (btn == BUTTON_3_SHORT_RELEASE) {
          // Statistics details submenu navigation flag
          state.inStatisticsDetailsSubmenu = true;
        }
      } else {
        float calculatedValue = statisticsArray[statisticsMenuMode][statisticsPeriod] / 10.0;

        if (statisticsMenuMode == TOTAL) {
          snprintf(formattedString, sizeof(formattedString), "%3d%c", (int)calculatedValue, countingUnit);
        } else {
          // Cant use default sprintf here because of float
          // Need to convert float to string to combine with unit
          dtostrf(calculatedValue, 4, 1, formattedString);
          snprintf(formattedString, sizeof(formattedString), "%4s%c", formattedString, countingUnit);
        }

        MFS.write(formattedString);
      }
    }

    if (btn == BUTTON_1_SHORT_RELEASE) {
      if (!state.inStatisticsDetailsSubmenu && !state.inStatisticsDailySubmenu) {
        // Return to main menu
        state.inStatisticsSubmenu = false;
        statisticsPeriod = ROLL365D;
      } else {
        // Return to prev menu
        state.inStatisticsDetailsSubmenu = false;
        state.inStatisticsDailySubmenu = false;
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
      if (config.tune > 0 && config.tune <= melodiesSize) {
        sprintf(formattedString, "tn.%02d", config.tune);
        MFS.write(formattedString);
      } else if (config.tune == melodiesSize + 1) {
        MFS.write("rnd");
      } else {
        MFS.write("off");
      }

      if (btn == BUTTON_2_SHORT_RELEASE) {
        // Cycle to next tune
        config.tune = (config.tune + 1) % (melodiesSize + 2);
      } else if (btn == BUTTON_3_SHORT_RELEASE || btn == BUTTON_1_SHORT_RELEASE) {
        if (btn == BUTTON_3_SHORT_RELEASE && config.tune > 0 && config.tune <= melodiesSize) {
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
        // Return to config menu
        state.inConfigSubmenu = false;
      }
    } else if (configMenuMode == INFO) {
      // Author information
      scrollMessage("GItHUB CO|I BESCHASNY");

      if (btn == BUTTON_3_SHORT_RELEASE || btn == BUTTON_1_SHORT_RELEASE) {
        // Return to config menu
        state.inConfigSubmenu = false;
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
  RtcDateTime now = Rtc.GetDateTime();

  switch (dateTimeMode) {
    case HOURS_MINUTES:
      sprintf(formattedString, "%02d.%02d", now.Hour(), now.Minute());
      break;
    case DAY_MONTH:
      sprintf(formattedString, "%02d.%02d", now.Day(), now.Month());
      break;
    case YEAR:
      sprintf(formattedString, "%02d", now.Year());
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

// Scrolling text animation
void scrollMessage(const char* message) {
  static unsigned long previousMillis = 0;
  static int scrollIndex = 0;
  static const char* lastMessage = nullptr;
  static bool edgePauseActive = false;
  static unsigned long edgePauseMillis = 0;
  static bool firstFourCharsDisplayed = false;

  const unsigned long scrollInterval = 300; // ms between scroll steps
  const unsigned long edgePause = 500;      // ms pause at start and restart

  unsigned long currentMillis = millis();

  int messageLength = strlen(message);
  int maxScrollIndex = messageLength;

  // Detect new message and reset everything
  if (message != lastMessage) {
    scrollIndex = 0;
    lastMessage = message;
    edgePauseActive = true;
    edgePauseMillis = currentMillis;
    firstFourCharsDisplayed = false;

    // Show first 4 chars
    char displayBuffer[5] = "    ";
    for (int i = 0; i < 4; i++) {
      displayBuffer[i] = (i < messageLength) ? message[i] : ' ';
    }
    MFS.write(displayBuffer);

    return;
  }

  // Handle edge pause if needed
  if (edgePauseActive) {
    if (currentMillis - edgePauseMillis >= edgePause) {
      edgePauseActive = false;
      previousMillis = currentMillis;
    } else {
      return;
    }
  }

  if (currentMillis - previousMillis >= scrollInterval) {
    previousMillis = currentMillis;

    // Build and display the 4-char window
    char displayBuffer[5] = "    ";
    for (int i = 0; i < 4; i++) {
      int charIndex = scrollIndex + i;
      displayBuffer[i] = (charIndex < messageLength) ? message[charIndex] : ' ';
    }
    MFS.write(displayBuffer);

    // If first four characters are displayed, ensure a pause
    if (scrollIndex == 0 && !firstFourCharsDisplayed) {
      edgePauseActive = true;
      edgePauseMillis = currentMillis;
      firstFourCharsDisplayed = true;
    }

    // Update scroll index
    scrollIndex++;

    // If fully cleared, reset and trigger restart pause
    if (scrollIndex > maxScrollIndex) {
      scrollIndex = 0;
      edgePauseActive = true;
      edgePauseMillis = currentMillis;

      // Reset so next cycle pauses
      firstFourCharsDisplayed = false;
    }
  }
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
          NOTE_A4 = 440, NOTE_AS4 = 466, NOTE_B4 = 494, NOTE_C4 = 262, NOTE_CS4 = 277, NOTE_D4 = 294, NOTE_DS4 = 311, NOTE_E4 = 330, NOTE_F4 = 349, NOTE_FS4 = 370, NOTE_G4 = 392, NOTE_GS4 = 415,
          NOTE_A5 = 880, NOTE_B5 = 988, NOTE_C5 = 523, NOTE_CS5 = 554, NOTE_D5 = 587, NOTE_DS5 = 622, NOTE_E5 = 659, NOTE_F5 = 698, NOTE_FS5 = 740, NOTE_G5 = 784, NOTE_GS5 = 831,
          NOTE_A6 = 1760,
          REST = 0;

// Koji Kondo - Super Mario Bros. (Flagpole Fanfare)
const int melody1[] PROGMEM = {
  NOTE_FS3, 15,
  NOTE_B3,  15,
  NOTE_DS4, 15,
  NOTE_FS4, 15,
  NOTE_B4,  15,
  NOTE_DS5, 15,
  NOTE_FS5,  5,
  NOTE_DS5,  5,

  NOTE_G3,  15,
  NOTE_B3,  15,
  NOTE_D4,  15,
  NOTE_G4,  15,
  NOTE_B4,  15,
  NOTE_D5,  15,
  NOTE_G5,   5,
  NOTE_D5,   5,

  NOTE_A3,  15,
  NOTE_CS4, 15,
  NOTE_E4,  15,
  NOTE_A4,  15,
  NOTE_CS5, 15,
  NOTE_E5,  15,
  NOTE_A5,   5,
  NOTE_A5,  15,
  NOTE_A5,  15,
  NOTE_A5,  15,
  NOTE_B5,   4,
  REST,     15,
};

// Koji Kondo - Super Mario Bros. (Overworld theme)
const int melody2[] PROGMEM = {
  NOTE_C5,  -6,
  NOTE_G4,  12,
  REST, 6,
  NOTE_E4,  -6,
  NOTE_A4,   6,
  NOTE_B4,   6,
  NOTE_AS4, 12,

  NOTE_A4,   6,
  NOTE_G4, -12,
  NOTE_E5, -12,
  NOTE_G5, -12,
  NOTE_A5,   6,
  NOTE_F5,  12,
  NOTE_G5,  12,
  REST,     12,
  NOTE_E5,   6,
  NOTE_C5,  12,
  NOTE_D5,  12,
  NOTE_B4,  -6,
};

// Henry Mancini - Baby Elephant Walk (intro)
const int melody3[] PROGMEM = {
  NOTE_C4, -8,
  NOTE_E4, 16,
  NOTE_G4,  8,
  NOTE_C5,  8,
  NOTE_E5,  8,
  NOTE_D5,  8,
  NOTE_C5,  8,
  NOTE_A4,  8,
  NOTE_FS4, 8,
  NOTE_G4,  8
};

// Henry Mancini - Baby Elephant Walk (outro)
const int melody4[] PROGMEM = {
  NOTE_C5,   4,
  NOTE_C5,   4,
  NOTE_AS4, 16,
  NOTE_C5,  16,
  NOTE_AS4, 16,
  NOTE_G4,  16,
  NOTE_F4,   8,
  NOTE_DS4,  8,
  NOTE_FS4,  4,
  NOTE_FS4,  4,
  NOTE_F4,  16,
  NOTE_G4,  16,
  NOTE_F4,  16,
  NOTE_DS4, 16,
  NOTE_C4,   8,
  NOTE_G4,   8,
  NOTE_AS4,  8,
  NOTE_C5,   8,
};

// Tetris
const int melody5[] PROGMEM = {
  NOTE_E5, 4,
  NOTE_B4, 8,
  NOTE_C5, 8,
  NOTE_D5, 4,
  NOTE_C5, 8,
  NOTE_B4, 8,
  NOTE_A4, 4
};

// Elderbrook & Vintage Culture - Run
const int melody6[] PROGMEM = {
  NOTE_AS4, 16,
  NOTE_AS4, 16, REST, 16,
  NOTE_AS4, 16, REST, 16,
  NOTE_GS4, 16, REST, 16,
  NOTE_GS4, 16, REST, 16,
  NOTE_GS4, 16, REST, 16,
  NOTE_GS4, 16,
  NOTE_AS4, 16, REST, 16,
  NOTE_G4,  16, REST, 16,

  NOTE_DS4, 16,
  NOTE_DS4, 16, REST, 16,
  NOTE_DS4, 16, REST, 16,
  NOTE_DS4, 16, REST, 16,
  NOTE_DS4, 16, REST, 16,
  NOTE_DS4, 16, REST, 16,
  NOTE_DS4, 16,
  NOTE_F4,  16, REST, 16,
  NOTE_D4,  16, REST, 16
};

// K.Flay - Not In California
const int melody7[] PROGMEM = {
  NOTE_GS4, 6,
  NOTE_FS4, 6,
  NOTE_GS4, 6,
  NOTE_FS4, 6,
  NOTE_E4,  6,
  NOTE_B3,  3,
  NOTE_E4,  2,
  NOTE_E4,  6,
  NOTE_DS4, 6,
  NOTE_CS4, 3
};

// Lana Del Ray - Summertime Sadness
const int melody8[] PROGMEM = {
  NOTE_CS4, 10, REST, 15,
  NOTE_CS4, 10, REST, 15,
  NOTE_CS4, 10, REST, 15,
  NOTE_E4,   6,
  NOTE_FS4,  6,
  NOTE_E4,   3,
  NOTE_CS4, 10, REST, 15,
  NOTE_CS4, 10, REST, 15,
  NOTE_CS4,  4, REST, 15,
  NOTE_FS4,  3,
  NOTE_E4,   3
};

// Coldplay - Clocks
const int melody9[] PROGMEM = {
  NOTE_DS4, 10,
  NOTE_AS3, 10,
  NOTE_G3,  10,
  NOTE_DS4, 10,
  NOTE_AS3, 10,
  NOTE_G3,  10,
  NOTE_DS4, 10,
  NOTE_AS3, 10,
  NOTE_CS4, 10,
  NOTE_AS3, 10,
  NOTE_F3,  10,
  NOTE_CS4, 10,
  NOTE_AS3, 10,
  NOTE_F3,  10,
  NOTE_CS4, 10,
  NOTE_AS3, 10
};

// Metallica - Master Of Puppets
const int melody10[] PROGMEM = {
  NOTE_E3,  12,
  NOTE_E3,  12,
  NOTE_E4,  12,
  NOTE_E3,  12,
  NOTE_E3,  12,
  NOTE_DS4, 12,
  NOTE_E3,  12,
  NOTE_E3,  12,
  NOTE_D4,   6,
  NOTE_CS4,  6,
  NOTE_C4,   3,
  
  NOTE_E3,  12,
  NOTE_E3,  12,
  NOTE_B3,  12,
  NOTE_E3,  12,
  NOTE_E3,  12,
  NOTE_AS3, 12,
  NOTE_E3,  12,
  NOTE_E3,  12,
  NOTE_A3,  12,
  NOTE_E3,  12,
  NOTE_GS3, 12,
  NOTE_E3,  12,
  NOTE_G3,  12,
  NOTE_E3,  12,
  NOTE_FS3, 12,
  NOTE_E3,  12,
};

// Little Barrie - Better Call Saul Theme
const int melody11[] PROGMEM = {
  NOTE_A4,   2, REST, 4,
  NOTE_E4,  12,
  NOTE_A4,  12,
  NOTE_CS5, 12,
  NOTE_GS4, 12,
  NOTE_E4,   6,
  NOTE_C5,   2, REST, 4,
  NOTE_E5,   6,
  NOTE_D5,  12,
  NOTE_C5,   4,
  NOTE_A4,   2,
};

// Gorillaz - Clint Eastwood
const int melody12[] PROGMEM = {
  NOTE_DS5, 12,
  NOTE_CS5,  6,
  NOTE_DS5,  6,
  NOTE_DS4,  6, REST, 6,
  NOTE_DS4,  6,
  NOTE_DS4, 12,
  NOTE_FS4,  6,
  NOTE_AS4,  6,
  NOTE_GS4, 12,
  NOTE_AS4,  6,
  NOTE_DS5,  6,
  NOTE_DS4,  6, REST, 6,
  NOTE_DS4,  6,
  NOTE_DS4, 12,
  NOTE_FS4,  6,
  NOTE_AS4,  6,
};

// Fontaines D.C. - Starbuster
const int melody13[] PROGMEM = {
  NOTE_A3,   3,
  NOTE_FS4,  3,
  NOTE_A3,   6,
  NOTE_D4,  18,
  NOTE_FS4, 18,
  NOTE_A4,  18,
  NOTE_D5,   3,
  NOTE_AS3,  3,
  NOTE_F4,   3,
  NOTE_D5,   3,
  NOTE_C5,   3,
};

// A-Ha - Take On Me
const int melody14[] PROGMEM = {
  NOTE_FS5, 10,
  NOTE_FS5, 10,
  NOTE_D5,  10,
  NOTE_B4,  10, REST, 10,
  NOTE_B4,  10, REST, 10,
  NOTE_E5,  10, REST, 10,
  NOTE_E5,  10, REST, 10,

  NOTE_E5,  10,
  NOTE_GS5, 10,
  NOTE_GS5, 10,
  NOTE_A5,  10,
  NOTE_B5,  10,
  NOTE_A5,  10,
  NOTE_A5,  10,
  NOTE_A5,  10,
  NOTE_E5,  10, REST, 10,
  NOTE_D5,  10, REST, 10,
  NOTE_FS5, 10, REST, 10,
  NOTE_FS5, 10, REST, 10,

  NOTE_FS5, 10,
  NOTE_E5,  10,
  NOTE_E5,  10,
  NOTE_FS5, 10,
  NOTE_E5,  10,
};

const int* const melodies[] PROGMEM = {
  melody1,
  melody2,
  melody3,
  melody4,
  melody5,
  melody6,
  melody7,
  melody8,
  melody9,
  melody10,
  melody11,
  melody12,
  melody13,
  melody14
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
  sizeof(melody9) / sizeof(melody9[0]),
  sizeof(melody10) / sizeof(melody10[0]),
  sizeof(melody11) / sizeof(melody11[0]),
  sizeof(melody12) / sizeof(melody12[0]),
  sizeof(melody13) / sizeof(melody13[0]),
  sizeof(melody14) / sizeof(melody14[0]),
};

const uint8_t melodiesSize = sizeof(melodies) / sizeof(melodies[0]);

// Play melody with disco lights
void playMelody(byte melodyId) {
  if (melodyId == melodiesSize + 1) {
    // Randomize melodies
    melodyId = random(1, melodiesSize + 1);
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

