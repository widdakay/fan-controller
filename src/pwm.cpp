#include "pwm.h"

//-----------------------------------------------------------------------------
// Configure LEDC Timer 0 for 20 kHz, 8-bit resolution
//-----------------------------------------------------------------------------
void setupLEDC() {
  // 1) LEDC timer config:
  ledc_timer_config_t ledc_timer = {};
  ledc_timer.speed_mode = LEDC_MODE;
  ledc_timer.duty_resolution = LEDC_RESOLUTION;
  ledc_timer.timer_num = LEDC_TIMER;
  ledc_timer.freq_hz = LEDC_FREQUENCY;
  ledc_timer.clk_cfg = LEDC_AUTO_CLK;
  ledc_timer_config(&ledc_timer);

  // 2) LEDC channel 0 → PWM_A (pin 19)
  ledc_channel_config_t channelA = {};
  channelA.channel = LEDC_CHANNEL_A;
  channelA.duty = 0;  // start at 0
  channelA.gpio_num = PWM_A;
  channelA.speed_mode = LEDC_MODE;
  channelA.hpoint = 0;
  channelA.timer_sel = LEDC_TIMER;
  ledc_channel_config(&channelA);

  // 3) LEDC channel 1 → PWM_B (pin 18)
  ledc_channel_config_t channelB = {};
  channelB.channel = LEDC_CHANNEL_B;
  channelB.duty = 0;  // start at 0
  channelB.gpio_num = PWM_B;
  channelB.speed_mode = LEDC_MODE;
  channelB.hpoint = 0;
  channelB.timer_sel = LEDC_TIMER;
  ledc_channel_config(&channelB);
}

//-----------------------------------------------------------------------------
// Write an 8-bit duty to the given LEDC channel
//-----------------------------------------------------------------------------
void writeDuty(ledc_channel_t channel, int duty) {
  if (duty < 0) duty = 0;
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

  // Send to LEDC channels:
  writeDuty(LEDC_CHANNEL_A, 0);
  writeDuty(LEDC_CHANNEL_B, pwm_b);

  // (Optional) Read current-sense ADC channels:
  int isns_a_val = analogRead(ISNS_A);
  int isns_b_val = analogRead(ISNS_B);
  // Uncomment to debug:
  // Serial.print("ISNS_A = "); Serial.print(isns_a_val);
  // Serial.print("  ISNS_B = "); Serial.println(isns_b_val);
}