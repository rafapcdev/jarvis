#include <Arduino.h>

// ==========================================
// 1. FUNÇÕES DO SISTEMA ISOLADAS PARA TESTE
// ==========================================

// VERSÃO CORRIGIDA:
// Filtro de ruídos que preserva UTF-8 (acentos do Português: á, ç, ã, õ, etc.)
// O problema original: bytes > 126 eram descartados, mas UTF-8 usa bytes > 127
// para codificar caracteres acentuados. A solução é permitir bytes > 127.
// Apenas bytes de controle reais (0x00 a 0x1F) e 0x7F (DEL) são removidos.
String filtrarMensagem(String mensagemRecebida) {
  String mensagemLimpa = "";
  for (int i = 0; i < (int)mensagemRecebida.length(); i++) {
    uint8_t c = (uint8_t)mensagemRecebida[i];
    // Aceita: espaço (32) até '~' (126), e TODOS os bytes UTF-8 (>127)
    // Remove: bytes de controle (0-31) e DEL (127)
    if (c >= 32 && c != 127) {
      mensagemLimpa += mensagemRecebida[i];
    }
  }
  mensagemLimpa.trim();
  return mensagemLimpa;
}

// Função que extrai o texto do JSON retornado pela API da Groq/OpenAI
String extrairRespostaGroq(String json) {
  int idx = json.indexOf("\"content\":");
  if (idx == -1) return "";

  int start = json.indexOf("\"", idx + 10) + 1;
  if (start == 0) return "";

  int end = json.indexOf("\"", start);
  if (end == -1) return "";

  String resposta = json.substring(start, end);
  // Substitui sequência literal \n (barra+n) por espaço
  resposta.replace("\\n", " ");
  resposta.trim();
  return resposta;
}

// Detecta se uma tag de comando está presente na resposta
bool contemComando(String resposta, String comando) {
  return resposta.indexOf(comando) != -1;
}

// Remove a tag de comando da resposta e normaliza espaços duplos
// (quando a tag está no meio da frase, seu lugar vira um espaço duplo)
String removerComando(String resposta, String comando) {
  resposta.replace(comando, "");
  // Normaliza múltiplos espaços para um único espaço
  while (resposta.indexOf("  ") != -1) {
    resposta.replace("  ", " ");
  }
  resposta.trim();
  return resposta;
}

// ==========================================
// 2. SISTEMA DE TESTES UNITÁRIOS
// ==========================================

int testesPassaram = 0;
int testesFalharam = 0;

void assertEqual(String atual, String esperado, const char* nomeTeste) {
  if (atual == esperado) {
    Serial.printf("[PASS] %s\n", nomeTeste);
    testesPassaram++;
  } else {
    Serial.printf("[FAIL] %s\n", nomeTeste);
    Serial.printf("   -> Esperado: '%s'\n", esperado.c_str());
    Serial.printf("   -> Obtido:   '%s'\n", atual.c_str());
    testesFalharam++;
  }
}

void assertBool(bool atual, bool esperado, const char* nomeTeste) {
  if (atual == esperado) {
    Serial.printf("[PASS] %s\n", nomeTeste);
    testesPassaram++;
  } else {
    Serial.printf("[FAIL] %s (Esperado %s, obtido %s)\n",
                  nomeTeste,
                  esperado ? "true" : "false",
                  atual ? "true" : "false");
    testesFalharam++;
  }
}

