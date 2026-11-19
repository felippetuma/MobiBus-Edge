/*
  MOBIBUS - MONITORAMENTO DE FROTA DE ONIBUS
  ESP32 + 2x DHT22 + BUZZER + BOTAO DE ALERTA + FIWARE (MQTT)

  Funções:
  - Temperatura interna do onibus (DHT22) e temperatura do motor (DHT22)
  - Botao de alerta: o motorista aciona quando o onibus quebra
  - Buzzer: alerta do botao (prioridade) e alarme de temperatura
  - Integracao FIWARE (IoT Agent UltraLight 2.0 + Mosquitto) via MQTT:
      * publica sensores (t, m, h), pressao simulada dos pneus (p1..p6, PSI),
        alerta (a) e estado do buzzer (s)
      * recebe comandos, incluindo "resolverAlerta" vindo do sistema web

  Dependencias: PubSubClient e DHT sensor library for ESPx (DHTesp).

  Dashboard: 
  https://mobi-bus-gs.vercel.app/dashboard

  Authors: Felippe Tuma, Yan da Silva, Davi Toyota, Renan Martins, Felipe Motta
*/
 
#include <WiFi.h>
#include <PubSubClient.h>
 
// Biblioteca do sensor DHT
#include "DHTesp.h"
 
// ---------------- CONFIGURACAO FIWARE / REDE ----------------
// Preencha com os seus dados. Nao versione credenciais reais.
 
// WiFi (no Wokwi: "Wokwi-GUEST" e senha vazia)
#define WIFI_SSID      "SEU_WIFI_SSID"
#define WIFI_PASSWORD  "SUA_SENHA_WIFI"

// Broker MQTT (Mosquitto)
#define MQTT_BROKER    "fiware_server_ip"
#define MQTT_PORT      1883
// Deixe MQTT_USER vazio ("") para conectar sem autenticacao
#define MQTT_USER      ""
#define MQTT_PASSWORD  ""
 
// Os mesmos valores provisionados no IoT Agent (service group e device)
#define APIKEY         "TEF"
#define DEVICE_ID      "onibus001"   // ID do onibus (mesmo do IoT Agent)
 
// Intervalos (ms)
const unsigned long INTERVALO_PUB_SENSORES = 5000;   // publicacao de t, m, h, a
const unsigned long INTERVALO_PUB_ESTADO   = 10000;  // republicacao periodica de s
const unsigned long INTERVALO_RECONEXAO    = 5000;   // tentativas de MQTT
const unsigned long INTERVALO_RECONEXAO_WIFI = 10000; // tentativas de WiFi (associar leva alguns segundos)
 
// Tamanhos fixos dos buffers da camada de comunicacao
const uint16_t MQTT_BUFFER_BYTES = 256;  // padrao do PubSubClient e pequeno demais
const uint16_t MQTT_TIMEOUT_S = 3;       // limita o bloqueio de uma tentativa de connect
const size_t TAM_TOPICO = 96;
const size_t TAM_PAYLOAD = 128;
const size_t TAM_VALOR = 16;
const size_t TAM_MENSAGEM_CMD = 64;
const size_t TAM_CLIENT_ID = 64;
 
// ---------------- PINOS ----------------
 
#define PIN_DHT_INTERNO 22   // DHT22: temperatura interna do onibus
#define PIN_DHT_MOTOR   21   // DHT22: temperatura do motor
#define PIN_BUZZER      19
#define PIN_BOTAO       18   // INPUT_PULLUP: pressionado = LOW
 
// ---------------- SENSORES DHT22 ----------------
 
// Duas instancias separadas, uma por sensor
DHTesp dhtInterno;
DHTesp dhtMotor;
 
// DHT22 tem resolucao de 0,1 grau
#define DHT_CASAS_DECIMAIS 1
 
// Periodo de leitura (ms); o setup() garante pelo menos o minimo do DHT22
const unsigned long INTERVALO_LEITURA_MS = 2000;
unsigned long intervaloLeitura = INTERVALO_LEITURA_MS;
 
