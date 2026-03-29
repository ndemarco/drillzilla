# Drillzilla: Nicky's Monster Drill Press Controller

**When your drill press needs a PID loop, VFD, and a touchscreen**

---

## Project Overview

A closed-loop speed control system for a drill press using:
- **VFD**: Nidec M200 (Size 2) variable frequency drive for 2HP 3-phase motor
- **VFD interface**: AI-485 adapter - RS485
- **Controller**: ESP32 Cheap Yellow Display (CYD) board
- **Feedback**: Digital tachometer for precise speed measurement
- **Communication**: RS485 Modbus RTU for bidirectional control and monitoring

### Key Features
- True closed-loop speed regulation with PID control
- Real-time speed, current, and load monitoring on TFT display
- Stall and overload detection
- Digital communication for diagnostics and status
- External PID loop compensates for M200's insufficient RFC-A performance

---

## System Architecture

```
┌─────────────────┐
│ Digital         │ (Actual Speed Feedback)
│ Tachometer      │
└────────┬────────┘
         │ Pulse Train
         ▼
┌─────────────────────────────────────┐
│    ESP32 CYD (Controller)           │
│                                     │
│  ┌──────────────────────────────┐  │
│  │  Hardware Pulse Counter      │  │
│  │  (PCNT - Zero CPU overhead)  │  │
│  └──────────┬───────────────────┘  │
│             ▼                       │
│  ┌──────────────────────────────┐  │
│  │   PID Speed Controller       │  │
│  │   - Proportional gain        │  │
│  │   - Integral (anti-windup)   │  │
│  │   - Derivative filtering     │  │
│  └──────────┬───────────────────┘  │
│             ▼                       │
│  ┌──────────────────────────────┐  │
│  │   Modbus RTU Master          │  │
│  │   - Write freq reference     │  │
│  │   - Read motor current       │  │
│  │   - Read output frequency    │  │
│  │   - Detect faults            │  │
│  └──────────┬───────────────────┘  │
│             │                       │
│  ┌──────────────────────────────┐  │
│  │   5" IPS 480x800 (portrait)   │  │
│  │   - capacitive touch (GT911) │  │
│  │   - Setpoint RPM             │  │
│  │   - Actual RPM               │  │
│  │   - Direction (FWD/REV)      │  │
│  │   - Load bar graph           │  │
│  │   - Speed presets             │  │
│  │   - System state             │  │
│  └──────────────────────────────┘  │
│                                     │
│  Control Inputs:                   │
│  ┌──────────────────────────────┐  │
│  │ • ON Button (GPIO26)         │  │
│  │ • OFF Button (GPIO27)        │  │
│  │ • Grayhill Encoder (speed)   │  │
│  │ • Encoder Button (presets)   │  │
│  └──────────────────────────────┘  │
│                                     │
│  Status Output:                    │
│  ┌──────────────────────────────┐  │
│  │ • LED (GPIO23)               │  │
│  │   - Solid: At speed          │  │
│  │   - Flash: Not at speed      │  │
│  │   - Off: Disabled            │  │
│  └──────────────────────────────┘  │
└──────────┬──────────────────────────┘
           │ RS485 (Modbus RTU)
           │ 38400 baud, 8-N-1
           ▼
    ┌──────────────┐
    │  MAX485     │  (Level converter)
    │  Module     │
    └──────┬───────┘
           │ Differential pair
           │ (Noise immune)
           ▼
    ┌──────────────────────────┐
    │  Nidec M200 Drive        │
    │  + AI-485 Adaptor        │
    │                          │
    │  Control Inputs:         │
    │  - Drive Enable (T11)    │
    │  - Run Forward (T12)     │
    │  - Run Reverse (T13)     │
    │  - E-Stop (hardwired)    │
    └──────────┬───────────────┘
               │ 3-Phase Power
               ▼
    ┌──────────────────────────┐
    │   3-Phase Motor          │
    │   2 HP, 4-pole           │
    │   With shaft-mounted     │
    │   digital tachometer     │
    └──────────────────────────┘
```

---

## Hardware Components

### Primary Components