void executarTestesUnitarios() {
  Serial.println("\n=== TESTES UNITÁRIOS - JARVIS ESP32 ===\n");

  // --- Grupo 1: filtrarMensagem() ---
  Serial.println("--- [Grupo 1] Filtro de Mensagem ESP-NOW ---");

  // Teste básico com quebra de linha
  assertEqual(
    filtrarMensagem("Como vai?\r\n"),
    "Como vai?",
    "filtrarMensagem: remove quebra de linha"
  );

  // Teste com bytes de controle (ruídos de rádio)
  String comRuido = "OK";
  comRuido += (char)0x01; // byte de controle
  comRuido += (char)0x02; // byte de controle
  comRuido += " Jarvis";
  assertEqual(
    filtrarMensagem(comRuido),
    "OK Jarvis",
    "filtrarMensagem: remove bytes de controle (ruído de rádio)"
  );

  // Teste com espaços extras nas pontas
  assertEqual(
    filtrarMensagem("   texto limpo   "),
    "texto limpo",
    "filtrarMensagem: remove espaços nas pontas"
  );

  // Teste com string vazia
  assertEqual(
    filtrarMensagem(""),
    "",
    "filtrarMensagem: string vazia"
  );

  // TESTE DO BUG CORRIGIDO: acentos do Português devem sobreviver ao filtro
  assertEqual(
    filtrarMensagem("Liga\u00e7\u00e3o com sucesso!"),
    "Liga\u00e7\u00e3o com sucesso!",
    "filtrarMensagem: preserva acentos do Português (UTF-8)"
  );

  // --- Grupo 2: extrairRespostaGroq() ---
  Serial.println("\n--- [Grupo 2] Extração de JSON da Groq ---");

  // JSON válido e completo
  String jsonValido = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"Vou ligar o ar.\"}}]}";
  assertEqual(
    extrairRespostaGroq(jsonValido),
    "Vou ligar o ar.",
    "extrairRespostaGroq: JSON valido padrao"
  );

  // JSON com sequência literal \n (como a Groq envia de verdade)
  String jsonComN = "{\"choices\":[{\"message\":{\"content\":\"Linha 1\\nLinha 2\"}}]}";
  assertEqual(
    extrairRespostaGroq(jsonComN),
    "Linha 1 Linha 2",
    "extrairRespostaGroq: substitui \\n literal por espaco"
  );

  // JSON quebrado / sem campo content
  String jsonInvalido = "{\"error\":\"invalid_api_key\"}";
  assertEqual(
    extrairRespostaGroq(jsonInvalido),
    "",
    "extrairRespostaGroq: JSON invalido retorna string vazia"
  );

  // JSON com campo content vazio
  String jsonVazio = "{\"choices\":[{\"message\":{\"content\":\"\"}}]}";
  assertEqual(
    extrairRespostaGroq(jsonVazio),
    "",
    "extrairRespostaGroq: content vazio retorna string vazia"
  );

  // --- Grupo 3: Comandos de Ar Condicionado ---
  Serial.println("\n--- [Grupo 3] Deteccao e Remocao de Comandos ---");

  assertBool(contemComando("Vou ligar o ar. [AR_ON]", "[AR_ON]"),  true,  "contemComando: detecta AR_ON presente");
  assertBool(contemComando("Tudo certo.",               "[AR_ON]"),  false, "contemComando: nao detecta AR_ON ausente");
  assertBool(contemComando("[AR_OFF] Desligando.",       "[AR_OFF]"), true,  "contemComando: detecta AR_OFF presente");
  assertBool(contemComando("Temperatura subindo. [AR_TEMP_UP]", "[AR_TEMP_UP]"), true, "contemComando: detecta AR_TEMP_UP");

  assertEqual(
    removerComando("Vou ligar o ar. [AR_ON]", "[AR_ON]"),
    "Vou ligar o ar.",
    "removerComando: remove tag do final"
  );
  assertEqual(
    removerComando("[AR_OFF] Desligando agora.", "[AR_OFF]"),
    "Desligando agora.",
    "removerComando: remove tag do inicio"
  );
  assertEqual(
    removerComando("Ok. [AR_TEMP_UP] Feito.", "[AR_TEMP_UP]"),
    "Ok. Feito.",
    "removerComando: remove tag do meio"
  );

  // --- Resultado Final ---
  Serial.println("\n===========================================");
  Serial.printf("RESULTADO: %d Passaram | %d Falharam\n", testesPassaram, testesFalharam);
  if (testesFalharam == 0) {
    Serial.println(">>> TODOS OS TESTES PASSARAM! Sistema OK. <<<");
  } else {
    Serial.println(">>> ATENCAO: HA FALHAS. Veja os [FAIL] acima! <<<");
  }
  Serial.println("===========================================\n");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  executarTestesUnitarios();
}

void loop() {
  // Nada a fazer
}
