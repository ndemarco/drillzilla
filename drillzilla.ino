/*
 * DRILLZILLA - Nicky's Monster Drill Press Controller
 * When your drill press needs a PID loop, VFD, and a touchscreen
 * 
 * Hardware:
 * - ESP32 CYD (Cheap Yellow Display)
 * - Nidec M200-022-00075A VFD
 * - Marathon SKV145THTR5329AA 2HP 4-pole motor
 * - Single pulse tachometer (1-4 PPR)
 * - Grayhill 62A22-02-045CH encoder for setpoint
 * - MAX485 RS485 interface
 * 
 * Author: NickyDoes
 * Project: DRILLZILLA
 * Date: 2024-11-12
 */

#include <ModbusMaster.h>
#include <TFT_eSPI.h>
#include "driver/pcnt.h"

// ============= HARDWARE PINS =============
#define TACH_PIN 34           // Digital tach input (PCNT capable pin)
#define RS485_TX 17           // RS485 TX
#define RS485_RX 16           // RS485 RX
#define RS485_DE_RE 4         // RS485 Direction control

// Grayhill encoder pins
#define ENCODER_A 32          // Quadrature channel A
#define ENCODER_B 33          // Quadrature channel B  
#define ENCODER_BTN 25        // Push button

// Control buttons and status LED
#define BTN_ON 26             // ON button input
#define BTN_OFF 27            // OFF button input
#define LED_STATUS 23         // Status LED output

// ============= TACH CONFIGURATION =============
#define TACH_PPR 1            // Pulses per revolution (1-4 selectable on tach)
#define PCNT_UNIT PCNT_UNIT_0
#define SAMPLE_PERIOD_MS 500  // Calculate RPM every 500ms (longer for low PPR)

// ============= ENCODER CONFIGURATION =============
#define RPM_STEP 50           // RPM change per encoder click
#define MIN_RPM 200           // Minimum setpoint RPM
#define MAX_RPM 1800          // Maximum setpoint RPM (synchronous speed)

volatile int32_t encoder_count = 0;
volatile bool button_pressed = false;
unsigned long last_button_time = 0;
const unsigned long BUTTON_DEBOUNCE = 200; // ms

// ============= CONTROL BUTTONS =============
bool system_enabled = false;     // Master enable flag
bool last_on_state = HIGH;       // ON button state (active LOW with pullup)
bool last_off_state = HIGH;      // OFF button state (active LOW with pullup)
unsigned long last_on_press = 0;
unsigned long last_off_press = 0;

// ============= STATUS LED =============
unsigned long last_led_toggle = 0;
bool led_state = false;
const unsigned long LED_FLASH_FAST = 100;  // 100ms = rapid flash when not at speed

// ============= PID CONFIGURATION =============
float Kp = 2.5;               // Proportional gain
float Ki = 0.8;               // Integral gain  
float Kd = 0.05;              // Derivative gain

float setpoint_rpm = 600;     // Default starting speed
float actual_rpm = 0;
float error = 0;
float last_error = 0;
float integral = 0;
float derivative = 0;
float output_hz = 0;

const float MAX_INTEGRAL = 500.0;  // Anti-windup limit
const float MAX_SPEED_HZ = 60.0;   // Maximum frequency
const float MIN_SPEED_HZ = 5.0;    // Minimum useful speed

// ============= MOTOR CONFIGURATION =============
#define MOTOR_POLES 4         // Marathon SKV145THTR5329AA is 4-pole
#define MOTOR_BASE_FREQ 60.0  // Motor rated frequency
const float MOTOR_BASE_RPM = (120.0 * MOTOR_BASE_FREQ) / MOTOR_POLES; // 1800 RPM
const float CURRENT_LIMIT = 7.5;  // Marathon motor rated current @ 230V

// ============= MODBUS =============
ModbusMaster node;
HardwareSerial RS485Serial(2);
#define MODBUS_ADDRESS 1

// ============= DISPLAY =============
TFT_eSPI tft = TFT_eSPI();

// ============= TACH VARIABLES =============
volatile int16_t pulse_count = 0;
unsigned long last_sample_time = 0;
float rpm_filtered = 0;
const float RPM_FILTER_ALPHA = 0.2; // More filtering for low PPR