| Component | Part Number / Spec | Purpose | Notes |
|-----------|-------------------|---------|-------|
| VFD | Nidec M200-022-00075A | Motor control | Size 2, 230V 3-phase, 7.5A |
| RS485 Interface | AI-485 Adaptor | Modbus communication | Mounts on M200 |
| Controller | ESP32-8048S050C (CYD) | PID + Display | ESP32-S3, 5" IPS 800×480 portrait, capacitive touch (GT911), 8M PSRAM, 16M Flash |
| Level Shifter | MAX485 / MAX3485 | RS485 interface | TTL to RS485 |
| Speed Control | Grayhill 62A22-02-045CH | Setpoint input | Quadrature encoder, 45° detent, push button |
| Tachometer | Single pulse output | Speed feedback | 1-4 PPR (configurable) |
| Motor | Marathon SKV145THTR5329AA | 3-phase induction | 2 HP, 4-pole, 230/460V |

### Supporting Components

- **Encoder**: Grayhill 62A22-02-045CH (quadrature with push button) for speed setpoint
- **ON/OFF Buttons**: Momentary push buttons (SPST NO) for system enable/disable
- **Status LED**: High-brightness LED with 220Ω current-limiting resistor
- **Emergency Stop**: Hardwired to drive enable circuit
- **Enclosure**: NEMA-rated for industrial environment
- **Power Supply**: 5V for ESP32 (USB or isolated supply)
- **Wiring**: Shielded twisted pair for RS485, shielded cable for tachometer

---

## Electrical Connections

### ESP32 CYD to MAX485 Module

| ESP32 Pin | Function | MAX485 Pin | Notes |
|-----------|----------|------------|-------|
| GPIO16 | UART2 RX | RO | Receive data from RS485 |
| GPIO17 | UART2 TX | DI | Transmit data to RS485 |
| GPIO4 | Direction control | DE + RE | Tied together, controls TX/RX |
| 3.3V | Power | VCC | Power for MAX485 |
| GND | Ground | GND | Common ground |

### ESP32 Control Inputs and Outputs

| ESP32 Pin | Function | Connection | Notes |
|-----------|----------|------------|-------|
| GPIO26 | ON Button | Momentary switch to GND | Active LOW with internal pullup |
| GPIO27 | OFF Button | Momentary switch to GND | Active LOW with internal pullup |
| GPIO23 | Status LED | LED + resistor to GND | Active HIGH, 220Ω resistor recommended |

### MAX485 to M200 AI-485 Adaptor

| MAX485 Pin | M200 AI-485 Pin | Signal | Notes |
|------------|-----------------|--------|-------|
| A | Pin 3 (RX TX) | Data+ | RS485 differential pair |
| B | Pin 2 (RX\ TX\) | Data- | RS485 differential pair |
| - | Pin 1 (0V) | Ground | Common reference |
| - | Pin 2 → Pin 4 | Termination | 120Ω resistor at M200 end only |

**Important**: 
- Use shielded twisted pair cable for RS485
- Keep RS485 runs under 1200m (typically much shorter)
- Ground shield at one end only (M200 end preferred)
- Add 120Ω termination at M200 end (link pins 2-4)

### Digital Tachometer to ESP32

**Single Pulse Output Tachometer (1-4 PPR selectable)**

Connection for typical NPN open collector or 5V logic output:
```
Tach Signal → ESP32 GPIO34
Tach GND    → ESP32 GND
Tach VCC    → 5V (if powered tach)
             ↓
           100pF cap to GND (noise filter)
```

**For NPN open collector:**
```
+3.3V → 4.7kΩ pullup → GPIO34 ← Tach Signal
                        ↓
                      100pF cap
Tach GND → ESP32 GND
```

**Important for Low PPR:**
- With 1 PPR, RPM calculation needs longer averaging
- At 1800 RPM with 1 PPR: only 30 pulses/second
- Use 500ms sample period for stable readings
- Consider increasing to 2 or 4 PPR for faster response

### Grayhill Encoder (Speed Setpoint Control)

