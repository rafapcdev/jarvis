#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoGPTChat.h>
#include <env.h>

// Pinos do Microfone I2S
#define I2S_MIC_SERIAL_CLOCK 32    
#define I2S_MIC_LEFT_RIGHT_CLOCK 25 
#define I2S_MIC_SERIAL_DATA 33     

#define BOOT_BUTTON_PIN    0   // Botão BOOT interno do ESP32
#define EXT_BUTTON_PIN     4   // Botão externo: conecte entre GPIO 4 e GND
#define SAMPLE_RATE 8000

// ----- CONFIGURAÇÕES DA REDE E API -----
const char* ssid     = ENV_WIFI_SSID;
const char* password = ENV_WIFI_PASSWORD;
// Chave carregada do env.h (protegido pelo .gitignore)
const char* groqKey = ENV_GROQ_API_KEY;

// Aqui passamos a URL base da Groq, que emula a OpenAI perfeitamente e de graça!
ArduinoGPTChat gptChat(groqKey, "https://api.groq.com/openai");
// !!! COLOQUE AQUI O MAC ADDRESS DO SEU ESP2 !!!
uint8_t enderecoESP2[] = {0x78, 0x1C, 0x3C, 0xdc, 0x4e, 0x70};    
//78:1c:3c:dc:4e:70


esp_now_peer_info_t peerInfo;

bool buttonPressed = false;
bool wasButtonPressed = false;

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(EXT_BUTTON_PIN,  INPUT_PULLUP); // Botão externo (pull-up interno = sem resistor)

  Serial.println("\n[ESP1] Iniciando Ouvido...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n[ESP1] WiFi Conectado!");

  // Inicia o Rádio (ESP-NOW)
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  // Regista o ESP2 como recetor
  memcpy(peerInfo.peer_addr, enderecoESP2, 6);
  peerInfo.channel = 0; 
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // Inicia o Microfone
  gptChat.initializeRecording(I2S_MIC_SERIAL_CLOCK, I2S_MIC_LEFT_RIGHT_CLOCK, I2S_MIC_SERIAL_DATA,
                             SAMPLE_RATE, I2S_MODE_STD, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT);
                             
  Serial.println("[ESP1] Pronto! Segure o botao BOOT (GPIO 0) ou o botao externo (GPIO 4) para falar.");
}

void loop() {
  // Qualquer um dos dois botões ativa a gravação
  buttonPressed = (digitalRead(BOOT_BUTTON_PIN) == LOW) || (digitalRead(EXT_BUTTON_PIN) == LOW);

  if (buttonPressed && !wasButtonPressed && !gptChat.isRecording()) {
    Serial.println("\n[ GRAVANDO ] Fale agora...");
    gptChat.startRecording();
    wasButtonPressed = true;
  }
  else if (!buttonPressed && wasButtonPressed && gptChat.isRecording()) {
    Serial.println("[ ENVIANDO PARA A OPENAI ]...");
    
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

  // Verifica se há texto digitado no Monitor Serial para enviar ao ESP2
  if (Serial.available() > 0) {
    String entradaSerial = Serial.readStringUntil('\n');
    entradaSerial.trim();
    if (entradaSerial.length() > 0) {
      Serial.println("\n[ SERIAL ] Pergunta recebida: " + entradaSerial);
      Serial.println("[ SERIAL ] Enviando para o ESP2...");
      esp_now_send(enderecoESP2, (uint8_t *)entradaSerial.c_str(), entradaSerial.length() + 1);
    }
  }

  delay(10);
}