#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "SSID"; 
const char* password = "パスワード";

const char* url = "http://abehiroshi.la.coocan.jp/top.htm";


void setup() {
  Serial.begin(115200);
  delay(4000); // シリアルモニタの準備待ち

  // Wi-Fi接続開始
  Serial.printf("Wi-Fiに接続中: %s\n", ssid);
  WiFi.begin(ssid, password);

  // 接続が確立されるまで待機
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fiに接続完了");
  Serial.print("IPアドレス: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) { // Wi-Fiが接続されている場合
    HTTPClient http;

    Serial.printf("URLに接続中: %s\n", url);
    http.begin(url); // 指定したURLに接続

    int httpCode = http.GET(); // GETリクエストを送信

    if (httpCode > 0) { // レスポンスが受信できた場合
      Serial.printf("HTTPステータスコード: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK) { // ステータスコードが200の場合
        String payload = http.getString(); // レスポンスの内容を取得
        Serial.println("受信したデータ:");
        Serial.println(payload);
      }
    } else {
      Serial.printf("GETリクエストに失敗: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end(); // 接続を終了
  } else {
    Serial.println("Wi-Fiが切断されています。再接続を試みます...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\n再接続完了");
  }

  delay(60000); // 60秒待機してから再度リクエスト
}