**Grayhill 62A22-02-045CH Specifications:**
- Quadrature encoder with A/B channels
- 45° detent positions (8 positions per revolution)
- Push button switch integrated
- Output: Open collector NPN
- Supply: 5V typical

**Connection to ESP32:**
```
Encoder Pin    ESP32 Pin     Function
───────────────────────────────────────
Common         GND           Ground reference
+5V            5V            Power supply
Channel A      GPIO32        Quadrature A (with pullup)
Channel B      GPIO33        Quadrature B (with pullup)
Push Button    GPIO25        Switch input (with pullup)

Internal ESP32 pullups enabled, or add external:
+3.3V → 4.7kΩ → GPIO32/33/25
```

**Encoder Reading Strategy:**
- Use interrupt-driven quadrature decoding
- Each detent = adjustable RPM step (e.g., 50 RPM per click)
- Push button for speed preset selection or reset
- Display current setpoint on TFT

### M200 Control Terminals

| Terminal | Function | Connection | Notes |
|----------|----------|------------|-------|
| 1 | 0V Common | System ground | Reference for all signals |
| 2 | Analog Input 1 | Not used | Reserved for future analog control |
| 4 | +10V Output | Not used | Available for sensors |
| 5 | Analog Input 2 | Not used | Reserved |
| 9 | +24V Output | Optional | Can power external devices (100mA max) |
| 10 | Digital I/O 1 | Not used | Configurable |
| 11 | Drive Enable | Enable switch | Must be high to run |
| 12 | Run Forward | Control signal | Digital input |
| 13 | Run Reverse | Control signal | Digital input |
| 14 | Digital Input 5 | Not used | Configurable |
| 41, 42 | Relay | Drive OK status | NO contacts, 2A @ 240VAC |

**Safety**: 
- Wire emergency stop to break Terminal 11 (Drive Enable)
- Consider adding run/stop buttons for Terminals 12/13
- Use relay output (41, 42) for status indication light

---

## Configuration

**Marathon Motor Nameplate Data:**
- Model: SKV145THTR5329AA
- HP: 2.0
- Poles: 4
- RPM: 1750 (full load)
- Voltage: 230/460V (use 230V with M200-022)
- Current: 7.5A @ 230V, 3.75A @ 460V
- Frequency: 60 Hz
- Service Factor: 1.0

### ESP32 Configuration Constants

#### Tachometer Settings
```cpp
#define TACH_PIN 34           // GPIO for digital tach input
#define TACH_PPR 1            // Single pulse per revolution (1-4 PPR selectable)
#define SAMPLE_PERIOD_MS 500  // Calculate RPM every 500ms (longer for low PPR)
```

**PPR Configuration**:
- Tachometer is configurable for 1, 2, 3, or 4 pulses per revolution
- **Start with 1 PPR** for testing
- Higher PPR gives faster update rate but may need adjustment:
  - 1 PPR @ 1800 RPM = 30 Hz signal → 500ms sample period
  - 4 PPR @ 1800 RPM = 120 Hz signal → 100ms sample period possible
- Can increase PPR later for better responsiveness

#### Motor Settings
```cpp
#define MOTOR_POLES 4         // Marathon SKV145THTR5329AA is 4-pole
#define MOTOR_BASE_FREQ 60.0  // Rated frequency (60 Hz)

// Calculated base RPM
const float MOTOR_BASE_RPM = (120.0 * MOTOR_BASE_FREQ) / MOTOR_POLES;
// 4-pole, 60Hz → 1800 RPM synchronous (actual ~1750 RPM @ full load)
```

#### PID Tuning Parameters
```cpp
float Kp = 2.5;   // Proportional gain - START HERE
float Ki = 0.8;   // Integral gain
float Kd = 0.05;  // Derivative gain

const float MAX_INTEGRAL = 500.0;  // Anti-windup limit
const float MAX_SPEED_HZ = 60.0;   // Frequency limit
const float MIN_SPEED_HZ = 5.0;    // Minimum useful speed
```

