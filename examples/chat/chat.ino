#include <WiFi.h>
#include <ArduinoGPTChat.h>
#include "Audio.h"
#include <env.h>

// Pinos de saída de áudio (MAX98357A)
#define I2S_DOUT 22
#define I2S_BCLK 26
#define I2S_LRC 27

// Pinos de entrada do microfone (INMP441)
#define I2S_MIC_SERIAL_CLOCK 32    
#define I2S_MIC_LEFT_RIGHT_CLOCK 25 
#define I2S_MIC_SERIAL_DATA 33     

#define BOOT_BUTTON_PIN 0
#define SAMPLE_RATE 8000

#define I2S_MODE I2S_MODE_STD
#define I2S_BIT_WIDTH I2S_DATA_BIT_WIDTH_16BIT
#define I2S_SLOT_MODE I2S_SLOT_MODE_MONO
#define I2S_SLOT_MASK I2S_STD_SLOT_LEFT



Audio audio;

// WiFi settings
const char* ssid     = ENV_WIFI_SSID;
const char* password = ENV_WIFI_PASSWORD;

// Option 2: Use custom API key and URL (uncomment if using custom configuration)
//OpenAI apiBaseUrl : "https://api.openai.com"
const char* apiKey = ENV_GEMINI_API_KEY;
const char* apiBaseUrl = "https://generativelanguage.googleapis.com/v1beta/openai";
ArduinoGPTChat gptChat(apiKey, apiBaseUrl);

const char* systemPrompt = "Seja direto. Responda em no MÁXIMO 10 palavras. Sem explicações.";

bool buttonPressed = false;
bool wasButtonPressed = false;


void setup() {
  Serial.begin(115200);
  delay(1000);

  // Uso da Macro F() para salvar RAM
  Serial.println(F("\n\n----- Sistema Ultra-Leve Iniciando -----"));
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print(F("Conectando WiFi"));

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    Serial.print('.');
    delay(1000);
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\nWiFi conectado!"));
    
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(100);

    // EXTREMO: Reduz buffer para 4KB (Se a voz picotar muito, volte para 8192)
    audio.setBufsize(8192, 0); 

    // Garante que a IA não guarda histórico para poupar RAM
    gptChat.enableMemory(false);
    gptChat.setSystemPrompt(systemPrompt);

    gptChat.initializeRecording(I2S_MIC_SERIAL_CLOCK, I2S_MIC_LEFT_RIGHT_CLOCK, I2S_MIC_SERIAL_DATA,
                               SAMPLE_RATE, I2S_MODE, I2S_BIT_WIDTH, I2S_SLOT_MODE, I2S_SLOT_MASK);

    Serial.println(F("\n----- SISTEMA PRONTO -----"));
    Serial.println(F("Segure BOOT para falar..."));

  } else {
    Serial.println(F("\nFalha de WiFi."));
  }
}


void loop() {
  audio.loop();

  buttonPressed = (digitalRead(BOOT_BUTTON_PIN) == LOW);

  if (buttonPressed && !wasButtonPressed && !gptChat.isRecording()) {
    Serial.println(F("\n[ GRAVANDO ]"));

    if (gptChat.startRecording()) {
      wasButtonPressed = true;
    }
  }
  else if (!buttonPressed && wasButtonPressed && gptChat.isRecording()) {
    Serial.println(F("\n[ PROCESSANDO AUDIO ]"));

    String transcribedText = gptChat.stopRecordingAndProcess();

    if (transcribedText.length() > 0) {
      Serial.print(F("Voce: "));
      Serial.println(transcribedText);
      Serial.println(F("[ PENSANDO ]"));

      String response = gptChat.sendMessage(transcribedText);

      if (response != "") {
        Serial.print(F("IA: "));
        Serial.println(response);

        if (response.length() > 0) {
          Serial.println(F("[ GERANDO VOZ ]"));

          bool success = gptChat.textToSpeech(response);

          if (!success) {
            Serial.println(F("Falha na voz."));
          }
        }
      } else {
        Serial.println(F("Falha IA."));
      }
    } else {
      Serial.println(F("Nao entendi."));
    }

    wasButtonPressed = false;
  }
  else if (buttonPressed && gptChat.isRecording()) {
    gptChat.continueRecording();
  }
  else if (!buttonPressed && !gptChat.isRecording()) {
    wasButtonPressed = false;
  }

  delay(10); 
}