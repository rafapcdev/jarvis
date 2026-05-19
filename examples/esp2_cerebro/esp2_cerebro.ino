#include <WiFi.h>
#include <esp_now.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "Audio.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <env.h>

// Pinos do Alto-falante I2S (MAX98357A)
#define I2S_DOUT 22
#define I2S_BCLK 26
#define I2S_LRC 27

// Pino do LED Emissor de Infravermelhos
const uint16_t PINO_IR = 4;
// Pino do Buzzer para sons de feedback (Conecte um buzzer passivo ou ativo aqui, consome 0 RAM)
const uint16_t PINO_BUZZER = 15;

// ----- CONFIGURAÇÕES DA REDE E API -----
const char* ssid     = ENV_WIFI_SSID;
const char* password = ENV_WIFI_PASSWORD;
const char* geminiKey = ENV_GEMINI_API_KEY; 
const char* openAiKey = ENV_OPENAI_API_KEY; // Usada apenas para gerar a Voz (TTS)

// !!! COLOQUE AQUI O MAC ADDRESS DO SEU ESP1 !!! (Necessario para a criptografia funcionar no receptor)
uint8_t enderecoESP1[] = {0xFF, 0xFF, 0xFF, 0xXX, 0xXX, 0xXX}; 

// Chave Criptográfica ESP-NOW (Deve ser igual no ESP1 e ESP2 - exatos 16 bytes)
uint8_t chaveEspNow[16] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00};

Audio audio;
IRsend irsend(PINO_IR);

String mensagemRecebida = "";
bool temNovaMensagem = false;
unsigned long ultimoCheckWiFi = 0;

// Função ultraleve para falar com o Gemini
String enviarParaGemini(String pergunta) {
  if (WiFi.status() != WL_CONNECTED) return "";

  WiFiClientSecure client;
  client.setInsecure(); // Criptografia super leve

  HTTPClient http;
  String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(geminiKey);
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  // O nosso prompt injetado de forma nativa e rápida usando char array para não fragmentar a RAM
  char payload[512];
  snprintf(payload, sizeof(payload), 
    "{\"system_instruction\":{\"parts\":{\"text\":\"Seja direto. Responda em ate 20 palavras. Se o usuario pedir para ligar o ar, inclua a tag [AR_ON]. Se pedir para desligar, use [AR_OFF]. Se pedir para subir a temperatura, use [AR_TEMP_UP]. Se pedir para descer, use [AR_TEMP_DOWN].\"}},\"contents\":[{\"parts\":[{\"text\":\"%s\"}]}]}", 
    pergunta.c_str());

  int httpCode = http.sendRequest("POST", (uint8_t*)payload, strlen(payload));
  String resposta = "";

  if (httpCode == HTTP_CODE_OK) {
    String jsonResponse = http.getString();
    int textIndex = jsonResponse.indexOf("\"text\":");
    if (textIndex != -1) {
      int start = jsonResponse.indexOf("\"", textIndex + 7) + 1;
      int end = jsonResponse.indexOf("\"", start);
      resposta = jsonResponse.substring(start, end);
      resposta.replace("\\n", "");
      resposta.trim();
    }
  } else {
    Serial.println("Erro na conexao com o Gemini: " + String(httpCode));
  }
  http.end();
  return resposta;
}

// Callback super rápido do ESP-NOW (recebe o texto do ESP1)
void aoReceberDados(const esp_now_recv_info_t *info, const uint8_t *dados, int tamanho) {
  char bufferTexto[tamanho + 1];
  memcpy(bufferTexto, dados, tamanho);
  bufferTexto[tamanho] = '\0';
  String msg = String(bufferTexto);

  // Intercepta comandos de som
  if (msg == "[BEEP_START]") {
    tone(PINO_BUZZER, 2000, 100); // Bipe agudo e rápido (100ms) ao começar a gravar
    return;
  }
  if (msg == "[BEEP_STOP]") {
    tone(PINO_BUZZER, 1000, 150); // Bipe mais grave ao parar de gravar e começar a pensar
    return;
  }

  mensagemRecebida = msg;
  temNovaMensagem = true;
}

void setup() {
  Serial.begin(115200);
  irsend.begin();
  pinMode(PINO_BUZZER, OUTPUT);

  Serial.println("\n[ESP2] Iniciando Cerebro e Boca...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  // Removido while bloqueante. A conexao sera verificada no loop()

  // Inicia o Rádio
  if (esp_now_init() != ESP_OK) return;
  
  // Para ESP-NOW Criptografado, o receptor também precisa registrar o emissor
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, enderecoESP1, 6);
  peerInfo.channel = 0; 
  peerInfo.encrypt = true;
  memcpy(peerInfo.lmk, chaveEspNow, 16);
  esp_now_add_peer(&peerInfo);

  esp_now_register_recv_cb(aoReceberDados);

  // Configurações de Áudio com máxima RAM livre
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(100);
  audio.setBufsize(16384, 0); // Dobro do buffer de áudio porque temos RAM de sobra!

  Serial.println("[ESP2] Pronto! Aguardando comandos do ESP1...");
}

void loop() {
  // Reconexão Wi-Fi Não-Bloqueante
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - ultimoCheckWiFi > 5000) {
      Serial.println("[ESP2] Tentando reconectar ao Wi-Fi...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      ultimoCheckWiFi = millis();
    }
  }

  audio.loop();

  if (temNovaMensagem) {
    temNovaMensagem = false;
    Serial.println("\n> ESP1 Ouviu: " + mensagemRecebida);

    String respostaIA = enviarParaGemini(mensagemRecebida);

    if (respostaIA != "") {
      
      // Filtra e executa comandos do Ar Condicionado
      if (respostaIA.indexOf("[AR_ON]") != -1) {
        Serial.println("--> ACAO: Ligando o Ar Condicionado");
        irsend.sendNEC(0x20DF10EF, 32); // Código HEX base
        respostaIA.replace("[AR_ON]", "");
      }
      else if (respostaIA.indexOf("[AR_OFF]") != -1) {
        Serial.println("--> ACAO: Desligando o Ar Condicionado");
        irsend.sendNEC(0x20DF906F, 32);
        respostaIA.replace("[AR_OFF]", "");
      }
      else if (respostaIA.indexOf("[AR_TEMP_UP]") != -1) {
        Serial.println("--> ACAO: Subindo Temperatura");
        irsend.sendNEC(0x20DFA05F, 32);
        respostaIA.replace("[AR_TEMP_UP]", "");
      }
      else if (respostaIA.indexOf("[AR_TEMP_DOWN]") != -1) {
        Serial.println("--> ACAO: Descendo Temperatura");
        irsend.sendNEC(0x20DFB04F, 32);
        respostaIA.replace("[AR_TEMP_DOWN]", "");
      }

      // Toca o áudio da fala humana via OpenAI de forma nativa e leve
      respostaIA.trim();
      if (respostaIA.length() > 0) {
        Serial.println("> Gemini Respondeu: " + respostaIA);
        
        // Usa a API gratuita do Google Tradutor (Custo zero, voz robotizada)
        audio.connecttospeech(respostaIA.c_str(), "pt");
      }
    }
  }
}