#### Protection Settings
```cpp
const float CURRENT_LIMIT = 7.5;  // Marathon motor rated current @ 230V
const float OVERLOAD_THRESHOLD = 0.9;  // Trip at 90% of current limit (6.75A)
const float SPEED_ERROR_THRESHOLD = 0.15;  // 15% speed error tolerance
```

---

## PID Tuning Procedure

### Initial Setup
1. Start with conservative gains:
   ```cpp
   Kp = 1.0;
   Ki = 0.0;
   Kd = 0.0;
   ```

2. Set a moderate speed setpoint (50% of max)

3. Enable the system and observe response

### Step 1: Tune Proportional Gain (Kp)
1. Increase Kp in steps of 0.5
2. Look for:
   - **Too low**: Slow response, large steady-state error
   - **Too high**: Overshoot, oscillation
   - **Just right**: Fast response with slight overshoot
3. Target: 10-20% overshoot on step response
4. **Typical range**: 1.0 to 5.0 for drill press

### Step 2: Add Integral Gain (Ki)
1. Start with Ki = Kp / 10
2. Gradually increase Ki
3. Look for:
   - **Too low**: Steady-state error remains under load
   - **Too high**: Slow oscillation, instability
   - **Just right**: Zero steady-state error, no oscillation
4. **Typical range**: 0.3 to 2.0 for drill press

### Step 3: Add Derivative Gain (Kd) - Optional
1. Only if overshoot/oscillation persists
2. Start with Kd = Kp / 100
3. Increase gradually
4. **Typical range**: 0.01 to 0.1 for drill press

### Load Testing
1. Apply drilling load while running
2. Observe speed drop and recovery time
3. Increase Ki if speed droop is excessive
4. Ensure no oscillation under varying loads

### Fine Tuning Tips
- **Speed droop under load**: Increase Ki
- **Overshoot on start**: Decrease Kp or increase Kd
- **Slow response**: Increase Kp
- **Oscillation**: Decrease Kp, possibly decrease Ki
- **Instability**: Reduce all gains by 50%, start over

### Anti-Windup Verification
Test at maximum speed limit:
1. Set setpoint above maximum
2. Verify system doesn't accumulate excessive integral
3. Check smooth transition when limit released
4. Adjust `MAX_INTEGRAL` if needed

---

## System Operation

### Control Loop Timing

```
Main Loop (20ms / 50Hz):
├─ Read tachometer via PCNT
├─ Read setpoint (potentiometer or touchscreen)
├─ Calculate PID output
└─ Write frequency command to M200 via Modbus

Monitoring Loop (100ms / 10Hz):
├─ Read motor current from M200
├─ Read output frequency from M200
├─ Check for overload condition
└─ Update system state

Display Loop (200ms / 5Hz):
├─ Update setpoint display
├─ Update actual speed display
├─ Update current and load display
├─ Update bar graph
└─ Update system state indicator
```

### State Machine

```
DISABLED (Initial State)
  ├─ Conditions: system_enabled = false
  ├─ Actions: Output = 0 Hz, LED off, display shows "DISABLED"
  ├─ User Can: Adjust setpoint (preview), press ON button
  └─ Transitions: → STOPPED (ON button pressed)

STOPPED
  ├─ Conditions: System enabled, setpoint < minimum speed
  ├─ Actions: Output = 0 Hz, reset integral, LED flashes
  └─ Transitions: → DISABLED (OFF button)
                  → RUNNING (setpoint > minimum)
                  → FAULT (drive trip)

RUNNING
  ├─ Conditions: System enabled, valid setpoint, drive enabled
  ├─ Actions: PID active, monitor current/speed
  ├─ LED Status: Solid if at speed (±10%), flashing if not
  └─ Transitions: → DISABLED (OFF button)
                  → STOPPED (setpoint < minimum)
                  → OVERLOAD (high current + speed error)
                  → FAULT (drive trip)

OVERLOAD
  ├─ Conditions: Current > 90% limit AND speed error > 15%
  ├─ Actions: Reduce output by 50%, reset integral, LED flashes rapidly
  └─ Transitions: → DISABLED (OFF button)
                  → RUNNING (overload cleared)
                  → FAULT (sustained overload)

FAULT
  ├─ Conditions: Drive trip, communication loss, E-stop, critical error
  ├─ Actions: Stop drive, display fault code, LED flashes rapidly
  └─ Transitions: → DISABLED (manual reset: OFF button + clear fault)
```

