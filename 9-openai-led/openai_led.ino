#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>
#include <cJSON.h>
#include <cmath>

// wifi, openaiの接続情報
#include "env.h"
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const String openai_spi_key = OPENAI_API_KEY;

// マイク設定
#define I2S_MIC_SCK GPIO_NUM_42
#define I2S_MIC_WS GPIO_NUM_41
#define I2S_MIC_SD GPIO_NUM_16
// スピーカー
#define PIEZO_SPEAKER_PIN GPIO_NUM_12
// LED
#define LED_PIN_1  GPIO_NUM_13
#define LED_PIN_2  GPIO_NUM_14

// 録音設定
#define SAMPLE_RATE 16000
#define SAMPLE_SECONDS 1.5
#define NUM_SAMPLES (SAMPLE_RATE * SAMPLE_SECONDS)
#define I2S_BUFFER_SIZE (NUM_SAMPLES * sizeof(int16_t))

// I2S設定
i2s_config_t i2s_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = SAMPLE_RATE,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
  .communication_format = I2S_COMM_FORMAT_I2S,
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
  .dma_buf_count = 4,
  .dma_buf_len = 1024,
  .use_apll = false,
  .tx_desc_auto_clear = false,
  .fixed_mclk = 0
};
// I2Sピン設定
i2s_pin_config_t i2s_pins = {
  .bck_io_num = I2S_MIC_SCK,
  .ws_io_num = I2S_MIC_WS,
  .data_out_num = I2S_PIN_NO_CHANGE,
  .data_in_num = I2S_MIC_SD
};

