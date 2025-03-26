#include <Arduino.h>
#include <driver/i2s.h>
#include <cmath> // pow関数を使うために必要

#define SAMPLE_BUFFER_SIZE 512
#define PITCH_SHIFT_SEMITONES 0
int SAMPLE_RATE;

#define I2S_MIC_SERIAL_CLOCK 42
#define I2S_MIC_LEFT_RIGHT_CLOCK 41
#define I2S_MIC_SERIAL_DATA 16

#define PIEZO_SPEAKER_PIN 21  // 圧電スピーカー出力（PWM）

// PWM設定
#define PWM_CHANNEL 0
#define PWM_RESOLUTION 8
#define PWM_FREQ 20000  // 圧電スピーカー向け高周波PWM

i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE, // 後で設定
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

i2s_pin_config_t i2s_mic_pins = {
    .bck_io_num = I2S_MIC_SERIAL_CLOCK,
    .ws_io_num = I2S_MIC_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SERIAL_DATA};

void setup()
{
  Serial.begin(115200);

  // サンプリングレート設定
  SAMPLE_RATE = 8000 * pow(2, (float)PITCH_SHIFT_SEMITONES / 12.0);
  if (SAMPLE_RATE <= 0) SAMPLE_RATE = 1;
  Serial.print("設定サンプリングレート: ");
  Serial.println(SAMPLE_RATE);
  i2s_config.sample_rate = SAMPLE_RATE;

  // I2S初期化
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);

  // PWM初期化
  // ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  // ledcAttachPin(PIEZO_SPEAKER_PIN, PWM_CHANNEL);
  ledcAttach(PIEZO_SPEAKER_PIN, PWM_FREQ, PWM_RESOLUTION);

  delay(3000);
}

void loop()
{
  int32_t raw_samples[SAMPLE_BUFFER_SIZE];
  size_t bytes_read = 0;
  i2s_read(I2S_NUM_0, raw_samples, sizeof(int32_t) * SAMPLE_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
  int samples_read = bytes_read / sizeof(int32_t);

  for (int i = 0; i < samples_read; i++)
  {
    // --- 修正ポイント1: マイクデータをスピーカー向けPWM値に変換 ---
    int32_t sample = raw_samples[i] >> 16; // 32bitを16bitにスケーリング
    sample = constrain(sample, -32768, 32767); // 範囲制限
    uint8_t pwm_value = map(sample, -32768, 32767, 0, 255); // PWM値に変換
    // ledcWrite(PWM_CHANNEL, pwm_value); // PWM出力
    ledcWrite(PIEZO_SPEAKER_PIN, pwm_value); // PWM出力

    delayMicroseconds(1000000 / SAMPLE_RATE); // 出力タイミング調整
  }

  // --- シリアルプロッタ出力（任意） ---
  int rangelimit = 3000;
  Serial.print(-rangelimit);
  Serial.print(" ");
  Serial.print(rangelimit);
  Serial.print(" ");
  float mean = 0;
  for (int i = 0; i < samples_read; i++) mean += raw_samples[i];
  mean /= samples_read;
  Serial.println(mean);
}