### Startup Sequence

1. **Power On**
   - ESP32 boots, initializes PCNT, RS485, display, controls
   - Display shows "Initializing..."
   - Status LED off
   - System starts in DISABLED state

2. **Establish Communication**
   - Attempt Modbus connection to M200
   - Read drive status and parameters
   - Verify drive is ready (not tripped)

3. **System Ready**
   - Display shows "System Ready" with "DISABLED" state
   - Encoder can adjust setpoint (displayed but not applied)
   - Waiting for ON button press

4. **Enable System (ON Button)**
   - User presses ON button
   - System transitions to ENABLED state
   - Display shows current state (STOPPED or RUNNING)
   - Status LED begins operating (solid or flashing)
   - If setpoint > minimum, motor begins running

5. **Running**
   - PID loop controls motor speed to match setpoint
   - Status LED: Solid when at speed (±10%), flashing when not at speed
   - Encoder adjusts setpoint in real-time
   - Display updates continuously

### Shutdown Sequence

1. **Stop Command (OFF Button)**
   - User presses OFF button
   - System immediately transitions to DISABLED state
   - PID output set to 0 Hz
   - Integral term reset
   - Motor ramps down per M200 deceleration settings
   - Status LED turns off
   - Display shows "DISABLED"

2. **Safe Stop**
   - Motor coasts or ramps to stop based on M200 configuration
   - Drive remains powered and ready
   - Setpoint is preserved
   - System can be re-enabled with ON button

3. **Emergency Stop**
   - E-stop button removes drive enable (Terminal 11)
   - Drive immediately coasts to stop
   - ESP32 detects drive disabled via Modbus, enters FAULT state
   - Display shows "FAULT - E-STOP ACTIVE"
   - Status LED flashes rapidly
   - Requires E-stop reset AND OFF/ON button cycle to recover

---

## Status LED Behavior

The status LED provides at-a-glance feedback on system state:

| LED State | Meaning | Condition |
|-----------|---------|-----------|
| **Off** | System disabled | OFF button pressed or power off |
| **Solid ON** | Running at speed | Speed error < 10% of setpoint |
| **Rapid Flash** (5 Hz) | Running but not at speed | Speed error ≥ 10% or accelerating |
| **Fast Flash** (10 Hz) | Fault condition | E-stop active, drive trip, or critical error |

**LED Connection:**
```
ESP32 GPIO23 → 220Ω resistor → LED Anode (+)
                                LED Cathode (-) → GND

For panel-mount LED with bezel:
Use LED with built-in resistor OR calculate for 3.3V logic:
R = (3.3V - LED_Vf) / 10mA
```

**Typical LED Forward Voltages:**
- Red: 2.0V → 130Ω resistor
- Green: 2.1V → 120Ω resistor  
- Yellow: 2.1V → 120Ω resistor
- Blue/White: 3.0V → 30Ω resistor

Use 220Ω as universal value (safe for all LED types).

---

## Touchscreen UI

### Display Orientation

The CYD is mounted in **portrait orientation**: 480 pixels wide × 800 pixels tall. All UI layout is designed for this aspect ratio.

### Operator Controls

| Control | Type | Function |
|---------|------|----------|
| Rotary Encoder | Grayhill 62A22 w/ pushbutton | Primary speed adjustment (free dial), preset selection |
| Start Button | Physical momentary (NO) | Enable drive — software cannot start the motor |
| Stop Button | Physical momentary (NO) | Disable drive — software cannot start the motor |
| Power Off | Physical toggle (elsewhere on machine) | Full system power off |
| Touchscreen | 5" capacitive (GT911) | Presets, direction, settings, numpad speed entry |

**Design principle**: The motor can only be started/stopped by physical buttons. The touchscreen controls setpoint, direction, and configuration — never run/stop.

### Screens