// メモリデバッグ用 malloc ラッパー, 録音時間によってメモリ不足になるので調査用で作成
void* debugMalloc(size_t size, const char* label) {
  Serial.printf("[malloc] %s: 要求サイズ = %u bytes\n", label, (unsigned int)size);
  Serial.printf("  Before malloc: Heap = %u, Max Alloc = %u\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  void* ptr = malloc(size);
  if (ptr) {
    Serial.println("  malloc 成功！");
  } else {
    Serial.println("  malloc 失敗！！");
  }
  Serial.printf("  After malloc: Heap = %u, Max Alloc = %u\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  return ptr;
}

// ビープ音を出す関数, 録音合図のため作成
void beep() {
  const int frequency = 2000; // Hz
  const int resolution = 8; // 8-bit resolution
  const int duty = 2; // 50% デューティ（最大255）

  ledcAttach(PIEZO_SPEAKER_PIN, frequency, resolution);

  ledcWrite(PIEZO_SPEAKER_PIN, duty);  // 音を出す
  delay(1000);                         // 1s
  ledcWrite(PIEZO_SPEAKER_PIN, 0);     // 音を止める
}

void setup() {
  Serial.begin(115200);

  // LED初期化
  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
  digitalWrite(LED_PIN_1, LOW);
  digitalWrite(LED_PIN_2, LOW);

  // WiFi接続
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");

  // I2S初期化
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &i2s_pins);
  i2s_zero_dma_buffer(I2S_NUM_0);

  delay(1000); // 安定化待ち

  // ビープで録音合図
  Serial.println("3秒後に録音開始します...");
  beep();

  // 録音開始
  Serial.println("Recording...");
  uint8_t* audio_data = (uint8_t*)debugMalloc(I2S_BUFFER_SIZE, "audio_data");
  if (!audio_data) return;

  size_t bytes_read = 0;
  i2s_read(I2S_NUM_0, audio_data, I2S_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
  Serial.printf("録音完了: %d bytes\n", bytes_read);

  // WAV作成
  size_t wav_size = 44 + bytes_read;
  uint8_t* wav_data = (uint8_t*)debugMalloc(wav_size, "wav_data");
  if (!wav_data) {
    free(audio_data);
    return;
  }

  writeWavHeader(wav_data, bytes_read);
  memcpy(wav_data + 44, audio_data, bytes_read);
  free(audio_data);

  // Whisper API送信
  String result = sendWhisperRequest(wav_data, wav_size);
  free(wav_data);

  Serial.println("=== 認識結果 ===");
  Serial.println(result);

  handleResultLED(result);
}

void loop() {}

void writeWavHeader(uint8_t* header, size_t pcmDataLen) {
  uint32_t fileSize = pcmDataLen + 36;
  uint32_t byteRate = SAMPLE_RATE * 2;
  uint16_t blockAlign = 2;
  uint16_t bitsPerSample = 16;

  memcpy(header, "RIFF", 4);
  *(uint32_t*)(header + 4) = fileSize;
  memcpy(header + 8, "WAVEfmt ", 8);
  *(uint32_t*)(header + 16) = 16;
  *(uint16_t*)(header + 20) = 1;
  *(uint16_t*)(header + 22) = 1;
  *(uint32_t*)(header + 24) = SAMPLE_RATE;
  *(uint32_t*)(header + 28) = byteRate;
  *(uint16_t*)(header + 32) = blockAlign;
  *(uint16_t*)(header + 34) = bitsPerSample;
  memcpy(header + 36, "data", 4);
  *(uint32_t*)(header + 40) = pcmDataLen;
}

String sendWhisperRequest(uint8_t* audio_data, size_t audio_len) {
  String endpoint = "https://api.openai.com/v1/audio/transcriptions";
  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String itemPrefix = "--" + boundary + "\r\nContent-Disposition: form-data; name=";
  String contentType = "audio/wav";

  String reqBody = "";
  reqBody += itemPrefix + "\"model\"\r\n\r\nwhisper-1\r\n";
  reqBody += itemPrefix + "\"file\"; filename=\"record.wav\"\r\n";
  reqBody += "Content-Type: " + contentType + "\r\n\r\n";

  String reqEnd = "\r\n--" + boundary + "--\r\n";

  size_t totalLen = reqBody.length() + audio_len + reqEnd.length();
  uint8_t* postData = (uint8_t*)malloc(totalLen + 1);
  if (!postData) {
    Serial.println("Failed to allocate postData");
    return "";
  }

  uint8_t* p = postData;
  memcpy(p, reqBody.c_str(), reqBody.length());
  p += reqBody.length();
  memcpy(p, audio_data, audio_len);
  p += audio_len;
  memcpy(p, reqEnd.c_str(), reqEnd.length());
  p += reqEnd.length();
  *p = 0;

  HTTPClient http;
  http.begin(endpoint);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Authorization", "Bearer " + openai_spi_key);

  int httpCode = http.sendRequest("POST", postData, totalLen);
  String response = http.getString();
  http.end();
  free(postData);

  if (httpCode != 200) {
    Serial.printf("HTTP error code: %d\n", httpCode);
    return "";
  }

  cJSON* root = cJSON_Parse(response.c_str());
  if (!root) {
    Serial.println("Failed to parse JSON");
    return "";
  }

  cJSON* text = cJSON_GetObjectItem(root, "text");
  String result = "";
  if (text && cJSON_IsString(text)) {
    result = text->valuestring;
  } else {
    Serial.println("No text field found in response.");
  }

  cJSON_Delete(root);
  return result;
}

void handleResultLED(const String& result) {
  if (result == "Yes." || result == "Yeah.") { // Yeahになってしまうことがあるので追加
    digitalWrite(LED_PIN_1, HIGH);
    digitalWrite(LED_PIN_2, LOW);
  } else if (result == "No.") {
    digitalWrite(LED_PIN_1, HIGH);
    digitalWrite(LED_PIN_2, HIGH);
  } else {
    digitalWrite(LED_PIN_1, LOW);
    digitalWrite(LED_PIN_2, LOW);
  }

  // 3秒後に両方OFF
  delay(3000);
  digitalWrite(LED_PIN_1, LOW);
  digitalWrite(LED_PIN_2, LOW);
}
