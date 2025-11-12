# Drillzilla: Nicky's Monster Drill Press Controller

**When your drill press needs a PID loop, VFD, and a touchscreen**

---

## Project Overview

A closed-loop speed control system for a drill press using:
- **VFD**: Nidec M200 (Size 2) variable frequency drive for 2HP 3-phase motor
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
│  │   2.8" TFT Display           │  │
│  │   - Setpoint RPM             │  │
│  │   - Actual RPM               │  │
│  │   - Speed error %            │  │
│  │   - Motor current (A)        │  │
│  │   - Load bar graph           │  │
│  │   - System state             │  │
│  │   - Enable/Disable status    │  │
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
| Controller | ESP32 CYD | PID + Display | 2.8" TFT, WiFi capable |
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

## Software Configuration

### M200 Drive Parameters

#### Basic Setup
```
Pr 05 (Drive Configuration) = 0 (AV) or 1 (AI)
Pr 06 (Motor Rated Current) = 7.5 A (Marathon SKV145THTR5329AA @ 230V)
Pr 07 (Motor Rated Speed) = 1750 RPM (full load speed)
Pr 08 (Motor Rated Voltage) = 230 V (for 230V connection)
Pr 39 (Motor Rated Frequency) = 60 Hz
Pr 40 (Number of Motor Poles) = 4 (or Auto)
```

**Marathon Motor Nameplate Data:**
- Model: SKV145THTR5329AA
- HP: 2.0
- Poles: 4
- RPM: 1750 (full load)
- Voltage: 230/460V (use 230V with M200-022)
- Current: 7.5A @ 230V, 3.75A @ 460V
- Frequency: 60 Hz
- Service Factor: 1.0

#### Modbus Communication
```
Pr 43 (Serial Baud Rate) = 7 (38400 baud)
Pr 44 (Serial Address) = 1
Pr 11.024 (Serial Mode) = 1 (8-1-NP: 8 data bits, 1 stop, no parity)
Pr 45 (Reset Serial Comms) = 1 (activate changes, then resets to 0)
```

#### Control Mode
```
Pr 79 (User Drive Mode) = 1 (Open Loop) 
Pr 41 (Control Mode) = 4 (Ur.I - Open loop vector with current control)
Pr 31 (Stop Mode) = 1 (rp - ramp to stop)
```

#### Ramp Settings
```
Pr 03 (Acceleration Rate 1) = 5.0 s/100Hz
Pr 04 (Deceleration Rate 1) = 10.0 s/100Hz
Pr 28 (Ramp Mode Select) = 1 (Standard)
```

#### Protection
```
Pr 01 (Minimum Speed) = 5.0 Hz
Pr 02 (Maximum Speed) = 60.0 Hz
Pr 37 (Maximum Switching Frequency) = 3 kHz (or higher for quieter operation)
```

**Note**: After changing parameters, execute a parameter save:
1. Set Pr 00 = 1 (Save) or enter value 1001
2. Press red reset button or send reset command via Modbus

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

## Modbus Communication

### Modbus RTU Configuration
- **Protocol**: Modbus RTU (binary)
- **Baud Rate**: 38400 bps
- **Data Format**: 8 data bits, No parity, 1 stop bit (8-N-1)
- **Slave Address**: 1 (configurable via Pr 44)
- **Update Rate**: 10 Hz for monitoring, 50 Hz for control

### Key Modbus Registers (M200)

#### Read Registers (Function Code 0x03)
| Register | Parameter | Description | Units | Scale |
|----------|-----------|-------------|-------|-------|
| 0x0001 | Pr 01 | Minimum Speed | Hz | ×100 |
| 0x0002 | Pr 02 | Maximum Speed | Hz | ×100 |
| 0x0054 | Pr 84 | DC Bus Voltage | V | ×1 |
| 0x0055 | Pr 85 | Output Frequency | Hz | ×100 |
| 0x0056 | Pr 86 | Output Voltage | V | ×1 |
| 0x0057 | Pr 87 | Motor RPM | RPM | ×1 |
| 0x0058 | Pr 88 | Current Magnitude | A | ×100 |
| 0x0059 | Pr 89 | Torque Producing Current | A | ×100 |

