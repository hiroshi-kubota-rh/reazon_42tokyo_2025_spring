const int LED_PIN = 35;

void setup() {
  pinMode(LED_PIN, OUTPUT);

}

void loop() {
  // "Hello, World!" をシリアルモニタに出力
  digitalWrite(LED_PIN, HIGH);

  // 1秒待つ
  delay(1000);

  digitalWrite(LED_PIN, LOW);

  // 1秒待つ
  delay(1000);
}