// ---------------- LIMITES DE TEMPERATURA ----------------
 
// Acima destes valores o buzzer dispara (prioridade menor que o botao).
// Os limites sao fixos no firmware: o ESP32 detecta sozinho, sem o dashboard.
// O DHT22 le de -40 a 80 C; o limite do motor (79) fica logo abaixo do fundo
// de escala, entao so e atingivel na faixa 79,1 a 80 C.
const float TEMP_INTERNA_MAX = 35.0;
const float TEMP_MOTOR_MAX   = 79.0;
 
// ---------------- PRESSAO DOS PNEUS (SIMULADA) ----------------

// Sem sensor fisico: a pressao de cada pneu e simulada por um passeio
// aleatorio que volta devagar para a pressao nominal. Para usar sensores
// reais, troque apenas simularPneus() por leituras reais.
// Ordem: p1 = dianteiro esq., p2 = dianteiro dir., p3/p4 = traseiros esq.,
// p5/p6 = traseiros dir.
const uint8_t NUM_PNEUS = 6;
const float PRESSAO_NOMINAL_PSI = 110.0;  // tipico de onibus urbano
const float PRESSAO_RUIDO_PSI = 0.3;      // variacao maxima por leitura
const float PRESSAO_RETORNO = 0.05;       // fracao do desvio corrigida por leitura
const float PRESSAO_SIM_MIN_PSI = 100.0;
const float PRESSAO_SIM_MAX_PSI = 120.0;

float pressaoPneus[NUM_PNEUS];

// ---------------- BOTAO DE ALERTA ----------------

const unsigned long DEBOUNCE_BOTAO_MS = 50;
 
bool botaoEstavel = HIGH;          // ultimo nivel estavel (HIGH = solto)
bool botaoLeituraAnterior = HIGH;
unsigned long botaoUltimaMudanca = 0;
 
// Ligado ao apertar o botao; so cai com o comando "resolverAlerta" do sistema web.
// Ponto de melhoria: um clique longo no botao como saida offline.
bool alertaAtivo = false;
bool ultimoAlertaPublicado = false;
 
// ---------------- CONTROLE DE ALARMES ----------------
 
bool alarmeTempInterna = false;
bool alarmeTempMotor = false;
 
unsigned long ultimaLeitura = 0;
unsigned long ultimoSom = 0;
 
// Evita repetir o mesmo alerta sonoro continuamente
String ultimoAlertaSonoro = "";
 
// ---------------- CONTROLE REMOTO DO BUZZER ----------------
 
// Buzzer: forcado (tom continuo) ou silenciado ate o fim da anomalia atual
bool buzzerForcado = false;
bool buzzerSilenciado = false;
 
// Alarme remoto de temperatura (comando alertTemp do front-end).
// Cai quando as leituras locais normalizam (ver lerSensores) ou com alertOff.
bool remotoTemperatura = false;
 
// ---------------- ALERTA SONORO SEM delay() ----------------
// Sequencia de tons: o primeiro toca em emitirSom(), os demais em
// atualizarSom(), espaçados por somPeriodo (mesmos tempos do codigo original).
 
int somFreq = 0;
int somDuracao = 0;
unsigned long somPeriodo = 0;
int somRestantes = 0;
unsigned long somProximo = 0;
 
// ---------------- ESTADO DA CONEXAO / FIWARE ----------------
 
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
 
// Topicos UltraLight 2.0 (montados no setup)
char topicAttrsBase[TAM_TOPICO];  // /<apikey>/<device_id>/attrs
char topicCmd[TAM_TOPICO];        // /<apikey>/<device_id>/cmd
char topicCmdexe[TAM_TOPICO];     // /<apikey>/<device_id>/cmdexe
char clientId[TAM_CLIENT_ID];
bool comunicacaoPronta = false;   // topicos, clientId e buffer montados no setup
 