**Example**: Reading motor current (Pr 88)
- Raw value: 850
- Actual current: 850 / 100 = 8.50 A

#### Write Registers (Function Code 0x06)
| Register | Parameter | Description | Units | Scale |
|----------|-----------|-------------|-------|-------|
| 0x0001 | Pr 01 | Frequency Reference* | Hz | ×100 |
| 0x006E | Pr 110 | Direct Frequency Ref | Hz | ×100 |

*Actual parameter depends on Drive Configuration setting

**Example**: Writing 45.5 Hz frequency command
- Desired frequency: 45.5 Hz
- Scaled value: 45.5 × 100 = 4550
- Modbus command: Write 4550 to register 0x0001

#### Control Registers
| Register | Parameter | Description | Values |
|----------|-----------|-------------|--------|
| 0x0006 | Pr 06.015 | Drive Enable | 0=Disabled, 1=Enabled |
| 0x0000 | Pr 00 | System Commands | See command table below |

**Pr 00 Command Values**:
- 0: No action
- 1001: Save parameters
- 1233: Load 50 Hz defaults
- 1244: Load 60 Hz defaults

### Modbus Communication Error Handling

#### Timeouts
```cpp
const unsigned long MODBUS_TIMEOUT = 500;  // 500ms timeout
const uint8_t MAX_RETRIES = 3;

uint8_t result = node.readHoldingRegisters(register, count);
if (result != node.ku8MBSuccess) {
  // Handle error: retry, use last known value, or fault
}
```

#### Common Error Codes
- `ku8MBSuccess` (0x00): Success
- `ku8MBIllegalFunction` (0x01): Invalid function code
- `ku8MBIllegalDataAddress` (0x02): Invalid register address
- `ku8MBIllegalDataValue` (0x03): Invalid data value
- `ku8MBSlaveDeviceFailure` (0x04): Slave device fault
- Timeout: No response from device

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

---

## Troubleshooting

### Speed Control Issues

| Symptom | Possible Cause | Solution |
|---------|----------------|----------|
| Speed oscillates | Kp too high | Reduce Kp by 25% |
| Slow response to changes | Kp too low | Increase Kp by 50% |
| Speed droop under load | Ki too low | Increase Ki by 50% |
| Slow oscillation | Ki too high | Reduce Ki by 25% |
| Overshoot on start | Kp too high, no Kd | Reduce Kp or add small Kd |
| No speed control | Tach not connected | Check PCNT is counting pulses |
| Erratic speed reading | Noise on tach signal | Add filter capacitor, check shielding |
| Speed limit not working | MAX_SPEED_HZ too high | Verify Pr 02 and MAX_SPEED_HZ match |

### Communication Issues

| Symptom | Possible Cause | Solution |
|---------|----------------|----------|
| No Modbus communication | Wrong baud rate | Verify Pr 43 = 38400 baud |
| | Wrong address | Verify Pr 44 matches code |
| | Wiring reversed | Swap A and B lines |
| | No termination | Add 120Ω resistor at M200 |
| Intermittent communication | Noise on RS485 | Use shielded twisted pair |
| | Ground loop | Ground shield at one end only |
| Communication errors | Long cable run | Reduce baud rate to 19200 |
| Display shows old data | Modbus timeout | Check error handling, retry logic |

### Tachometer Issues

| Symptom | Possible Cause | Solution |
|---------|----------------|----------|
| RPM reads zero | No signal | Check tach power, wiring |
| | Wrong GPIO | Verify TACH_PIN = 34 |
| | PCNT not configured | Check setupPCNT() called |
| RPM too high | Wrong PPR setting | Verify TACH_PPR matches tach spec |
| RPM too low | Wrong PPR setting | Verify TACH_PPR matches tach spec |
| | Counting both edges | Change to COUNT_INC only |
| Noisy RPM reading | Electrical noise | Add 100pF cap, use shielded cable |
| | Wrong filter setting | Adjust RPM_FILTER_ALPHA |
| RPM not updating | Sample period too long | Reduce SAMPLE_PERIOD_MS |

