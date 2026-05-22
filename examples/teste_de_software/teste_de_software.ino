#include "Audio.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <env.h>

// Pinos virtuais para I2S (não precisa conectar nada)
#define I2S_DOUT 22
#define I2S_BCLK 26
#define I2S_LRC 27

const char *ssid = ENV_WIFI_SSID;
const char *password = ENV_WIFI_PASSWORD;
const char *groqKey = ENV_GROQ_API_KEY;

Audio audio;
bool testeExecutado = false;
bool ttsIniciado = false;
bool ttsTerminado = false;
unsigned long ttsInicioMs = 0;

// ==============================================
// CALLBACKS DA BIBLIOTECA DE ÁUDIO
// (chamadas automaticamente pelo audio.loop())
// ==============================================

// Imprime todos os logs internos da biblioteca de áudio
void audio_info(const char *info) {
  Serial.print("[Audio] ");
  Serial.println(info);
}

// Chamado quando o download/decodificação do TTS termina com sucesso
void audio_eof_speech(const char *info) {
  Serial.println("[TTS] Download e decodificação concluídos!");
  Serial.print("[TTS] Info final: ");
  Serial.println(info);
  ttsTerminado = true;
}

// Chamado quando um stream de rede termina
void audio_eof_stream(const char *info) {
  Serial.print("[TTS] Stream encerrado: ");
  Serial.println(info);
  ttsTerminado = true;
}

// ==============================================
// FUNÇÃO DE TESTE DA GROQ
// ==============================================
String enviarParaGroq(String pergunta) {
  if (WiFi.status() != WL_CONNECTED)
    return "";
  Serial.println("[Groq] Enviando pergunta...");

  String payload = "{";
  payload += "\"model\":\"llama-3.3-70b-versatile\",";
  payload += "\"max_tokens\":20,";
  payload += "\"messages\":[";
  payload += "{\"role\":\"system\",\"content\":\"Responda apenas com a palavra "
             "Sucesso.\"},";
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
    int idx = json.indexOf("\"content\":");
    if (idx != -1) {
      int start = json.indexOf("\"", idx + 10) + 1;
      int end = json.indexOf("\"", start);
      resposta = json.substring(start, end);
      resposta.trim();
    }
  } else {
    Serial.println("[Groq] Erro HTTP: " + String(httpCode));
  }
  http.end();
  return resposta;
}

// ==============================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== TESTE DE INTEGRAÇÃO - JARVIS ===");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Conectando ao Wi-Fi: %s", ssid);

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[Wi-Fi] Conectado! IP: %s\n",
                  WiFi.localIP().toString().c_str());
    Serial.printf("[Wi-Fi] Sinal (RSSI): %d dBm\n", WiFi.RSSI());

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(100);
    audio.setBufsize(16384, 0);
    Serial.println("[Audio] I2S inicializado em pinos virtuais.");
    delay(2000);
  } else {
    Serial.println("\n[ERRO] Falha no Wi-Fi. Verifique credenciais em env.h");
  }
}

// ==============================================
void loop() {
  audio.loop(); // ESSENCIAL: processa o áudio em background

  if (WiFi.status() == WL_CONNECTED && !testeExecutado) {
    testeExecutado = true;

    // --- Passo 1: Testar Groq ---
    Serial.println("\n--- [Passo 1] Testando API Groq ---");
    String respostaIA = enviarParaGroq("Teste de conexao");

    if (respostaIA.length() > 0) {
      Serial.println("[PASS] Groq respondeu: " + respostaIA);
    } else {
      Serial.println(
          "[FAIL] Groq nao respondeu. Verifique a chave API em env.h");
      return;
    }

    // --- Passo 2: Testar Google TTS ---
    Serial.println("\n--- [Passo 2] Testando Google TTS (download de voz) ---");
    Serial.println("[TTS] Iniciando conexao com translate.google.com...");
    Serial.println("[TTS] Aguarde os logs do audio_info abaixo:");
    Serial.println("---------------------------------------------");

    ttsIniciado = true;
    ttsInicioMs = millis();
    // Texto curto para o teste ser rápido
    audio.connecttospeech("Teste de voz.", "pt-BR");
  }

  // Monitora o progresso do TTS e detecta timeout
  if (ttsIniciado && !ttsTerminado) {
    unsigned long elapsed = millis() - ttsInicioMs;

    // Timeout de 15 segundos para o TTS
    if (elapsed > 15000) {
      Serial.println("---------------------------------------------");
      Serial.println("[FAIL] TTS: Timeout de 15s atingido sem resposta.");
      Serial.println("       Possíveis causas:");
      Serial.println("       1. Google TTS bloqueou a requisição (rate limit)");
      Serial.println("       2. Problema de memória no ESP32");
      Serial.println("       3. Falha na conexão SSL com translate.google.com");
      ttsIniciado = false;
    }
  }

  if (ttsTerminado) {
    Serial.println("---------------------------------------------");
    Serial.println("[PASS] TTS: Stream processado com sucesso!");
    Serial.println("\n=== TODOS OS TESTES PASSARAM ===");
    ttsTerminado = false;
    ttsIniciado = false;
  }
}