unsigned long ultimaTentativaWifi = 0;
bool primeiraTentativaWifi = true;
bool wifiConectadoAnterior = false;
unsigned long ultimaTentativaMqtt = 0;
bool primeiraTentativaMqtt = true;
 
unsigned long ultimaPubSensores = 0;
unsigned long ultimaPubEstado = 0;
bool ultimoEstadoPublicado = false;
 
// Ultimas leituras validas (para publicacao)
float ultimaTempInterna = 0;
float ultimaUmidade = 0;
float ultimaTempMotor = 0;
bool internoValido = false;  // ultima leitura do DHT interno foi valida?
bool motorValido = false;    // ultima leitura do DHT do motor foi valida?
 
// ---------------- WIFI ----------------
 
// NAO bloqueante: no maximo uma tentativa por INTERVALO_RECONEXAO_WIFI.
// Chamada a cada volta do loop; so age enquanto nao ha conexao.
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
 
  // (agora - anterior) >= intervalo continua correto quando millis() da a volta
  if (!primeiraTentativaWifi &&
      millis() - ultimaTentativaWifi < INTERVALO_RECONEXAO_WIFI) {
    return;
  }
 
  primeiraTentativaWifi = false;
  ultimaTentativaWifi = millis();
 
  Serial.print("WiFi: conectando a ");
  Serial.print(WIFI_SSID);
  Serial.print(" (status antes=");
  Serial.print((int)WiFi.status());
  Serial.println(")");
 
  // Descarta uma associacao pendente para recomecar limpo
  WiFi.disconnect();
  wl_status_t status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.print("WiFi: begin status=");
  Serial.println((int)status);
}
 
// ---------------- MQTT ----------------
 
// Monta /<apikey>/<device_id>/<sufixo>; false se nao couber no buffer
bool montarTopico(char *destino, size_t tamanho, const char *sufixo) {
  int n = snprintf(destino, tamanho, "/%s/%s/%s", APIKEY, DEVICE_ID, sufixo);
 
  if (n < 0 || (size_t)n >= tamanho) {
    Serial.print("MQTT: ERRO topico truncado: ");
    Serial.println(sufixo);
    return false;
  }
 
  return true;
}
 
// Publica no broker e registra no Serial ("->") o retorno do publish
bool publicar(const char *topico, const char *payload) {
  if (!mqtt.connected()) return false;
 
  bool ok = mqtt.publish(topico, payload);
 
  Serial.print("-> ");
  Serial.print(topico);
  Serial.print(" ");
  Serial.print(payload);
  Serial.print(ok ? " (true)" : " (false, state=");
  if (!ok) {
    Serial.print(mqtt.state());
    Serial.print(")");
  }
  Serial.println();
 
  return ok;
}
 
// Publica um atributo em /<apikey>/<device_id>/attrs/<atributo> (valor puro)
bool publicarAtributo(const char *atributo, const char *valor) {
  char topico[TAM_TOPICO];
 
  int n = snprintf(topico, sizeof(topico), "%s/%s", topicAttrsBase, atributo);
  if (n < 0 || (size_t)n >= sizeof(topico)) {
    Serial.println("MQTT: ERRO topico de atributo truncado");
    return false;
  }
 
  return publicar(topico, valor);
}
 
// Formata temperatura / umidade conforme a resolucao do sensor
void formatarDht(float valor, char *destino, size_t tamanho) {
  snprintf(destino, tamanho, "%.*f", DHT_CASAS_DECIMAIS, valor);
}
 
// Publica o status do alerta ("true"/"false"), valor puro como os demais atributos
void publicarAlerta() {
  publicarAtributo("a", alertaAtivo ? "true" : "false");
 
  // Atualiza mesmo se falhar: o periodico (publicarSensores) repete
  ultimoAlertaPublicado = alertaAtivo;
}
 
