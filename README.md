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
- DuPont jumper wires (5x female-to-female, 20 cm each)
- Power cable for Arduino

### Connection and wiring diagram

> ⚠️ **Warning:** When mounting the Multifunction Shield on the Arduino Uno with a USB Type-B socket, ensure the underside of the shield **does not touch** the metal parts of the socket. Contact may cause a short circuit when powering the device. Use a thin piece of insulating material (like a plastic card) between the boards to prevent accidental contact.

- DS1302 connects to MFS pins:
  - `RST` → `~5`
  - `DAT` → `~6`
  - `CLK` → `~9`
  - `VCC` → `+5V`
  - `GND` → `GND`
- MFS (with jumper near the buttons) plugs directly onto Arduino
- USB power cable (5 V) or external power connects to Arduino

![runUp: Arduino Pomodoro Timer wiring diagram](docs/wiring-diagram.png)

### RTC module

If you plan to build an enclosure or want to keep the DS1302 RTC module hidden under the MFS, you may need to trim the module's corners (depending on the board's component layout) and bend its contacts to 90° so it can fit between the boards, as shown with red marks in the illustration below. Do this carefully to avoid damaging the module's tracks.

![Placing the RTC module](docs/assembly/step-0-rtc.png)

> ⚠️ **Warning:** Be sure to insulate the DS1302 RTC module with tape to prevent accidental damage or short circuits. You may also need to trim the pins or leads of the module's components if they are too long and prevent it from fitting between the boards.

On the MFS, place a jumper wire from the bottom-right group of pins, routing it between the buzzer and the blue trimmer resistor and then up through the top side of the shield, as shown with green marks above. From there, lead the wire through the top part of the Arduino (between the USB socket and the 9V power input) and connect it to the DS1302, which is located between the boards.

Note that the wire routing and RTC module position depend on the board's component layout and the jumper wire length, so they may differ from the illustration shown here in your case.

## Enclosure

### Tools

You will need the following tools to build the enclosure:

- Wire cutters, pliers, awl, utility knife (for case walls, holes and button rods)
- Flat and round files (for smoothing and adjusting case walls and rod edges)
- Lighter or matches (for shrinking heat-shrink tubing)
- Black permanent marker (for coloring button rods)

### Parts

The enclosure can be assembled from the following components:

- Two transparent acrylic Arduino Uno cases
- Hex standoffs (M3 threaded, with screws and nuts) for fixing the enclosure
- Bamboo sushi stick pieces as button rods
- Heat-shrink tubing to hold button rods and case walls in place

It's best to have sets of different M3 hex standoffs and heat-shrink tubing to test-fit the optimal sizes. In my case, the following dimensions worked best:

#### M3 hex standoff

Optimal body length: **2 x 32 mm** (bottom part) and **2 x 30 mm** (top part). These measurements refer to the standoff body only and do not include the **6 mm** threaded sections on each end. They can be combined from **20 mm**, **12 mm** and **10 mm** pieces.

#### Heat-shrink tubing

Optimal inner diameter: **6 mm** for the button rods, **3 mm** for the case walls.

### Assembly

#### Step 1: Preparing the case parts

For the front (with buttons) and back (opposite) sides of the enclosure, use two big bottom parts of Arduino Uno acrylic cases, as shown below.

![Assembly step 1: Preparing the case parts](docs/assembly/step-1-case-parts.png)

#### Step 2: Enlarging corner and button holes

If you plan to use M3 threaded hex standoffs, first enlarge the four corner mounting holes in the front and back parts of the enclosure (see red marks). Then mount the Arduino with screws on the back side of the enclosure and plug the MFS directly onto it.

Assemble the front and back sides of the enclosure, then mark the positions of the future button holes — 3 for MFS bottom buttons and 1 for reset button — on the front panel, as shown with purple marks in the illustration below.

![Assembly step 2: Enlarging corner and button holes](docs/assembly/step-2-holes.png)

Make the holes with an awl and carefully enlarge them with a round file until they match the diameter of your button rods (sushi sticks or something similar). Try to make it as precise as possible, because if the holes are too large or misaligned with the buttons, pressing them and using the runUp device will be difficult.

#### Step 3: Preparing the button rods

Cut the sushi sticks into 4 pieces: 3 for the MFS bottom buttons (about **14 mm** high) and 1 for the reset button (about **11 mm** high). The reset button rod is shorter due to the height difference between the top and bottom parts of the enclosure. The jump wire connectors at the bottom increase the height, but if you are comfortable with soldering, you can make the enclosure symmetrical.

Exact dimensions may vary depending on your specific board, so remember: **measure twice, cut once**. Each button rod should extend roughly **1-2 mm** above the front surface of the enclosure, because if the rods are too short or too tall, pressing them will be difficult.

Once you have finished sizing the rods, use a utility knife to make small recesses on the bottom side of each rod — this will make them more stable when mounted on the MFS buttons. Then slide a heat-shrink tube onto each rod (inner diameter **6 mm**), leaving about **3 mm** length exposed at the top, and use a lighter to shrink the tube. This will hold the rods in place so they won't fall out if you turn your device upside down.

![Assembly step 3: Preparing the button rods](docs/assembly/step-3-button-rods.png)

Use a black permanent marker to color the button rods so the heat-shrink tubing blends in.

#### Step 4: Preparing the side walls

Assemble case parts with the standoffs and test-fit the case walls to check the alignment.

Use a utility knife and a flat file to adjust the walls at the marked positions (shown below) until they fit properly with the standoffs. Secure each pair of walls in a stacked position with heat-shrink tubing (3 mm fits well, even without heating).

![Assembly step 4: Preparing the side walls](docs/assembly/step-4-side-walls.png)

#### Step 5: Final look

After routing the jumper wire and assembling the enclosure, the device should look like this (button icons added for clarity):

![Assembly step 5: Final look](docs/assembly/step-5-runUp-device.png)