// ============= SYSTEM STATE =============
enum SystemState {
  STOPPED,
  RUNNING,
  OVERLOAD,
  FAULT
};
SystemState state = STOPPED;

float motor_current = 0;
float motor_frequency = 0;

// ============= PRESET SPEEDS =============
const int NUM_PRESETS = 5;
int current_preset = 0;
int preset_speeds[NUM_PRESETS] = {300, 600, 900, 1200, 1500}; // RPM


// =============================================
// Encoder Interrupt Handlers
// =============================================
void IRAM_ATTR encoderISR() {
  static uint8_t last_state = 0;
  uint8_t a = digitalRead(ENCODER_A);
  uint8_t b = digitalRead(ENCODER_B);
  uint8_t current_state = (a << 1) | b;
  
  // Gray code decoding
  uint8_t combined = (last_state << 2) | current_state;
  
  // Forward: 0b0001, 0b0111, 0b1110, 0b1000
  // Reverse: 0b0010, 0b1011, 0b1101, 0b0100
  if (combined == 0b0001 || combined == 0b0111 || 
      combined == 0b1110 || combined == 0b1000) {
    encoder_count++;
  } else if (combined == 0b0010 || combined == 0b1011 || 
             combined == 0b1101 || combined == 0b0100) {
    encoder_count--;
  }
  
  last_state = current_state;
}

void IRAM_ATTR buttonISR() {
  unsigned long current_time = millis();
  if (current_time - last_button_time > BUTTON_DEBOUNCE) {
    button_pressed = true;
    last_button_time = current_time;
  }
}

// =============================================
// Encoder Setup
// =============================================
void setupEncoder() {
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_BTN), buttonISR, FALLING);
}

// =============================================
// Control Buttons and LED Setup
// =============================================
void setupControls() {
  pinMode(BTN_ON, INPUT_PULLUP);
  pinMode(BTN_OFF, INPUT_PULLUP);
  pinMode(LED_STATUS, OUTPUT);
  
  digitalWrite(LED_STATUS, LOW); // LED off initially
  
  Serial.println("Control buttons and status LED initialized");
}

// =============================================
// PCNT (Pulse Counter) Setup
// =============================================
void setupPCNT() {
  pcnt_config_t pcnt_config = {
    .pulse_gpio_num = TACH_PIN,
    .ctrl_gpio_num = PCNT_PIN_NOT_USED,
    .lctrl_mode = PCNT_MODE_KEEP,
    .hctrl_mode = PCNT_MODE_KEEP,
    .pos_mode = PCNT_COUNT_INC,      // Count rising edges
    .neg_mode = PCNT_COUNT_DIS,      // Don't count falling edges
    .counter_h_lim = 32767,
    .counter_l_lim = -32768,
    .unit = PCNT_UNIT,
    .channel = PCNT_CHANNEL_0,
  };

  pcnt_unit_config(&pcnt_config);
  
  // Set filter to ignore glitches < 10us
  pcnt_set_filter_value(PCNT_UNIT, 1000);
  pcnt_filter_enable(PCNT_UNIT);
  
  pcnt_counter_pause(PCNT_UNIT);
  pcnt_counter_clear(PCNT_UNIT);
  pcnt_counter_resume(PCNT_UNIT);
}

// =============================================
// Read RPM from PCNT
// =============================================
float readRPM() {
  unsigned long current_time = millis();
  unsigned long elapsed = current_time - last_sample_time;
  
  if (elapsed >= SAMPLE_PERIOD_MS) {
    // Read pulse count
    int16_t count;
    pcnt_get_counter_value(PCNT_UNIT, &count);
    
    // Calculate RPM
    // RPM = (pulses / PPR) * (60000 / elapsed_ms)
    float raw_rpm = ((float)count / TACH_PPR) * (60000.0 / elapsed);
    
    // Apply low-pass filter for smooth display
    if (rpm_filtered == 0) {
      rpm_filtered = raw_rpm; // Initialize on first reading
    } else {
      rpm_filtered = (RPM_FILTER_ALPHA * raw_rpm) + ((1.0 - RPM_FILTER_ALPHA) * rpm_filtered);
    }
    
    // Reset counter
    pcnt_counter_clear(PCNT_UNIT);
    last_sample_time = current_time;
    
    return rpm_filtered;
  }
  
  return rpm_filtered; // Return last filtered value
}