// Publica t (interno), m (motor), h (umidade interna) e a (alerta),
// um atributo por vez (apenas leituras validas)
void publicarSensores() {
  char valor[TAM_VALOR];
 
  if (internoValido) {
    formatarDht(ultimaTempInterna, valor, sizeof(valor));
    publicarAtributo("t", valor);
 
    formatarDht(ultimaUmidade, valor, sizeof(valor));
    publicarAtributo("h", valor);
  }
 
  if (motorValido) {
    formatarDht(ultimaTempMotor, valor, sizeof(valor));
    publicarAtributo("m", valor);
  }
 
  // Pressao dos pneus (PSI): p1..p6
  for (uint8_t i = 0; i < NUM_PNEUS; i++) {
    char atributo[4];
    snprintf(atributo, sizeof(atributo), "p%u", i + 1);
    formatarDht(pressaoPneus[i], valor, sizeof(valor));
    publicarAtributo(atributo, valor);
  }

  publicarAlerta();

  // Atualiza o relogio mesmo se falhar: a nova tentativa vem no proximo intervalo
  ultimaPubSensores = millis();
}
 
// Estado do buzzer: "on" se esta acionado
bool existeAlerta() {
  return alertaAtivo || alarmeTempInterna || alarmeTempMotor || remotoTemperatura;
}
 
bool atuadoresAtivos() {
  return buzzerForcado || (existeAlerta() && !buzzerSilenciado);
}
 
// Publica o estado do buzzer no formato "chave|valor"
void publicarEstado() {
  bool estado = atuadoresAtivos();
  char payload[TAM_VALOR];
 
  snprintf(payload, sizeof(payload), "s|%s", estado ? "on" : "off");
  publicar(topicAttrsBase, payload);
 
  // Atualiza mesmo se falhar, para nao tentar a cada volta do loop;
  // o periodico (INTERVALO_PUB_ESTADO) repete
  ultimoEstadoPublicado = estado;
  ultimaPubEstado = millis();
}
 
// Remove espacos das pontas (in place) e devolve o inicio do texto
char *aparar(char *texto) {
  while (*texto != '\0' && isspace((unsigned char)*texto)) texto++;
 
  size_t n = strlen(texto);
  while (n > 0 && isspace((unsigned char)texto[n - 1])) {
    texto[--n] = '\0';
  }
 
  return texto;
}
 
// Prepara o buzzer para tocar o padrao do alerta remoto no proximo ciclo
void iniciarAlertaRemoto() {
  buzzerForcado = false;     // o tom continuo manual cede ao padrao do alarme
  buzzerSilenciado = false;  // alertTemp deve soar mesmo depois de um buzzer_off
  ultimoAlertaSonoro = "";   // forca controlarAlertas a emitir o padrao agora
}
 
// Executa o comando recebido; retorna false se for desconhecido
bool executarComando(const char *cmd, const char *params) {
  // Comandos enviados pelo dashboard. O buzzer e tratado em
  // controlarAlertas(), que ja toca o padrao com millis().
  if (strcmp(cmd, "alertTemp") == 0) {
    remotoTemperatura = true;
    iniciarAlertaRemoto();
    return true;
  }
 
  if (strcmp(cmd, "alertOff") == 0) {
    remotoTemperatura = false;
    return true;
  }
 
  // Sistema web resolve o alerta aberto pelo botao do motorista
  if (strcmp(cmd, "resolverAlerta") == 0) {
    alertaAtivo = false;
    Serial.println("ALERTA: resolvido pelo sistema web");
    return true;
  }
 
  if (strcmp(cmd, "buzzer_on") == 0) {
    buzzerForcado = true;
    buzzerSilenciado = false;
    somRestantes = 0;
    tone(PIN_BUZZER, 2000);  // tom continuo
    return true;
  }
 
  if (strcmp(cmd, "buzzer_off") == 0) {
    buzzerForcado = false;
    noTone(PIN_BUZZER);
    somRestantes = 0;
    ultimoAlertaSonoro = "";
    // Silencia o alerta sonoro ate a anomalia atual terminar
    if (existeAlerta()) buzzerSilenciado = true;
    return true;
  }
 
  return false;
}
 
