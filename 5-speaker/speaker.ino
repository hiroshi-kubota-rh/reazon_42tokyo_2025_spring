#include <Arduino.h>

#define SPEAKER_PIN 21  // スピーカー接続ピン

void setup() {
  Serial.begin(115200);

  // PWMの設定: ピン21、周波数2000Hz、分解能8ビット
  if (ledcAttach(SPEAKER_PIN, 2000, 8)) {
    Serial.println("PWM設定成功");
  } else {
    Serial.println("PWM設定失敗");
  }
}

void loop() {
  // デューティ比50%でPWM出力
  ledcWrite(SPEAKER_PIN, 128);  // 8ビット分解能の場合、128は50%に相当
  delay(1000);

  // デューティ比0%でPWM停止
  ledcWrite(SPEAKER_PIN, 0);
  delay(1000);
}