// =============================================
// Modbus RS485 Setup
// =============================================
void setupModbus() {
  pinMode(RS485_DE_RE, OUTPUT);
  digitalWrite(RS485_DE_RE, LOW);
  
  RS485Serial.begin(38400, SERIAL_8N1, RS485_RX, RS485_TX);
  node.begin(MODBUS_ADDRESS, RS485Serial);
  
  // Callbacks for RS485 direction control
  node.preTransmission([]() { digitalWrite(RS485_DE_RE, HIGH); });
  node.postTransmission([]() { digitalWrite(RS485_DE_RE, LOW); });
  
  Serial.println("Modbus initialized: 38400 baud, address 1");
}

// =============================================
// Write Frequency Command to M200
// =============================================
bool setDriveFrequency(float hz) {
  // M200 expects frequency in 0.01 Hz units
  uint16_t freq_scaled = (uint16_t)(hz * 100.0);
  uint8_t result = node.writeSingleRegister(0x0001, freq_scaled); // Pr 01
  
  if (result != node.ku8MBSuccess) {
    Serial.print("Modbus write error: ");
    Serial.println(result);
  }
  
  return (result == node.ku8MBSuccess);
}

// =============================================
// Read Motor Current from M200
// =============================================
float readMotorCurrent() {
  uint8_t result = node.readHoldingRegisters(0x0058, 1); // Pr 88 - Current Magnitude
  
  if (result == node.ku8MBSuccess) {
    uint16_t raw = node.getResponseBuffer(0);
    return raw / 100.0; // M200 returns current in 0.01A units
  }
  
  return motor_current; // Return last known value on error
}

// =============================================
// Read Output Frequency from M200
// =============================================
float readMotorFrequency() {
  uint8_t result = node.readHoldingRegisters(0x0055, 1); // Pr 85 - Output Frequency
  
  if (result == node.ku8MBSuccess) {
    uint16_t raw = node.getResponseBuffer(0);
    return raw / 100.0; // M200 returns frequency in 0.01 Hz units
  }
  
  return motor_frequency;
}

// =============================================
// PID Controller
// =============================================
float computePID(float setpoint, float measured, float dt) {
  error = setpoint - measured;
  
  // Proportional
  float P = Kp * error;
  
  // Integral with anti-windup
  integral += error * dt;
  integral = constrain(integral, -MAX_INTEGRAL, MAX_INTEGRAL);
  float I = Ki * integral;
  
  // Derivative (with filtering to reduce noise)
  derivative = (error - last_error) / dt;
  float D = Kd * derivative;
  
  last_error = error;
  
  // Compute output
  float output = P + I + D;
  
  // Convert RPM to Hz for the motor
  // Hz = (RPM * poles) / 120
  float output_rpm = output;
  float output_hz = (output_rpm * MOTOR_POLES) / 120.0;
  
  // Clamp output
  output_hz = constrain(output_hz, 0, MAX_SPEED_HZ);
  
  // Reset integral if saturated (anti-windup)
  if (output_hz >= MAX_SPEED_HZ || output_hz <= 0) {
    integral *= 0.9; // Decay integral when saturated
  }
  
  return output_hz;
}

// =============================================
// Read Setpoint from Encoder
// =============================================
float readSetpoint() {
  static int32_t last_encoder_count = 0;
  
  // Check if encoder has moved
  if (encoder_count != last_encoder_count) {
    int32_t delta = encoder_count - last_encoder_count;
    setpoint_rpm += delta * RPM_STEP;
    
    // Constrain to limits
    setpoint_rpm = constrain(setpoint_rpm, MIN_RPM, MAX_RPM);
    
    last_encoder_count = encoder_count;
    
    Serial.print("Setpoint changed: ");
    Serial.print(setpoint_rpm);
    Serial.println(" RPM");
  }
  
  // Check if button was pressed (cycle through presets)
  if (button_pressed) {
    button_pressed = false;
    
    current_preset = (current_preset + 1) % NUM_PRESETS;
    setpoint_rpm = preset_speeds[current_preset];
    
    Serial.print("Preset selected: ");
    Serial.print(current_preset);
    Serial.print(" = ");
    Serial.print(setpoint_rpm);
    Serial.println(" RPM");
  }
  
  return setpoint_rpm;
}

