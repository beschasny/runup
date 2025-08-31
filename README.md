<img src="docs/icons/r.svg" alt="r" width="64" /><img src="docs/icons/u.svg" alt="u" width="64" /><img src="docs/icons/n.svg" alt="n" width="64" /><img src="docs/icons/up-arrow.svg" alt="Up arrow" width="64" />

# **runUp**: Arduino Pomodoro Timer

Pomodoro timer implementation for **Arduino Uno R3** (or compatible) using a **Multifunction Shield** and a **DS1302 RTC module**. It helps manage work in focused sprints, track productivity with daily historical statistics stored in EEPROM and customize timer behavior with various settings.

The runUp device is easy to assemble and requires no soldering. It uses widely available, low-cost components, making it accessible for hobbyists and beginners. The Multifunction Shield plugs directly onto the Arduino Uno, and the DS1302 RTC module connects via simple jumper wires.

The device can be placed into a simple DIY enclosure. Using two transparent acrylic Arduino Uno cases, hex standoffs, bamboo sushi stick and heat-shrink tubing, you can easily build a good-looking case.

## Features

### Sprint sessions

The runUp device follows the classic [Pomodoro technique](https://en.wikipedia.org/wiki/Pomodoro_Technique). Work sessions last **25 minutes**, during which you should focus solely on your task without distractions. Short **5-minute** breaks occur between each session, and after every **four** completed sessions, a longer **15-minute** break is taken. This cycle repeats continuously to help maintain focus and productivity.

| Session Type      | Duration | Notes                                      |
|-------------------|----------|--------------------------------------------|
| Work              | 25 min   | Focused work, no distractions              |
| Short Break       | 5 min    | Quick rest, stretch                        |
| Long Break        | 15 min   | After every 4 work sessions, extended rest |

### Statistics tracking

- Saves the number of completed sprints per day in EEPROM
- Provides weekly, monthly and yearly overview of your progress
- Shows statistics with minimum, average and maximum sprint counts

### Customizable settings

There are **one-click modes**: temporary modes activated while the device is powered on with a simple button hold. These modes are not saved, so there is no need to manually toggle settings in the configuration menu. Once the device is turned off, the temporary modes are reset.

- **Silent mode**: turns off all sound
- **No-music mode**: turns off melody playback but keeps simple beeps
- **Hero mode**: repeats 25-minute work sessions without breaks

There are also customizable settings that can be saved in EEPROM through the configuration menu:

- **Display**: 4 brightness levels
- **Sound**: silent mode, simple beeps or melody playback
- **Tune**: 8 built-in melodies for work session completion
- **Reminders**: beeps during pauses or breaks
- **LED**: progress indication via flickering light
- **Statistics**: flexible calculation logic with selectable units (hours or sprints)
  - All days of the week
  - Monday–Friday
  - Monday–Saturday
  - Only days with non-zero sprints
- **Hero mode**: repeats 25-minute work sessions without breaks
- **Sleep mode**: reduces power consumption with the display off

### Date and time handling

The RTC (real-time clock) module keeps track of the current date and time even when the device is powered off. The date and time can be set through the Clock menu, making it easy to monitor daily progress and align work sessions with real-world schedules.

## Hardware

- **Arduino Uno R3** (or compatible)
- **Multifunction Shield** (MFS)
- **DS1302** RTC module (with CR2032 battery)
- Jump wire DuPont cable (5x female-to-female) to connect DS1302 to MFS
- Power cable for Arduino

### Connection

> ⚠️ **Warning**: When mounting the Multifunction Shield on the Arduino Uno with a USB Type-B socket, ensure the underside of the shield **does not touch** the metal parts of the socket. Contact may cause a short circuit when powering the device. Use a thin piece of insulating material (like a plastic card) between the boards to prevent accidental contact.

- DS1302 connects to MFS pins:
  - `RST` → `~5`
  - `DAT` → `~6`
  - `CLK` → `~9`
  - `VCC` → `+5V`
  - `GND` → `GND`
- MFS plugs directly onto Arduino
- USB power cable (5 V) or external power connects to Arduino

If you plan to build an enclosure or want to keep the DS1302 module hidden under the MFS, you may need to trim its corners and bend the contacts 90° to fit between the boards. Make sure to insulate the module with tape to prevent accidental damage and short circuits.

### Wiring diagram

<img src="docs/wiring-diagram.png" alt="runUp - Arduino Pomodoro Timer wiring diagram" width="800" />
