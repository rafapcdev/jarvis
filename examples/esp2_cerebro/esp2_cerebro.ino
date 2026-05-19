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

// ----- CONFIGURAÇÕES DA REDE E API -----
const char* ssid     = ENV_WIFI_SSID;
const char* password = ENV_WIFI_PASSWORD;
const char* geminiKey = ENV_GEMINI_API_KEY; 
const char* openAiKey = ENV_OPENAI_API_KEY; // Usada apenas para gerar a Voz (TTS)

Audio audio;
IRsend irsend(PINO_IR);

String mensagemRecebida = "";
bool temNovaMensagem = false;

// Função ultraleve para falar com o Gemini
String enviarParaGemini(String pergunta) {
  if (WiFi.status() != WL_CONNECTED) return "";

  WiFiClientSecure client;
  client.setInsecure(); // Criptografia super leve

  HTTPClient http;
  String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(geminiKey);
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  // O nosso prompt de regras do Ar Condicionado injetado de forma nativa e rápida
  String payload = "{\"system_instruction\":{\"parts\":{\"text\":\"Seja direto. Responda em ate 10 palavras. Se o usuario pedir para ligar o ar, inclua a tag [AR_ON]. Se pedir para desligar, use [AR_OFF]. Se pedir para subir a temperatura, use [AR_TEMP_UP]. Se pedir para descer, use [AR_TEMP_DOWN].\"}},";
  payload += "\"contents\":[{\"parts\":[{\"text\":\"" + pergunta + "\"}]}]}";

  int httpCode = http.sendRequest("POST", payload);
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
  mensagemRecebida = String(bufferTexto);
  temNovaMensagem = true;
}

void setup() {
  Serial.begin(115200);
  irsend.begin();

  Serial.println("\n[ESP2] Iniciando Cerebro e Boca...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n[ESP2] WiFi Conectado!");

  // Inicia o Rádio
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(aoReceberDados);

  // Configurações de Áudio com máxima RAM livre
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(100);
  audio.setBufsize(16384, 0); // Dobro do buffer de áudio porque temos RAM de sobra!

  Serial.println("[ESP2] Pronto! Aguardando comandos do ESP1...");
}

void loop() {
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
        
        // Desativa a conexão segura estrita temporariamente para o áudio fluir rápido
        audio.openai_speech(String(openAiKey), "tts-1", respostaIA, "alloy", "mp3", "1.0");
      }
    }
  }
}