### Drive Issues

| Symptom | Possible Cause | Solution |
|---------|----------------|----------|
| Drive won't enable | Terminal 11 not high | Check enable signal wiring |
| | Drive tripped | Read Pr 56-58 for trip code |
| | Undervoltage | Check input power |
| Motor won't run | No run command | Check Terminal 12/13 |
| | Frequency = 0 | Verify Modbus write successful |
| Motor overheats | Current too high | Verify Pr 06 = motor nameplate |
| | Low speed operation | Check ventilation, use forced fan |
| Excessive current | Mechanical binding | Check spindle/bearings |
| | Wrong motor parameters | Verify Pr 06, 07, 08 match motor |
| Drive overheats | Switching freq too high | Reduce Pr 37 |
| | Poor ventilation | Clean filters, check fans |

### Display Issues

| Symptom | Possible Cause | Solution |
|---------|----------------|----------|
| Display blank | No power | Check USB power |
| | TFT not initialized | Verify TFT_eSPI configuration |
| Display frozen | Code crashed | Check Serial for errors, reboot |
| Wrong values displayed | Unit conversion error | Verify scaling factors |
| Display flickers | Update rate too fast | Increase display update period |

---

## Safety Considerations

### Electrical Safety

1. **Line Power**
   - Use properly rated circuit breaker for M200 input
   - Follow NEC/local electrical codes
   - Ground drive chassis properly

2. **Control Wiring**
   - Separate control wiring from power wiring
   - Use appropriate wire gauge for current loads
   - Secure all connections with proper terminals

3. **Emergency Stop**
   - Hardwire E-stop to break drive enable (Terminal 11)
   - E-stop must be easily accessible
   - Use normally-closed E-stop button
   - Test E-stop function regularly

4. **Enclosure**
   - Use NEMA-rated enclosure appropriate for environment
   - Ensure proper ventilation for drive cooling
   - Protect from moisture, dust, metal chips

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

### Planned Features
- [ ] Touchscreen speed setpoint entry
- [ ] Preset speed memory (common drilling speeds)
- [ ] Data logging to SD card
- [ ] WiFi monitoring/control interface
- [ ] Automatic feed rate control
- [ ] Tap cycle automation (reverse after depth)
- [ ] Material-specific speed recommendations
- [ ] Tool library with optimal speeds

### Advanced Control
- [ ] Feedforward control for faster response
- [ ] Adaptive PID tuning
- [ ] Current limiting for tool protection
- [ ] Torque-based control mode
- [ ] Vibration monitoring (using accelerometer)

### Connectivity
- [ ] MQTT for remote monitoring
- [ ] Web interface for configuration
- [ ] Integration with shop automation system
- [ ] Tool usage tracking

---

## Bill of Materials

### Electronics

| Item | Part Number | Qty | Est. Cost | Source | Notes |
|------|-------------|-----|-----------|--------|-------|
| VFD | M200-022-00075A | 1 | $400-600 | Nidec dealer | 2HP, 230V, 7.5A |
| Modbus Interface | AI-485 Adaptor | 1 | $100-150 | Nidec dealer | For M200 |
| Controller | ESP32 CYD | 1 | $15-25 | AliExpress | 2.8" display included |
| RS485 Module | MAX485/MAX3485 | 1 | $2-5 | Amazon/eBay | TTL to RS485 |
| Speed Control | Grayhill 62A22-02-045CH | 1 | $15-25 | Mouser/Digikey | Encoder with button |
| ON Button | Momentary SPST NO | 1 | $3-8 | Amazon | 16mm panel mount |
| OFF Button | Momentary SPST NO | 1 | $3-8 | Amazon | 16mm panel mount |
| Status LED | High-brightness LED | 1 | $1-3 | Amazon | Green, with bezel |
| Resistor | 220Ω 1/4W | 1 | <$1 | Amazon | For LED current limit |
| Tachometer | Single pulse, 1-4 PPR | 1 | $50-200 | Various | Shaft mount |
| Resistors | Assorted | - | $5 | Amazon | For pullups if needed |
| Capacitors | 100pF, 0.1µF | - | $5 | Amazon | Filtering |

