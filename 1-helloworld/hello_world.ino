// シリアル通信の速度を設定
#define SERIAL_BAUD_RATE 115200

void setup() {
  // シリアル通信を開始
  Serial.begin(SERIAL_BAUD_RATE);

}

void loop() {
  // "Hello, World!" をシリアルモニタに出力
  Serial.println("Hello, World!");

  // 1秒待つ
  delay(1000);
}