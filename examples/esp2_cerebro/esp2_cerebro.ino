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
// Groq: API gratuita (14.400 req/dia), compativel com o formato OpenAI
const char* groqKey  = ENV_GROQ_API_KEY;

Audio audio;
IRsend irsend(PINO_IR);

String mensagemRecebida = "";
bool temNovaMensagem = false;

// --- Buffers seguros para o callback ESP-NOW (roda numa ISR) ---
static char _bufferISR[250];
static int  _tamanhoISR = 0;

// Envia pergunta para a Groq (Llama 3.3) — gratuito, 14.400 req/dia
String enviarParaGroq(String pergunta) {
  if (WiFi.status() != WL_CONNECTED) return "";

  // Formato OpenAI Chat Completions (Groq é 100% compativel)
  String payload = "{";
  payload += "\"model\":\"llama-3.3-70b-versatile\",";
  payload += "\"max_tokens\":80,";
  payload += "\"messages\":[";
  payload += "{\"role\":\"system\",\"content\":\"Seja direto. Responda em ate 10 palavras. Se o usuario pedir para ligar o ar, inclua a tag [AR_ON]. Se pedir para desligar, use [AR_OFF]. Se pedir para subir a temperatura, use [AR_TEMP_UP]. Se pedir para descer, use [AR_TEMP_DOWN].\"},";
  payload += "{\"role\":\"user\",\"content\":\"" + pergunta + "\"}";
  payload += "]}";

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(10);

  HTTPClient http;
  http.setTimeout(15000);
  http.begin(client, "https://api.groq.com/openai/v1/chat/completions");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(groqKey));

  int httpCode = http.sendRequest("POST", payload);
  String resposta = "";

  if (httpCode == HTTP_CODE_OK) {
    String json = http.getString();
    // Extrai o campo "content" da resposta
    int idx = json.indexOf("\"content\":");
    if (idx != -1) {
      int start = json.indexOf("\"", idx + 10) + 1;
      int end   = json.indexOf("\"", start);
      resposta  = json.substring(start, end);
      resposta.replace("\\n", " ");
      resposta.trim();
    }
  } else {
    String corpo = http.getString();
    Serial.println("[Groq] Erro HTTP: " + String(httpCode));
    Serial.println("[Groq] Detalhe: " + corpo.substring(0, 150));
  }
  http.end();
  return resposta;
}

// Callback do ESP-NOW (roda numa ISR — apenas copia bytes para buffer estático)
void aoReceberDados(const esp_now_recv_info_t *info, const uint8_t *dados, int tamanho) {
  if (tamanho > 249) tamanho = 249; // protege contra overflow
  memcpy(_bufferISR, dados, tamanho);
  _bufferISR[tamanho] = '\0';
  _tamanhoISR = tamanho;
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
    // Copia o buffer da ISR para a string de forma segura fora da interrupção
    mensagemRecebida = String(_bufferISR);
    // Filtra caracteres não imprimíveis que possam ter vindo por ruído de rádio
    String mensagemLimpa = "";
    for (int i = 0; i < (int)mensagemRecebida.length(); i++) {
      char c = mensagemRecebida[i];
      if (c >= 32 && c < 127) mensagemLimpa += c;
    }
    mensagemLimpa.trim();
    if (mensagemLimpa.length() == 0) return;

    Serial.println("\n> ESP1 Ouviu: " + mensagemLimpa);

    String respostaIA = enviarParaGroq(mensagemLimpa);

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

      // Toca o áudio da fala humana via Google Translate TTS (100% Gratuito)
      respostaIA.trim();
      if (respostaIA.length() > 0) {
        Serial.println("> Groq Respondeu: " + respostaIA);
        // Usa a API do Google Translate (limite ~200 caracteres, ideal para frases curtas)
        audio.connecttospeech(respostaIA.c_str(), "pt-BR");
      }
    }
  }
}