1. **Main Screen** — always-visible operating view
2. **Numpad Overlay** — modal, for direct RPM entry
3. **Preset Label Keyboard** — modal, for naming a preset slot
4. **Settings Screen** — full-screen, accessed via gear icon (disabled while inverter is on)
5. **Fault Overlay** — modal, appears over main screen on VFD fault

### Main Screen

The main screen is the always-visible operating view. All elements are sized to be readable at arm's length. Layout is portrait (480 wide × 800 tall).

```
┌──────────────────────────┐
│  DRILLZILLA    READY  ●  │  ← Status bar: state, comms health icon
│──────────────────────────│
│                          │
│        ► FWD             │  ← Direction toggle (tap when stopped)
│                          │
│        1275              │  ← Actual RPM (large, dominant)
│         RPM              │
│                          │
│     ──── 1300 ────       │  ← Setpoint RPM (tap → numpad)
│                          │
│  ┌────────────────────┐  │
│  │████████████░░░░░░░░│  │  ← Load bar (% rated current)
│  │      62%           │  │
│  └────────────────────┘  │
│                          │
│  ┌────┐ ┌────┐ ┌────┐   │
│  │ 300│ │ 600│ │ 900│   │  ← Speed presets (2×3 grid, 6 max)
│  │Wood│ │Stl │ │Alum│   │     Tap: select preset
│  └────┘ └────┘ └────┘   │     Long-press: save current speed
│  ┌────┐ ┌────┐ ┌────┐   │       + open label keyboard
│  │1200│ │1500│ │    │   │     Empty slots show "+"
│  │Tap │ │Fin │ │ +  │   │
│  └────┘ └────┘ └────┘   │
│                       ⚙  │  ← Settings gear (disabled while
│                          │     inverter is on)
└──────────────────────────┘
```

**Main screen elements (top to bottom):**

| # | Element | Widget | Interaction | Notes |
|---|---------|--------|-------------|-------|
| 1 | Status bar | Label row | Read-only | System state (READY / RUNNING / OVERLOAD / FAULT) + Modbus comms icon (●/✕) |
| 2 | Direction indicator | Toggle button | Tap to switch FWD ↔ REV | Disabled (greyed, no tap) while inverter is on |
| 3 | Actual RPM | Large label | Read-only | Biggest element on screen. 5 Hz update rate |
| 4 | Setpoint RPM | Label + tap zone | Tap opens numpad; encoder adjusts freely | Smaller than actual RPM. Visible "tap to edit" affordance |
| 5 | Load bar | Progress bar | Read-only | Full width. Green < 60%, yellow 60–80%, red > 80%. Percentage label inside. On overload: bar turns red and flashes 3× rapidly |
| 6 | Preset grid | Button grid 2×3 | Tap: select. Long-press: save + label | Each shows RPM (top) + label (bottom). 6 slots max. Empty = "+" |
| 7 | Settings gear | Icon button | Tap opens settings | Disabled while inverter is on |

### Numpad Overlay

Modal overlay opened by tapping the setpoint RPM on the main screen.

| Element | Widget | Notes |
|---------|--------|-------|
| Current value | Label | Shows digits as entered |
| Number keys 0–9 | Button grid 4×3 | Large, glove-friendly targets |
| Backspace | Button | Clears last digit |
| OK | Button | Accepts if within MIN–MAX RPM, closes overlay |
| Cancel | Button | Discards entry, closes overlay |
| Range indicator | Label | Shows "200–1800 RPM" |

### Preset Label Keyboard

Modal overlay opened by long-pressing a preset slot.

| Element | Widget | Notes |
|---------|--------|-------|
| Text field | Label + cursor | Current label, max ~6 chars (must fit on preset button) |
| Keyboard | LVGL keyboard | Alpha-only, no symbols needed |
| OK / Cancel | Buttons | Save or discard label |

The current setpoint RPM is saved to the slot when the long-press is detected (before the keyboard opens). The keyboard is only for naming.

### Settings Screen

Full-screen replacement of main screen. **Disabled while inverter is on** — gear icon is greyed out and non-interactive when the drive is enabled.

