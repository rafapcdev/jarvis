<div align="center">

<img src="img/logo.png" alt="JARVIS Logo" width="200"/>

# 🤖 JARVIS — Assistente de Voz com IA Local

[![Arduino](https://img.shields.io/badge/Arduino-ESP32-blue.svg)](https://github.com/arduino/arduino-esp32)
[![Branch](https://img.shields.io/badge/Branch-GROCK--IA--1.0V-orange.svg)](https://github.com/rafapcdev/jarvis/tree/GROCK-IA-1.0V)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32-red.svg)](https://www.espressif.com/)
[![IA](https://img.shields.io/badge/IA-Groq%20%7C%20Llama%203.3-blueviolet.svg)](https://groq.com)
[![TTS](https://img.shields.io/badge/TTS-Google%20Translate-success.svg)](#)

**Assistente de voz serverless com dois ESP32 comunicando por rádio (ESP-NOW), IA e conversão de voz para texto via Groq e síntese de voz (TTS) 100% gratuita via Google Translate.**

</div>

---

## 📋 Índice

- [Visão Geral](#-visão-geral)
- [Arquitetura do Sistema](#-arquitetura-do-sistema)
- [Hardware Necessário](#-hardware-necessário)
- [Ligações dos Pinos](#-ligações-dos-pinos)
- [Configuração Rápida](#-configuração-rápida)
- [Como Usar](#-como-usar)
- [Dificuldades Superadas](#-dificuldades-superadas-e-soluções)
- [Estrutura do Projeto](#-estrutura-do-projeto)

---

## 🧠 Visão Geral

O JARVIS é um assistente de voz distribuído entre **dois ESP32**, onde cada um tem uma responsabilidade exclusiva:

- **ESP1 (Ouvido)** — grava a voz do utilizador pelo microfone I2S, converte o áudio em texto via API Groq (Whisper) e envia o texto pelo rádio interno do ESP32 usando o protocolo **ESP-NOW** (sem precisar de internet entre eles).
- **ESP2 (Cérebro + Boca)** — recebe o texto via rádio, consulta o modelo de linguagem **Llama 3.3 (Groq)** para gerar a resposta e fala a resposta em voz alta via alto-falante I2S.

> 💡 **Alternativa ao microfone:** enquanto o microfone físico não está disponível, é possível digitar perguntas diretamente no **Monitor Serial** do ESP1 e enviá-las ao ESP2.

---

## 🏗️ Arquitetura do Sistema

```
┌──────────────────────────────────────────────────────┐
│                     UTILIZADOR                       │
│           Fala ou digita no Monitor Serial           │
└──────────────────┬───────────────────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────────────────┐
│              ESP1 — "OUVIDO"                         │
│                                                      │
│  • Microfone I2S (INMP441)                           │
│  • Grava áudio ao pressionar botão BOOT              │
│  • Envia áudio → API Groq/Whisper (STT)              │
│  • OU lê texto digitado no Monitor Serial            │
│  • Transmite texto via ESP-NOW (rádio 2.4GHz)        │
└──────────────────┬───────────────────────────────────┘
                   │  ESP-NOW (sem internet, rádio direto)
                   ▼
┌──────────────────────────────────────────────────────┐
│              ESP2 — "CÉREBRO + BOCA"                 │
│                                                      │
│  • Recebe texto via ESP-NOW                          │
│  • Envia para Groq API → Llama 3.3-70b               │
│  • Resposta em até 20 palavras                       │
│  • Controle de Ar Condicionado via IR (opcional)     │
│  • Sintetiza voz via Google Translate TTS (100% free)│
└──────────────────────────────────────────────────────┘
```

### APIs Utilizadas

| API | Função | Plano | Limite Gratuito |
|-----|--------|-------|-----------------|
| [Groq](https://groq.com) | Chat IA (Llama 3.3-70b) | Gratuito | 14.400 req/dia |
| [Groq Whisper](https://groq.com) | Fala → Texto (STT) | Gratuito | incluído |
| [Google Translate](#) | Texto → Voz (TTS) | Gratuito | ilimitado |

---

## 🔌 Hardware Necessário

### ESP1 — Ouvido
| Componente | Modelo | Observação |
|---|---|---|
| Microcontrolador | ESP32 DevKit V1 | Qualquer variante com WiFi |
| Microfone | INMP441 | Interface I2S, alimentação 3.3V |

### ESP2 — Cérebro + Boca
| Componente | Modelo | Observação |
|---|---|---|
| Microcontrolador | ESP32 DevKit V1 | Qualquer variante com WiFi |
| Amplificador I2S | MAX98357A | Saída mono, até 3W |
| Alto-falante | 4Ω / 3W | Qualquer alto-falante compatível |
| Emissor IR | LED IR 38kHz | Opcional — para controle de ar-condicionado |

---

## 🔧 Ligações dos Pinos

### ESP1 — Microfone INMP441

| Pino INMP441 | Pino ESP32 | Função |
|---|---|---|
| VDD | 3.3V | Alimentação (NUNCA use 5V!) |
| GND | GND | Terra |
| L/R | GND | Seleciona canal esquerdo |
| WS | GPIO 25 | Left/Right Clock |
| SCK | GPIO 32 | Serial Clock |
| SD | GPIO 33 | Serial Data |

> O pino BOOT (GPIO 0) é usado para iniciar/parar a gravação de voz.

### ESP2 — Amplificador MAX98357A

| Função | Pino ESP32 | Descrição |
|---|---|---|
| I2S_DOUT | GPIO 22 | Dados de áudio |
| I2S_BCLK | GPIO 26 | Bit Clock |
| I2S_LRC | GPIO 27 | Left/Right Clock |

### ESP2 — LED Emissor IR (Opcional)

| Função | Pino ESP32 |
|---|---|
| IR OUT | GPIO 4 |

---

## ⚙️ Configuração Rápida

### 1. Instale a biblioteca no Arduino IDE

Copie a pasta do projeto para o diretório de bibliotecas do Arduino:

```
Linux:   ~/Arduino/libraries/jarvis/
Windows: Documents\Arduino\libraries\jarvis\
macOS:   ~/Documents/Arduino/libraries/jarvis/
```

### 2. Configure o arquivo `env.h`

> ⚠️ O arquivo `env.h` está protegido pelo `.gitignore` — suas chaves nunca vão para o repositório.

Edite o arquivo em `/home/dev/Arduino/libraries/jarvis/env.h` (ou no diretório global `/home/dev/Arduino/libraries/env/env.h`):

```c
#ifndef ENV_H
#define ENV_H

#define ENV_WIFI_SSID       "SEU_WIFI_AQUI"
#define ENV_WIFI_PASSWORD   "SUA_SENHA_AQUI"
#define ENV_GEMINI_API_KEY  "SUA_CHAVE_GEMINI_OPCIONAL"
#define ENV_GROQ_API_KEY    "SUA_CHAVE_GROQ"

#endif
```

Como obter as chaves gratuitas:
- **Groq**: https://console.groq.com → API Keys → Create API Key
- **Gemini** (opcional): https://aistudio.google.com/apikey

### 3. Configure o MAC Address do ESP2 no ESP1

Descubra o MAC Address do ESP2 abrindo o Monitor Serial dele após o boot. Em seguida, edite o `esp1_ouvido.ino`:

```cpp
// linha 23
uint8_t enderecoESP2[] = {0x78, 0x1C, 0x3C, 0xDC, 0x4E, 0x70}; // <-- substitua pelo MAC do seu ESP2
```

### 4. Instale as dependências

No Arduino IDE, vá em `Ferramentas → Gerenciar Bibliotecas` e instale:

- **ArduinoJson** (v7+)
- **IRremoteESP8266** (para controle IR no ESP2)

### 5. Faça o Upload

- Abra `examples/esp1_ouvido/esp1_ouvido.ino` → selecione a porta do **ESP1** → **Upload**
- Abra `examples/esp2_cerebro/esp2_cerebro.ino` → selecione a porta do **ESP2** → **Upload**

---

## 🕹️ Como Usar

### Modo 1 — Botão físico (microfone I2S)
1. Mantenha o botão **BOOT** do ESP1 pressionado para gravar
2. Fale sua pergunta ou comando
3. Solte o botão — o ESP1 envia o áudio para a Groq (Whisper)
4. O texto transcrito é enviado via rádio ESP-NOW ao ESP2
5. O ESP2 consulta o Llama 3.3, gera a resposta e fala em voz alta

### Modo 2 — Monitor Serial (sem microfone)
1. Abra o Monitor Serial do **ESP1** a **115200 baud**
2. Configure o fim de linha para **NL (Newline)**
3. Digite sua pergunta e pressione **Enter**
4. O texto é enviado via ESP-NOW ao ESP2 que responde normalmente

### Comandos de Ar Condicionado (ESP2)
O assistente reconhece intenções nos textos e aciona o LED IR automaticamente:

| Fala / Texto | Ação |
|---|---|
| "Liga o ar" | Envia código IR `[AR_ON]` |
| "Desliga o ar" | Envia código IR `[AR_OFF]` |
| "Sobe a temperatura" | Envia código IR `[AR_TEMP_UP]` |
| "Desce a temperatura" | Envia código IR `[AR_TEMP_DOWN]` |

---

## 🧩 Dificuldades Superadas e Soluções

Durante o desenvolvimento desta versão (`GROCK-IA-1.0V`), vários problemas técnicos foram identificados e resolvidos. Esta seção documenta cada um deles para ajudar outros desenvolvedores.

---

### 🔴 Problema 1 — ESP32 travando na conexão WiFi

**Sintoma:**
```
[ESP1] Iniciando Ouvido...
..........................(infinito)
```

**Causa:**
O código ficava preso no laço `while (WiFi.status() != WL_CONNECTED)` porque as credenciais WiFi no arquivo `env.h` estavam desatualizadas (apontando para a rede `"Robotica"` em vez da rede atual). Como existem **três cópias** do `env.h` no projeto (raiz, `src/`, e na pasta global de bibliotecas do Arduino), o compilador usava a versão errada.

**Solução:**
Identificamos que o Arduino IDE prioriza o `env.h` dentro da pasta `src/` da biblioteca durante a compilação. Todos os três arquivos foram sincronizados com as credenciais corretas, e foi criado um processo de verificação para garantir que as três cópias estejam sempre alinhadas.

---

### 🔴 Problema 2 — Mensagem ESP-NOW corrompida (`yq^@p^@`)

**Sintoma:**
```
> ESP1 Ouviu: yq^@p^@
```

**Causa:**
O callback `aoReceberDados()` do ESP-NOW **roda dentro de uma interrupção de hardware (ISR)**. No código original, a conversão `String(buffer)` era feita dentro da ISR — o tipo `String` do Arduino usa `malloc` internamente, o que é proibido em contexto de interrupção no ESP32 e causa corrupção de memória.

**Solução:**
O callback passou a apenas **copiar os bytes brutos para um buffer estático global** (`static char _bufferISR[250]`) e setar uma flag. A conversão para `String`, a filtragem de caracteres e o envio à API são feitos no `loop()`, fora da interrupção:

```cpp
// Dentro da ISR — APENAS cópia de bytes (seguro)
void aoReceberDados(const esp_now_recv_info_t *info, const uint8_t *dados, int tamanho) {
  if (tamanho > 249) tamanho = 249;
  memcpy(_bufferISR, dados, tamanho);
  _bufferISR[tamanho] = '\0';
  temNovaMensagem = true;
}

// No loop() — conversão e processamento seguros
if (temNovaMensagem) {
  temNovaMensagem = false;
  mensagemRecebida = String(_bufferISR); // seguro aqui
  ...
}
```

---

### 🔴 Problema 3 — HTTP -1 (falha silenciosa no handshake SSL)

**Sintoma:**
```
[Gemini] Erro HTTP: -1
```

**Causa:**
O `WiFiClientSecure` sem timeout definido pode abortar o handshake TLS silenciosamente no ESP32, retornando `-1` sem mensagem de erro. Isso ocorre porque o handshake SSL pode exceder o tempo de espera padrão com servidores mais lentos.

**Solução:**
Adicionados timeouts explícitos antes de cada requisição:

```cpp
client.setHandshakeTimeout(10); // 10 segundos para o handshake TLS
http.setTimeout(10000);         // 10 segundos para a resposta HTTP
```

---

### 🔴 Problema 4 — Erro 404 na API Gemini

**Sintoma:**
```
Erro na conexao com o Gemini: 404
```

**Causa:**
O modelo `gemini-1.5-flash` foi descontinuado no endpoint `v1beta`. A URL com o nome antigo do modelo retornava 404 (não encontrado).

**Solução:**
Atualizado para o modelo atual e mais eficiente:

```cpp
// Antes (descontinuado):
"https://...models/gemini-1.5-flash:generateContent"

// Depois (atual):
"https://...models/gemini-2.0-flash-lite:generateContent"
```

---

### 🔴 Problema 5 — Erro 429 persistente no Gemini (quota esgotada)

**Sintoma:**
```
[Gemini] Rate limit (429). Aguardando 5s... (tentativa 1/3)
[Gemini] Rate limit (429). Aguardando 5s... (tentativa 2/3)
[Gemini] Rate limit (429). Aguardando 5s... (tentativa 3/3)
[Gemini] Falhou apos 3 tentativas.
```

**Causa:**
A chave Gemini usada pertencia a uma conta Google diferente da conta logada no AI Studio. O dashboard mostrava **zero requisições**, confirmando que as chamadas nunca chegavam à conta correta. Mesmo ao usar a chave da conta correta, a conta tinha **billing ativado sem saldo**, o que desativa o tier gratuito e bloqueia todas as requisições.

**Solução:**
Migração completa do backend de IA do **Gemini para a Groq API**, que oferece:
- Acesso ao modelo **Llama 3.3-70b** (qualidade equivalente ao GPT-4)
- **14.400 requisições por dia** no plano gratuito
- **30 requisições por minuto** (dobro do Gemini free)
- Sem necessidade de cartão de crédito

---

### 🔴 Problema 6 — JSON malformado na requisição Gemini

**Sintoma:** Erros 400 ou respostas inesperadas da API.

**Causa:**
O campo `parts` do `system_instruction` estava formatado como **objeto JSON** `{}` quando a API exige um **array** `[]`:

```json
// Errado:
"system_instruction": {"parts": {"text": "..."}}

// Correto:
"system_instruction": {"parts": [{"text": "..."}]}
```

**Solução:**
Corrigido o payload da requisição e adicionado log do corpo da resposta de erro para facilitar diagnósticos futuros.

---

### 🔴 Problema 7 — Chaves de API expostas bloqueando o push para o GitHub

**Sintoma:**
```
remote: - GITHUB PUSH PROTECTION
remote: Push cannot contain secrets
remote: —— OpenAI API Key / Groq API Key ——
```

**Causa:**
O GitHub tem um sistema de **Secret Scanning** que bloqueia automaticamente pushes contendo chaves de API hardcoded no código-fonte, mesmo em branches privadas.

**Solução:**
Todas as chaves foram movidas para o arquivo `env.h` que está listado no `.gitignore`. O código-fonte usa apenas os defines:

```cpp
// No código (público, vai para o GitHub):
const char* groqKey = ENV_GROQ_API_KEY;

// No env.h (privado, nunca vai para o GitHub):
#define ENV_GROQ_API_KEY "gsk_..."
```

---

## 📁 Estrutura do Projeto

```
jarvis/
├── .gitignore                        # Protege env.h e .env
├── env.h                             # ⚠️ Chaves locais (NÃO vai ao GitHub)
├── README.md                         # Esta documentação
├── library.properties                # Configuração da biblioteca Arduino
├── keywords.txt                      # Palavras-chave para syntax highlight
│
├── src/                              # Código-fonte da biblioteca
│   ├── env.h                         # ⚠️ Cópia local das chaves
│   ├── ArduinoGPTChat.cpp/.h         # Comunicação com APIs de chat e STT
│   ├── ArduinoASRChat.cpp/.h         # Reconhecimento de voz em tempo real
│   ├── ArduinoTTSChat.cpp/.h         # Texto para voz
│   ├── ArduinoMinimaxTTS.cpp/.h      # TTS via Minimax
│   ├── ArduinoRealtimeDialog.cpp/.h  # Diálogo em tempo real
│   ├── Audio.cpp/.h                  # Biblioteca de áudio I2S modificada
│   ├── I2SAudioPlayer.cpp/.h         # Player de áudio I2S
│   └── [aac/flac/mp3/opus/vorbis]_decoder/  # Decodificadores de áudio
│
└── examples/
    ├── esp1_ouvido/
    │   └── esp1_ouvido.ino           # ESP1: Microfone + STT + ESP-NOW TX
    ├── esp2_cerebro/
    │   └── esp2_cerebro.ino          # ESP2: ESP-NOW RX + Groq IA + TTS + IR
    └── chat/
        └── chat.ino                  # Exemplo básico de chat
```

---

## 🔑 Variáveis do `env.h`

| Define | Descrição | Onde obter |
|--------|-----------|------------|
| `ENV_WIFI_SSID` | Nome da rede WiFi | — |
| `ENV_WIFI_PASSWORD` | Senha da rede WiFi | — |
| `ENV_GROQ_API_KEY` | Chave da Groq (IA + STT) | [console.groq.com](https://console.groq.com) |
| `ENV_GEMINI_API_KEY` | Chave do Gemini (opcional) | [aistudio.google.com](https://aistudio.google.com/apikey) |

---

<div align="center">

**Desenvolvido com muito debug e café ☕**

Se este projeto te ajudou, deixa uma ⭐️ no repositório!

[🐛 Reportar Bug](https://github.com/rafapcdev/jarvis/issues) · [💡 Sugerir Feature](https://github.com/rafapcdev/jarvis/issues)

</div>