// =============================================
// Read ON/OFF Control Buttons
// =============================================
void readControlButtons() {
  unsigned long current_time = millis();
  
  // Read ON button (active LOW with pullup)
  bool on_state = digitalRead(BTN_ON);
  if (on_state == LOW && last_on_state == HIGH) {
    // Button pressed (HIGH to LOW transition)
    if (current_time - last_on_press > BUTTON_DEBOUNCE) {
      system_enabled = true;
      Serial.println(">>> ON BUTTON PRESSED - System Enabled");
      last_on_press = current_time;
    }
  }
  last_on_state = on_state;
  
  // Read OFF button (active LOW with pullup)
  bool off_state = digitalRead(BTN_OFF);
  if (off_state == LOW && last_off_state == HIGH) {
    // Button pressed (HIGH to LOW transition)
    if (current_time - last_off_press > BUTTON_DEBOUNCE) {
      system_enabled = false;
      integral = 0; // Reset integral when stopping
      Serial.println(">>> OFF BUTTON PRESSED - System Disabled");
      last_off_press = current_time;
    }
  }
  last_off_state = off_state;
}

// =============================================
// Update Status LED
// =============================================
void updateStatusLED() {
  unsigned long current_time = millis();
  
  if (!system_enabled) {
    // System disabled - LED off
    digitalWrite(LED_STATUS, LOW);
    return;
  }
  
  // Check if at speed (within 10% of setpoint)
  float speed_error_pct = 0;
  if (setpoint_rpm > 0) {
    speed_error_pct = (abs(error) / setpoint_rpm) * 100.0;
  }
  
  bool at_speed = (speed_error_pct < 10.0) && (state == RUNNING);
  
  if (at_speed) {
    // At speed - LED solid ON
    digitalWrite(LED_STATUS, HIGH);
  } else {
    // Not at speed - LED rapid flash (100ms period = 5 Hz)
    if (current_time - last_led_toggle >= LED_FLASH_FAST) {
      led_state = !led_state;
      digitalWrite(LED_STATUS, led_state);
      last_led_toggle = current_time;
    }
  }
}

// =============================================
// Display Update
// =============================================
void updateDisplay() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  
  // Title
  tft.setCursor(10, 10);
  tft.println("DRILL PRESS CONTROL");
  
  // Setpoint
  tft.setCursor(10, 50);
  tft.print("Target: ");
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.print((int)setpoint_rpm);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println(" RPM");
  
  // Actual Speed
  tft.setCursor(10, 80);
  tft.print("Actual: ");
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.print((int)actual_rpm);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println(" RPM");
  
  // Speed error
  float speed_error_pct = 0;
  if (setpoint_rpm > 0) {
    speed_error_pct = (abs(error) / setpoint_rpm) * 100.0;
  }
  tft.setCursor(10, 110);
  tft.print("Error:  ");
  tft.setTextColor((speed_error_pct > 10) ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
  tft.print(speed_error_pct, 1);
  tft.println(" %");
  
  // Motor Current
  tft.setCursor(10, 140);
  tft.setTextColor((motor_current > CURRENT_LIMIT * 0.8) ? TFT_RED : TFT_CYAN, TFT_BLACK);
  tft.print("Current:");
  tft.print(motor_current, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println(" A");
  
  // Load bar graph
  int load_pct = (motor_current / CURRENT_LIMIT) * 100;
  load_pct = constrain(load_pct, 0, 100);
  
  tft.setCursor(10, 170);
  tft.print("Load: ");
  tft.print(load_pct);
  tft.println(" %");
  
  // Draw bar graph
  int bar_width = map(load_pct, 0, 100, 0, 200);
  uint16_t bar_color = (load_pct > 80) ? TFT_RED : (load_pct > 60) ? TFT_YELLOW : TFT_GREEN;
  tft.fillRect(10, 195, bar_width, 20, bar_color);
  tft.drawRect(10, 195, 200, 20, TFT_WHITE);
  
  // State & Preset indicator
  tft.setCursor(10, 230);
  
  if (!system_enabled) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.print("DISABLED");
  } else {
    switch(state) {
      case STOPPED:
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print("STOPPED");
        break;
      case RUNNING:
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.print("RUNNING");
        break;
      case OVERLOAD:
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.print("OVERLOAD!");
        break;
      case FAULT:
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.print("FAULT");
        break;
    }
  }
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print(" P");
  tft.print(current_preset + 1);
}