// Callback de mensagens recebidas: "<device_id>@<comando>|<parametros>"
void onMessage(char *topico, byte *payload, unsigned int length) {
  char msg[TAM_MENSAGEM_CMD];
 
  // Valida o tamanho antes de copiar (o payload nao termina em '\0')
  if (length == 0 || length >= sizeof(msg)) {
    Serial.print("<- ");
    Serial.print(topico);
    Serial.print(" descartada, tamanho invalido: ");
    Serial.println(length);
    return;
  }
 
  memcpy(msg, payload, length);
  msg[length] = '\0';
 
  Serial.print("<- ");
  Serial.print(topico);
  Serial.print(" ");
  Serial.println(msg);
 
  char *arroba = strchr(msg, '@');
  if (arroba == NULL) return;
  *arroba = '\0';
 
  // Ignora comandos destinados a outro device_id
  if (strcmp(aparar(msg), DEVICE_ID) != 0) {
    Serial.println("<- ignorado: device_id diferente");
    return;
  }
 
  char *cmd = arroba + 1;
  char *params = strchr(cmd, '|');
  if (params != NULL) {
    *params = '\0';
    params++;
  } else {
    params = cmd + strlen(cmd);  // sem '|': parametros vazios
  }
 
  cmd = aparar(cmd);
  params = aparar(params);
 
  bool ok = executarComando(cmd, params);
 
  // Confirma o comando e depois republica o estado
  char confirmacao[TAM_PAYLOAD];
  int n = snprintf(confirmacao, sizeof(confirmacao), "%s@%s|%s",
                   DEVICE_ID, cmd, ok ? "ok" : "unknown command");
  if (n < 0 || (size_t)n >= sizeof(confirmacao)) {
    Serial.println("MQTT: ERRO confirmacao truncada");
  } else {
    publicar(topicCmdexe, confirmacao);
  }
 
  // Aplica o novo estado imediatamente antes de reportar
  controlarAlertas();
  publicarAlerta();
  publicarEstado();
}
 
