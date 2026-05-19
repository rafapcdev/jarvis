#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoGPTChat.h>
#include <env.h>

// Pinos do Microfone I2S
#define I2S_MIC_SERIAL_CLOCK 32    
#define I2S_MIC_LEFT_RIGHT_CLOCK 25 
#define I2S_MIC_SERIAL_DATA 33     

// Pino do Botão Externo (Ligue o botão entre o Pino 14 e o GND)
#define PINO_BOTAO 14
#define SAMPLE_RATE 8000

// ----- CONFIGURAÇÕES DA REDE E API -----
const char* ssid     = ENV_WIFI_SSID;
const char* password = ENV_WIFI_PASSWORD;
const char* openAiKey = ENV_OPENAI_API_KEY; // Usada apenas para o Whisper (Reconhecimento de Voz)

// !!! COLOQUE AQUI O MAC ADDRESS DO SEU ESP2 !!!
uint8_t enderecoESP2[] = {0x24, 0x0A, 0xC4, 0xXX, 0xXX, 0xXX}; 

// Chave Criptográfica ESP-NOW (Deve ser igual no ESP1 e ESP2 - exatos 16 bytes)
uint8_t chaveEspNow[16] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00};

ArduinoGPTChat gptChat(openAiKey, "https://api.openai.com/v1/audio/transcriptions");
esp_now_peer_info_t peerInfo;

bool buttonPressed = false;
bool wasButtonPressed = false;
unsigned long ultimoCheckWiFi = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PINO_BOTAO, INPUT_PULLUP);

  Serial.println("\n[ESP1] Iniciando Ouvido...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  // Removido o while() bloqueante. A conexao sera verificada no loop()

  // Inicia o Rádio (ESP-NOW)
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  // Regista o ESP2 como recetor com CRIPTOGRAFIA
  memset(&peerInfo, 0, sizeof(peerInfo)); // Importante limpar a struct
  memcpy(peerInfo.peer_addr, enderecoESP2, 6);
  peerInfo.channel = 0; 
  peerInfo.encrypt = true; // Criptografia ativada!
  memcpy(peerInfo.lmk, chaveEspNow, 16);
  esp_now_add_peer(&peerInfo);

  // Inicia o Microfone
  gptChat.initializeRecording(I2S_MIC_SERIAL_CLOCK, I2S_MIC_LEFT_RIGHT_CLOCK, I2S_MIC_SERIAL_DATA,
                             SAMPLE_RATE, I2S_MODE_STD, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT);
                             
  Serial.println("[ESP1] Pronto! Segure o botao externo (Pino 14) para falar.");
}

void loop() {
  // Reconexão Wi-Fi Não-Bloqueante
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - ultimoCheckWiFi > 5000) {
      Serial.println("[ESP1] Tentando reconectar ao Wi-Fi...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      ultimoCheckWiFi = millis();
    }
  }

  buttonPressed = (digitalRead(PINO_BOTAO) == LOW);

  if (buttonPressed && !wasButtonPressed && !gptChat.isRecording()) {
    Serial.println("\n[ GRAVANDO ] Fale agora...");
    String cmd = "[BEEP_START]";
    esp_now_send(enderecoESP2, (uint8_t *)cmd.c_str(), cmd.length() + 1); // Pede para falar "Sim?"
    gptChat.startRecording();
    wasButtonPressed = true;
  }
  else if (!buttonPressed && wasButtonPressed && gptChat.isRecording()) {
    Serial.println("[ ENVIANDO PARA A OPENAI ]...");
    String cmd = "[BEEP_STOP]";
    esp_now_send(enderecoESP2, (uint8_t *)cmd.c_str(), cmd.length() + 1); // Pede para falar "Aguarde"
    
    // A biblioteca processa o áudio usando a sua função corrigida
    String textoTranscrito = gptChat.stopRecordingAndProcess();

    if (textoTranscrito.length() > 0) {
      Serial.println("Texto detetado: " + textoTranscrito);
      // Dispara o texto para o ESP2 via rádio instantaneamente
      esp_now_send(enderecoESP2, (uint8_t *)textoTranscrito.c_str(), textoTranscrito.length() + 1);
    } else {
      Serial.println("Falha na transcricao ou audio vazio.");
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