| Element | Widget | Notes |
|---------|--------|-------|
| Back button | Arrow / button | Returns to main screen |
| **PID Tuning** | | |
| Kp | Spinbox with +/- | Proportional gain |
| Ki | Spinbox with +/- | Integral gain |
| Kd | Spinbox with +/- | Derivative gain |
| **Ramp Times** | | |
| Ramp Up | Spinbox (seconds) | Acceleration ramp |
| Ramp Down | Spinbox (seconds) | Deceleration ramp |
| **Hardware** | | |
| Tach PPR | Dropdown | Values: 1, 2, 3, 4 |
| Direction swap | Toggle switch | Inverts FWD/REV mapping |
| **Presets Editor** | List | Tap to rename, reorder via up/down buttons |

### Fault Overlay

Modal overlay that appears over the main screen when the VFD reports a fault.

| Element | Widget | Notes |
|---------|--------|-------|
| Fault banner | Large label, red background | Covers top portion of screen |
| Fault description | Label | Plain English (e.g. "OVERCURRENT", not raw Pr codes) |
| Instructions | Label | "Press STOP to clear" |

Modbus communication loss is also shown as a fault.

### Physical Control Mapping

| Control | Function |
|---------|----------|
| Rotary encoder turn | Adjust setpoint RPM (free dial, always active) |
| Encoder pushbutton | Reserved for cycle applications (tapping mode — future) |
| Start button | Enable drive (physical only, not mirrored in UI) |
| Stop button | Disable drive (physical only, not mirrored in UI) |

### Overload Behavior

When overload is detected (current > 90% rated AND speed error > 15%):
- Status bar changes to OVERLOAD
- Load bar turns solid red and **flashes 3× rapidly**, then holds red
- System reduces output by 50%
- Clears automatically when current drops below 70% rated

### Future UI Features

- [ ] Depth indicator display (requires depth sensor hardware)
- [ ] Beeper/speaker alerts (approach speed, overload warning, fault)
- [ ] Tapping mode — encoder pushbutton triggers reverse at set depth
- [ ] Belt/pulley range advisor (suggest sheave position for target speed range)
- [ ] Rolling load trend graph
- [ ] Tool/material speed lookup

---

### Mechanical Safety

1. **Motor/Spindle**
   - Verify motor mounting is secure
   - Check belt tension regularly
   - Guard all rotating parts
   - Never exceed maximum rated speed

2. **Drilling Operation**
   - Use proper speeds for material/bit size
   - Don't exceed recommended drilling pressures
   - Monitor for unusual vibration or noise

3. **Maintenance**
   - Lock out/tag out before maintenance
   - Verify zero voltage before working on system
   - Follow manufacturer maintenance schedules

### Operational Safety

1. **Overload Protection**
   - System monitors motor current
   - Automatic reduction at 90% current limit
   - Manual reset required after overload

2. **Speed Limits**
   - Software limits prevent overspeed
   - Pr 02 sets absolute maximum frequency
   - Verify limits appropriate for application

3. **Fault Handling**
   - System stops on communication loss
   - Drive trips displayed immediately
   - Manual reset required after fault

---

## Future Enhancements

### Hardware Additions
- [ ] Depth sensor + quill position display
- [ ] Beeper/speaker for alerts (approach speed, overload, fault)
- [ ] Belt/pulley range advisor (suggest sheave position for target speed range)

### Planned Features
- [ ] Tapping mode (auto-reverse at set depth, requires depth sensor)
- [ ] Data logging to SD card
- [ ] Rolling load trend graph
- [ ] Material-specific speed recommendations
- [ ] Tool library with optimal speeds

### Advanced Control
- [ ] Feedforward control for faster response
- [ ] Adaptive PID tuning
- [ ] Current limiting for tool protection
- [ ] Torque-based control mode
- [ ] Vibration monitoring (using accelerometer)

### Connectivity
- [ ] WiFi monitoring/control interface
- [ ] MQTT for remote monitoring
- [ ] Web interface for configuration
- [ ] Integration with shop automation system
- [ ] Tool usage tracking