// NAO bloqueante: no maximo uma tentativa por INTERVALO_RECONEXAO
void connectMqtt() {
  if (mqtt.connected()) return;
 
  if (!primeiraTentativaMqtt &&
      millis() - ultimaTentativaMqtt < INTERVALO_RECONEXAO) {
    return;
  }
 
  primeiraTentativaMqtt = false;
  ultimaTentativaMqtt = millis();
 
  Serial.print("MQTT: conectando a ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.println(MQTT_PORT);
 
  bool conectou;
  if (strlen(MQTT_USER) > 0) {
    conectou = mqtt.connect(clientId, MQTT_USER, MQTT_PASSWORD);
  } else {
    conectou = mqtt.connect(clientId);
  }
 
  if (!conectou) {
    Serial.print("MQTT: falhou, state=");
    Serial.println(mqtt.state());
    return;
  }
 
  Serial.println("MQTT: conectado");
 
  if (!mqtt.subscribe(topicCmd)) {
    // Sem a assinatura de cmd o device nao recebe comandos: tenta de novo
    Serial.print("MQTT: subscribe falhou, state=");
    Serial.println(mqtt.state());
    mqtt.disconnect();
    return;
  }
  Serial.print("-> subscribe ");
  Serial.println(topicCmd);
 
  // Ao conectar, publica estado e leituras atuais
  publicarEstado();
  publicarSensores();
}
 
// Mantem WiFi e MQTT no ar sem bloquear o loop
void manterConexoes() {
  connectWiFi();
 
  bool wifiConectado = (WiFi.status() == WL_CONNECTED);
  if (wifiConectado != wifiConectadoAnterior) {
    wifiConectadoAnterior = wifiConectado;
    if (wifiConectado) {
      Serial.print("WiFi: conectado, IP ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("WiFi: conexao perdida");
    }
  }
 
  if (!wifiConectado || !comunicacaoPronta) return;
 
  connectMqtt();
 
  if (mqtt.connected() && !mqtt.loop()) {
    Serial.print("MQTT: conexao perdida, state=");
    Serial.println(mqtt.state());
  }
}
 
// ---------------- CONFIGURACAO ----------------
 
void setup() {
  Serial.begin(115200);
 
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BOTAO, INPUT_PULLUP);
 
  noTone(PIN_BUZZER);
 
  // Inicializa os dois DHT22
  dhtInterno.setup(PIN_DHT_INTERNO, DHTesp::DHT22);
  dhtMotor.setup(PIN_DHT_MOTOR, DHTesp::DHT22);
 
  // Respeita o periodo minimo de amostragem do sensor
  intervaloLeitura = max(INTERVALO_LEITURA_MS,
                         (unsigned long)dhtInterno.getMinimumSamplingPeriod());
 
  Serial.println("==================================");
  Serial.println("            MOBIBUS");
  Serial.println(" Monitoramento de frota de onibus");
  Serial.println("==================================");
 
  Serial.println("Sistema iniciado!");

  // Pressao inicial de cada pneu: nominal com pequena diferenca entre eles
  randomSeed(esp_random());
  for (uint8_t i = 0; i < NUM_PNEUS; i++) {
    pressaoPneus[i] = PRESSAO_NOMINAL_PSI + random(-30, 31) / 10.0;
  }

  // Topicos UltraLight 2.0, sempre montados a partir de APIKEY e DEVICE_ID
  bool topicosOk =
    montarTopico(topicAttrsBase, sizeof(topicAttrsBase), "attrs") &&
    montarTopico(topicCmd, sizeof(topicCmd), "cmd") &&
    montarTopico(topicCmdexe, sizeof(topicCmdexe), "cmdexe");
 
  // clientId unico por placa: fiware-<DEVICE_ID>-<MAC do efuse em hex>
  uint64_t mac = ESP.getEfuseMac();
  int tamClientId = snprintf(clientId, sizeof(clientId), "fiware-%s-%04X%08X",
                             DEVICE_ID, (unsigned int)(mac >> 32), (unsigned int)mac);
  bool clientIdOk = (tamClientId > 0 && (size_t)tamClientId < sizeof(clientId));
  Serial.print("MQTT clientId: ");
  Serial.println(clientId);
 
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setSocketTimeout(MQTT_TIMEOUT_S);
 
  // Sem isto o PubSubClient descarta publicacoes que nao cabem no buffer
  bool bufferOk = mqtt.setBufferSize(MQTT_BUFFER_BYTES);
  if (!bufferOk) Serial.println("MQTT: ERRO setBufferSize");
 
  comunicacaoPronta = topicosOk && clientIdOk && bufferOk;
  if (!comunicacaoPronta) Serial.println("MQTT: ERRO na montagem, comunicacao desativada");
 
  // Primeira tentativa de WiFi (nao bloqueia; o loop repete a cada intervalo)
  WiFi.mode(WIFI_STA);
  connectWiFi();
 
  Serial.println("Aguardando leitura dos sensores...");
  Serial.println();
}
 
// ---------------- BOTAO DE ALERTA ----------------
 
// Debounce + deteccao de borda: aciona o alerta na borda de descida (HIGH -> LOW)
void lerBotao() {
  bool leitura = digitalRead(PIN_BOTAO);
 
  if (leitura != botaoLeituraAnterior) {
    botaoLeituraAnterior = leitura;
    botaoUltimaMudanca = millis();
  }
 
  if (millis() - botaoUltimaMudanca >= DEBOUNCE_BOTAO_MS &&
      leitura != botaoEstavel) {
    botaoEstavel = leitura;
 
    if (botaoEstavel == LOW) {
      if (alertaAtivo) {
        Serial.println("BOTAO: alerta ja ativo, aguardando o sistema web resolver");
      } else {
        alertaAtivo = true;
        buzzerSilenciado = false;  // o alerta do botao sempre deve soar
        ultimoAlertaSonoro = "";   // forca o padrao sonoro agora
        Serial.println("ALERTA: acionado pelo motorista (onibus com problema)");
      }
    }
  }
}
 
// ---------------- LEITURA DOS SENSORES ----------------
 
// Le um DHT22; devolve false (e loga) se a leitura for invalida
bool lerDht(DHTesp &sensor, const char *nome, float &temperatura, float &umidade) {
  TempAndHumidity dados = sensor.getTempAndHumidity();
 
  if (sensor.getStatus() != DHTesp::ERROR_NONE ||
      isnan(dados.temperature) || isnan(dados.humidity)) {
    Serial.print("ERRO: Falha na leitura do DHT ");
    Serial.print(nome);
    Serial.print(": ");
    Serial.println(sensor.getStatusString());
    return false;
  }
 
  temperatura = dados.temperature;
  umidade = dados.humidity;
  return true;
}
 
// Avanca a simulacao: ruido aleatorio + retorno lento a pressao nominal
void simularPneus() {
  for (uint8_t i = 0; i < NUM_PNEUS; i++) {
    float ruido = (random(-100, 101) / 100.0) * PRESSAO_RUIDO_PSI;
    float p = pressaoPneus[i];
    p += (PRESSAO_NOMINAL_PSI - p) * PRESSAO_RETORNO + ruido;
    pressaoPneus[i] = constrain(p, PRESSAO_SIM_MIN_PSI, PRESSAO_SIM_MAX_PSI);
  }
}

void lerPneus() {
  simularPneus();

  Serial.print("Pressao dos pneus (PSI):");
  for (uint8_t i = 0; i < NUM_PNEUS; i++) {
    Serial.print(" p");
    Serial.print(i + 1);
    Serial.print("=");
    Serial.print(pressaoPneus[i], 1);
  }
  Serial.println();
}

void lerSensores() {
  float temp, umid;

  // Sensor interno: leitura invalida e ignorada (mantem o alarme anterior)
  if (lerDht(dhtInterno, "interno", temp, umid)) {
    ultimaTempInterna = temp;
    ultimaUmidade = umid;
    internoValido = true;
    alarmeTempInterna = (temp > TEMP_INTERNA_MAX);
 
    Serial.print("Temperatura interna: ");
    Serial.print(temp, 1);
    Serial.println(" C");
 
    Serial.print("Umidade interna: ");
    Serial.print(umid, 1);
    Serial.println(" %");
  } else {
    internoValido = false;
  }
 
  // Sensor do motor
  if (lerDht(dhtMotor, "motor", temp, umid)) {
    ultimaTempMotor = temp;
    motorValido = true;
    alarmeTempMotor = (temp > TEMP_MOTOR_MAX);
 
    Serial.print("Temperatura do motor: ");
    Serial.print(temp, 1);
    Serial.println(" C");
  } else {
    motorValido = false;
  }
 
  lerPneus();

  // O alarme remoto termina quando as leituras locais voltam ao normal
  if (!alarmeTempInterna && !alarmeTempMotor) remotoTemperatura = false;
 
  if (alarmeTempInterna) Serial.println("ALARME: TEMPERATURA INTERNA ACIMA DO LIMITE!");
  if (alarmeTempMotor)   Serial.println("ALARME: TEMPERATURA DO MOTOR ACIMA DO LIMITE!");
  if (alertaAtivo)       Serial.println("ALERTA: BOTAO DO MOTORISTA ATIVO");
  if (!existeAlerta())   Serial.println("STATUS: OPERACAO NORMAL");
 
  Serial.println("----------------------------------");
 
  // Controla os alertas sonoros
  controlarAlertas();
}
 
// ---------------- CONTROLE DOS ALERTAS ----------------
 
void controlarAlertas() {
  bool alerta = existeAlerta();
 
  // Fim da anomalia: o silenciamento do buzzer deixa de valer
  if (!alerta) {
    buzzerSilenciado = false;
  }
 
  if (buzzerForcado) {
    // Comando remoto: tom continuo ja iniciado, nada a fazer
    return;
  }
 
  if (alerta && !buzzerSilenciado) {
    // Prioridade: botao do motorista, depois temperatura interna,
    // depois temperatura do motor.
    String alertaAtual;
    unsigned long intervaloSom = 1800;
 
    if (alertaAtivo) {
      alertaAtual = "ALERTA";
      intervaloSom = 1000;  // mais insistente que os alarmes de temperatura
    }
    else if (alarmeTempInterna || remotoTemperatura) {
      alertaAtual = "TEMPERATURA";
    }
    else {
      alertaAtual = "MOTOR";
    }
 
    // Emite um novo padrao sonoro quando o tipo
    // de alerta muda ou quando o intervalo termina.
    if (alertaAtual != ultimoAlertaSonoro ||
        millis() - ultimoSom >= intervaloSom) {
 
      emitirSom(alertaAtual);
 
      ultimoAlertaSonoro = alertaAtual;
      ultimoSom = millis();
    }
  }
  else {
    // Sem alerta (ou silenciado): desliga o som
    noTone(PIN_BUZZER);
    somRestantes = 0;
 
    ultimoAlertaSonoro = "";
  }
}
 
// ---------------- ALERTAS SONOROS ----------------
 
// Inicia o padrao sonoro; os tons seguintes saem em atualizarSom()
void emitirSom(String tipo) {
  noTone(PIN_BUZZER);
 
  if (tipo == "ALERTA") {
    // Tres bips agudos e longos: botao do motorista
    somFreq = 3000;
    somDuracao = 250;
    somPeriodo = 350;
    somRestantes = 3;
 
    Serial.println("Alerta sonoro: BOTAO DO MOTORISTA");
  }
 
  else if (tipo == "TEMPERATURA") {
    // Dois sons medios: temperatura interna
    somFreq = 1800;
    somDuracao = 180;
    somPeriodo = 220;
    somRestantes = 2;
 
    Serial.println("Alerta sonoro: TEMPERATURA INTERNA");
  }
 
  else if (tipo == "MOTOR") {
    // Um som longo e grave: temperatura do motor
    somFreq = 1000;
    somDuracao = 500;
    somPeriodo = 500;
    somRestantes = 1;
 
    Serial.println("Alerta sonoro: TEMPERATURA DO MOTOR");
  }
 
  else {
    return;
  }
 
  // Primeiro tom imediato
  tone(PIN_BUZZER, somFreq, somDuracao);
  somRestantes--;
  somProximo = millis() + somPeriodo;
}
 
// Toca os tons restantes da sequencia no tempo certo (sem delay)
void atualizarSom() {
  if (somRestantes > 0 && millis() >= somProximo) {
    tone(PIN_BUZZER, somFreq, somDuracao);
    somRestantes--;
    somProximo += somPeriodo;
  }
}
 
// ---------------- LOOP PRINCIPAL ----------------
 
void loop() {
  // WiFi + MQTT (reconexao nao bloqueante e recebimento de comandos)
  manterConexoes();
 
  // Botao de alerta (debounce por millis)
  lerBotao();
 
  // Atualiza os sensores a cada intervaloLeitura
  if (millis() - ultimaLeitura >= intervaloLeitura) {
    ultimaLeitura = millis();
 
    lerSensores();
  }
 
  // Mantem o controle de alertas ativo entre leituras
  controlarAlertas();
  atualizarSom();
 
  // Publicacoes (somente com broker conectado)
  if (mqtt.connected()) {
    if (millis() - ultimaPubSensores >= INTERVALO_PUB_SENSORES) {
      publicarSensores();
    }
 
    // Alerta: publica assim que muda (botao apertado ou resolvido pelo web)
    if (alertaAtivo != ultimoAlertaPublicado) {
      publicarAlerta();
    }
 
    // Estado do buzzer: ao mudar e periodicamente
    if (atuadoresAtivos() != ultimoEstadoPublicado ||
        millis() - ultimaPubEstado >= INTERVALO_PUB_ESTADO) {
      publicarEstado();
    }
  }
}
