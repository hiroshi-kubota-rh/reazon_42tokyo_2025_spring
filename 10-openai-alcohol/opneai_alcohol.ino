#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Audio.h>
#include <HTTPClient.h>

// --- 設定項目 ---
const char *ssid = "HOGE-2G";
const char *password = "hoge";

// VOICE VOX Setting
const char *tts_api_base_url = "https://api.tts.quest/v3/voicevox/synthesis?text=";
const char *tts_speaker_param = "&speaker=3";

// text
String tts_text_high = "アルコールを飲みすぎなのだ。お水を飲むのだあ";
String tts_text_low = "今日は最高の日なのだ！好きなお酒を飲むのだあ";

String mp3_url; // MP3 URL格納用

// スピーカー用 I2Sピン設定
#define I2S_BCLK 16
#define I2S_LRCK 17
#define I2S_DOUT 15

// アルコールセンサーピン
#define ALCOHOL_SENSOR_PIN 36

Audio audio;
bool is_playing = false;
int alcoholValue = 0; // アルコールセンサーの値

// --- Optional Callback Functions ---
void audio_info(const char *info)
{
    Serial.print("Info:        ");
    Serial.println(info);
}
void audio_id3data(const char *info)
{
    Serial.print("ID3 Data:    ");
    Serial.println(info);
}
void audio_eof_mp3(const char *info)
{
    Serial.print("End of MP3:  ");
    Serial.println(info);
    is_playing = false;
    Serial.println("Playback finished or stream stopped.");
}
void audio_showstation(const char *info)
{
    Serial.print("Station:     ");
    Serial.println(info);
}
void audio_showstreamtitle(const char *info)
{
    Serial.print("Stream Title:");
    Serial.println(info);
}
void audio_bitrate(const char *info)
{
    Serial.print("Bitrate:     ");
    Serial.println(info);
}
void audio_commercial(const char *info)
{
    Serial.print("Commercial:  ");
    Serial.println(info);
}
void audio_icyurl(const char *info)
{
    Serial.print("ICY URL:     ");
    Serial.println(info);
}
void audio_lasthost(const char *info)
{
    Serial.print("Last Host:   ");
    Serial.println(info);
}
void audio_eof_speech(const char *info)
{
    Serial.print("End of Speech:");
    Serial.println(info);
    is_playing = false;
}

// --- WiFi接続関数 ---
void connectWiFi()
{
    Serial.print("Connecting to WiFi ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 30)
    {
        delay(500);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("\nWiFi connection failed. Restarting...");
        ESP.restart();
    }
}

// --- MP3 URL取得関数 ---
bool getMp3Url(String text)
{
    HTTPClient http;
    String tts_api_url = String(tts_api_base_url) + text + tts_speaker_param;
    http.begin(tts_api_url);
    int httpResponseCode = http.GET();

    if (httpResponseCode == HTTP_CODE_OK)
    {
        String payload = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload);
        mp3_url = doc["mp3DownloadUrl"].as<String>();
        http.end();
        return true;
    }
    else
    {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        http.end();
        return false;
    }
}

// --- セットアップ ---
void setup()
{
    Serial.begin(115200);
    Serial.println("\nESP32-S3 Audio Player with Alcohol Sensor");

    // WiFi接続
    connectWiFi();

    // アルコールセンサーピンの初期化
    pinMode(ALCOHOL_SENSOR_PIN, INPUT);

    // アルコールセンサーの値を読み取る
    alcoholValue = analogRead(ALCOHOL_SENSOR_PIN);
    Serial.print("Alcohol Sensor Value: ");
    Serial.println(alcoholValue);

    // MP3 URL取得
    if (alcoholValue >= 100)
    {
        if (getMp3Url(tts_text_high))
        {
            Serial.print("MP3 URL: ");
            Serial.println(mp3_url);
        }
        else
        {
            Serial.println("Failed to get MP3 URL.");
            while (1)
                ;
        }
    }
    else
    {
        if (getMp3Url(tts_text_low))
        {
            Serial.print("MP3 URL: ");
            Serial.println(mp3_url);
        }
        else
        {
            Serial.println("Failed to get MP3 URL.");
            while (1)
                ;
        }
    }

    // I2Sピン設定
    audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);

    // モノラル出力に設定
    audio.forceMono(true);

    // 音量設定
    audio.setVolume(2);
    Serial.println("Setup complete.");

    Serial.println("Connecting to audio host...");
    if (audio.connecttohost(mp3_url.c_str()))
    {
        is_playing = true;
        Serial.println("Connected to host, playback should start.");
    }
    else
    {
        Serial.println("Failed to connect to host.");
        is_playing = false;
    }
}

void loop()
{
    // WiFi接続を維持
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi disconnected. Trying to reconnect...");
        audio.stopSong();
        is_playing = false;
        connectWiFi();
    }

    // Audioライブラリのメインループ処理
    audio.loop();

    if (!is_playing && WiFi.status() == WL_CONNECTED)
    {
        Serial.println("Playback stopped or not started. Trying to connect/reconnect...");
        if (audio.connecttohost(mp3_url.c_str()))
        {
            is_playing = true;
            Serial.println("Connected to host, playback should start.");
        }
        else
        {
            Serial.println("Failed to connect to host.");
            is_playing = false;
            delay(5000);
        }
    }

    // CPU負荷を下げるために少し待機
    delay(1);
}