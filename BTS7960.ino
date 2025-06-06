#include <Arduino.h>
#include <driver/ledc.h>   // ESP-IDF LEDC driver

// Pin definitions
static const int EN_A   = 23;
static const int EN_B   = 22;
static const int PWM_A  = 19;
static const int PWM_B  = 18;
static const int ISNS_A = 35;
static const int ISNS_B = 34;

// LEDC configuration
static const ledc_timer_t   LEDC_TIMER      = LEDC_TIMER_0;
static const ledc_mode_t    LEDC_MODE       = LEDC_HIGH_SPEED_MODE;
static const ledc_timer_bit_t LEDC_RESOLUTION = LEDC_TIMER_8_BIT;  // 8-bit (0…255)
static const int            LEDC_FREQUENCY  = 20000;              // 20 kHz

// We’ll drive PWM_A on LEDC channel 0, PWM_B on channel 1:
static const ledc_channel_t LEDC_CHANNEL_A  = LEDC_CHANNEL_0;
static const ledc_channel_t LEDC_CHANNEL_B  = LEDC_CHANNEL_1;

// Dead-time and static-friction math
#define PWM_MAX 256
float dead_time_us          = 6.8;   // µs of dead-time
float drive_volts           = 12.0;
float static_friction_volts = 0.5;
float power                 = 0.0;   // user “power” [0.0 … 1.0]

int pwm_a = 0;
int pwm_b = 0;

float us_period;
float dead_ticks;

//-----------------------------------------------------------------------------
// Configure LEDC Timer 0 for 20 kHz, 8-bit resolution
//-----------------------------------------------------------------------------
void setupLEDC() {
  // 1) LEDC timer config:
  ledc_timer_config_t ledc_timer = {};
  ledc_timer.speed_mode       = LEDC_MODE;
  ledc_timer.duty_resolution  = LEDC_RESOLUTION;
  ledc_timer.timer_num        = LEDC_TIMER;
  ledc_timer.freq_hz          = LEDC_FREQUENCY;
  ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
  ledc_timer_config(&ledc_timer);

  // 2) LEDC channel 0 → PWM_A (pin 19)
  ledc_channel_config_t channelA = {};
  channelA.channel    = LEDC_CHANNEL_A;
  channelA.duty       = 0;             // start at 0
  channelA.gpio_num   = PWM_A;
  channelA.speed_mode = LEDC_MODE;
  channelA.hpoint     = 0;
  channelA.timer_sel  = LEDC_TIMER;
  ledc_channel_config(&channelA);

  // 3) LEDC channel 1 → PWM_B (pin 18)
  ledc_channel_config_t channelB = {};
  channelB.channel    = LEDC_CHANNEL_B;
  channelB.duty       = 0;             // start at 0
  channelB.gpio_num   = PWM_B;
  channelB.speed_mode = LEDC_MODE;
  channelB.hpoint     = 0;
  channelB.timer_sel  = LEDC_TIMER;
  ledc_channel_config(&channelB);
}

//-----------------------------------------------------------------------------
// Write an 8-bit duty to the given LEDC channel
//-----------------------------------------------------------------------------
void writeDuty(ledc_channel_t channel, int duty) {
  if (duty < 0)   duty = 0;
  if (duty > 255) duty = 255;
  ledc_set_duty(LEDC_MODE, channel, duty);
  ledc_update_duty(LEDC_MODE, channel);
}

//-----------------------------------------------------------------------------
// Compute pwm_a/pwm_b from `power` and send them to LEDC
//-----------------------------------------------------------------------------
void set_vals() {
  // baseline on B to overcome static friction
  int base_b = 35 + (static_friction_volts / drive_volts) * (PWM_MAX - 1);

  // power∈[0…1] → pwm_b ∈[base_b…255]
  pwm_b = (int)(power * ((PWM_MAX - 1) - base_b) + base_b);

  // Push them to LEDC channels:
  writeDuty(LEDC_CHANNEL_A, 0);
  writeDuty(LEDC_CHANNEL_B, pwm_b);

  // (Optional) Read current-sense ADC channels:
  int isns_a_val = analogRead(ISNS_A);
  int isns_b_val = analogRead(ISNS_B);
  // Uncomment to debug:
  // Serial.print("ISNS_A = "); Serial.print(isns_a_val);
  // Serial.print("  ISNS_B = "); Serial.println(isns_b_val);
}

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("BTS7960 PWM @ 20 kHz");

  // Precompute dead_ticks for diagnostics:
  us_period  = 1000000.0 / ((float)LEDC_FREQUENCY);  // period in µs
  dead_ticks = PWM_MAX * dead_time_us / us_period;
  Serial.print("dead_ticks = ");
  Serial.println(dead_ticks);

  // Initialize LEDC hardware for PWM
  setupLEDC();

  // Configure ADC resolution for current sensors
  analogReadResolution(16);
  pinMode(ISNS_A, INPUT);
  pinMode(ISNS_B, INPUT);

  // Enable the two half-bridges
  pinMode(EN_A, OUTPUT);
  pinMode(EN_B, OUTPUT);
  digitalWrite(EN_A, HIGH);
  digitalWrite(EN_B, HIGH);

  // Start with PWM = 0
  set_vals();
}

void loop() {
  // Hold at full power
  power = 0.4;
  set_vals();

  delay(2000);
}