### Hardware

| Item | Specification | Qty | Est. Cost | Notes |
|------|---------------|-----|-----------|-------|
| Enclosure | NEMA 4X | 1 | $50-150 | For controller & display |
| Cable, RS485 | Shielded twisted pair | 10ft | $15-25 | 22AWG or better |
| Cable, Tach | Shielded | 10ft | $10-20 | Depends on tach type |
| Terminal Blocks | DIN rail mount | As needed | $10-20 | For wiring organization |
| E-Stop Button | NC, 22mm | 1 | $10-20 | Mushroom head |
| Run/Stop Buttons | NO/NC | 2 | $10 | Optional |
| DIN Rail | 35mm | 1m | $5-10 | For mounting terminals |

### Consumables

| Item | Notes |
|------|-------|
| Wire, 18AWG | For control circuits |
| Wire, 22AWG | For low-voltage signals |
| Heat shrink tubing | Various sizes |
| Cable ties | UV resistant |
| Labels | For wire identification |

**Total Estimated Cost**: $700-1200 (not including motor)

---

## Document Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2024-11-12 | Initial | Initial draft |

---

## References

### Documentation
- [Nidec M200 Control User Guide](https://www.nidec-netherlands.nl/media/3523-frequentieregelaars-unidrive-m200-m201-control-user-guide-en-iss3-0478-0351-03.pdf)
- [Nidec M200 Parameter Reference Guide](https://acim.nidec.com/drives/control-techniques/)
- [ESP32 PCNT Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/pcnt.html)
- [Modbus Protocol Specification](https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf)

### Tools
- Nidec CTSoft - Drive configuration software
- Arduino IDE / PlatformIO - ESP32 development
- Serial monitor - Modbus debugging

### Support
- Nidec Technical Support: 1-800-893-2321
- ESP32 Forums: [esp32.com](https://www.esp32.com/)

---

## Appendix A: Complete Source Code

See separate file: `drill_press_controller.ino`

The complete Arduino/PlatformIO source code includes:
- Grayhill 62A22-02-045CH encoder handling with interrupts
- Hardware pulse counting for single-pulse tachometer (1-4 PPR)
- PID control loop at 50 Hz
- Modbus RTU communication with M200 drive
- TFT display with real-time monitoring
- Overload detection and protection
- Preset speed selection via encoder button

**Key Features:**
- Interrupt-driven quadrature decoding for smooth encoder response
- Anti-windup PID with integral limiting
- Low-pass filtering for stable RPM readings at low PPR
- 5 preset speeds (300, 600, 900, 1200, 1500 RPM)
- Real-time current and load monitoring
- Comprehensive serial debug output

---

## Appendix B: Parameter Quick Reference

### Critical M200 Parameters
```
Pr 05 = 0 or 1         (Drive configuration)
Pr 06 = 7.5            (Motor rated current - 230V)
Pr 07 = 1750           (Motor rated speed - RPM)
Pr 08 = 230            (Motor rated voltage)
Pr 39 = 60             (Motor rated frequency)
Pr 40 = 4              (Motor poles)
Pr 43 = 7              (38400 baud)
Pr 44 = 1              (Modbus address)
```

### Critical ESP32 Constants
```cpp
#define TACH_PPR 1              // Start with 1 PPR
#define MOTOR_POLES 4           // Marathon 4-pole motor
const float CURRENT_LIMIT = 7.5; // Marathon rated current @ 230V
#define ENCODER_A 32            // Grayhill channel A
#define ENCODER_B 33            // Grayhill channel B
#define ENCODER_BTN 25          // Grayhill push button
```

---

## Appendix C: Wiring Diagrams

*[Detailed wiring diagrams to be added]*

---

**END OF DOCUMENT**