// =============================================
// Overload Detection
// =============================================
void checkOverload() {
  // Detect stall: high current + large speed error
  if (motor_current > CURRENT_LIMIT * 0.9 && abs(error) > setpoint_rpm * 0.15) {
    state = OVERLOAD;
    
    // Reduce output to prevent damage
    output_hz *= 0.5;
    setDriveFrequency(output_hz);
    
    // Reset integral to prevent windup
    integral = 0;
    
    Serial.println("OVERLOAD DETECTED!");
  } else if (state == OVERLOAD && motor_current < CURRENT_LIMIT * 0.7) {
    // Clear overload if current drops
    state = RUNNING;
    Serial.println("Overload cleared");
  }
}

// =============================================
// Setup
// =============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=================================");
  Serial.println("Drill Press Controller Starting");
  Serial.println("=================================");
  
  // Setup hardware
  Serial.println("Initializing PCNT...");
  setupPCNT();
  
  Serial.println("Initializing Encoder...");
  setupEncoder();
  
  Serial.println("Initializing Controls...");
  setupControls();
  
  Serial.println("Initializing Modbus...");
  setupModbus();
  
  // Setup display
  Serial.println("Initializing Display...");
  tft.init();
  tft.setRotation(1); // Landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  tft.setCursor(20, 100);
  tft.println("Initializing...");
  
  delay(2000);
  
  Serial.println("System Ready");
  Serial.println("=================================\n");
  
  state = STOPPED;
  last_sample_time = millis();
}

// =============================================
// Main Loop
// =============================================
void loop() {
  static unsigned long last_control_update = 0;
  static unsigned long last_display_update = 0;
  static unsigned long last_modbus_read = 0;
  static unsigned long last_serial_print = 0;
  static unsigned long last_button_check = 0;
  
  unsigned long current_time = millis();
  
  // Check ON/OFF buttons - 20 Hz (50ms)
  if (current_time - last_button_check >= 50) {
    last_button_check = current_time;
    readControlButtons();
    updateStatusLED();
  }
  
  // Control loop - 50 Hz (20ms)
  if (current_time - last_control_update >= 20) {
    last_control_update = current_time;
    
    // Read tachometer
    actual_rpm = readRPM();
    
    // Read setpoint from encoder
    readSetpoint();
    
    // Only run PID if system is enabled AND we have a valid setpoint
    if (system_enabled && setpoint_rpm > MIN_SPEED_HZ * 120.0 / MOTOR_POLES) {
      if (state != OVERLOAD) {
        state = RUNNING;
      }
      
      // Compute PID
      float dt = 0.02; // 20ms = 0.02s
      output_hz = computePID(setpoint_rpm, actual_rpm, dt);
      
      // Send to drive
      setDriveFrequency(output_hz);
      
    } else {
      // System disabled or setpoint too low
      state = STOPPED;
      output_hz = 0;
      integral = 0; // Reset integral when stopped
      setDriveFrequency(0);
    }
  }
  
  // Modbus read loop - 10 Hz (100ms)
  if (current_time - last_modbus_read >= 100) {
    last_modbus_read = current_time;
    
    motor_current = readMotorCurrent();
    motor_frequency = readMotorFrequency();
    
    // Check for overload only when running
    if (state == RUNNING && system_enabled) {
      checkOverload();
    }
  }
  
  // Display update - 5 Hz (200ms)
  if (current_time - last_display_update >= 200) {
    last_display_update = current_time;
    updateDisplay();
  }
  
  // Debug output - 1 Hz (1000ms)
  if (current_time - last_serial_print >= 1000) {
    last_serial_print = current_time;
    
    Serial.print("EN:");
    Serial.print(system_enabled ? "YES" : "NO ");
    Serial.print(" | SP:");
    Serial.print((int)setpoint_rpm);
    Serial.print(" | ACT:");
    Serial.print((int)actual_rpm);
    Serial.print(" | ERR:");
    Serial.print((int)error);
    Serial.print(" | OUT:");
    Serial.print(output_hz, 2);
    Serial.print("Hz | I:");
    Serial.print(motor_current, 2);
    Serial.print("A | St:");
    Serial.println(state);
  }
}
