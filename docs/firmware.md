# Firmware

Voltar ao [índice](README.md). Código: [`firmware/Mobibus/mobibus.ino`](../firmware/Mobibus/mobibus.ino).

Dependências: `PubSubClient` 2.8 ou superior (o `setBufferSize` exige essa versão) e `DHT sensor library for ESPx` (`DHTesp`). No Wokwi, o arquivo [`wokwi/libraries.txt`](../wokwi/libraries.txt) as instala.

## Constantes de configuração

Ficam no topo do arquivo.

### Rede e FIWARE

| Constante | Padrão | Descrição |
|---|---|---|
| `WIFI_SSID`, `WIFI_PASSWORD` | `SEU_WIFI_SSID`, `SUA_SENHA_WIFI` | Placeholders. No Wokwi use `Wokwi-GUEST` e senha vazia |
| `MQTT_BROKER` | `fiware_server_ip` | Placeholder: IP ou nome do Mosquitto |
| `MQTT_PORT` | `1883` | Porta do Mosquitto |
| `MQTT_USER`, `MQTT_PASSWORD` | vazios | Com `MQTT_USER` vazio, conecta sem autenticação |
| `APIKEY` | `TEF` | Igual ao service group do IoT Agent |
| `DEVICE_ID` | `onibus001` | Igual ao `device_id` provisionado |

### Limites e tempos

| Constante | Valor | Descrição |
|---|---|---|
| `TEMP_INTERNA_MAX` | 35,0 °C | Alarme se a leitura for maior |
| `TEMP_MOTOR_MAX` | 79,0 °C | Alarme se a leitura for maior |
| `INTERVALO_LEITURA_MS` | 2000 ms | Período de leitura. O `setup()` usa o maior entre este valor e o mínimo do DHT22 |
| `INTERVALO_PUB_SENSORES` | 5000 ms | Publicação de `t`, `h`, `m`, `p1` a `p6` e `a` |
| `INTERVALO_PUB_ESTADO` | 10000 ms | Republicação periódica de `s` |
| `INTERVALO_RECONEXAO` | 5000 ms | Tentativas de conexão MQTT |
| `INTERVALO_RECONEXAO_WIFI` | 10000 ms | Tentativas de conexão Wi-Fi |
| `DEBOUNCE_BOTAO_MS` | 50 ms | Debounce do botão |

### Buffers da comunicação

| Constante | Valor |
|---|---|
| `MQTT_BUFFER_BYTES` | 256 |
| `MQTT_TIMEOUT_S` | 3 |
| `TAM_TOPICO` | 96 |
| `TAM_PAYLOAD` | 128 |
| `TAM_VALOR` | 16 |
| `TAM_MENSAGEM_CMD` | 64 |
| `TAM_CLIENT_ID` | 64 |

O buffer padrão do PubSubClient é pequeno demais para os tópicos deste projeto, por isso o `setup()` chama `setBufferSize(256)`. Os tópicos e a confirmação de comando são montados com `snprintf` e o código confere o tamanho antes de usar.

## Ciclo de execução

`setup()`:

1. Abre o serial a 115200 baud.
2. Configura os pinos do buzzer e do botão e inicializa os dois DHT22.
3. Imprime o banner e "Sistema iniciado!".
4. Sorteia a pressão inicial dos seis pneus.
5. Monta os tópicos, o `clientId` e o buffer MQTT. Se algo falhar, a comunicação fica desativada e o erro vai ao serial.
6. Coloca o Wi-Fi em modo estação e faz a primeira tentativa de conexão, sem bloquear.

`loop()`, nesta ordem:

1. `manterConexoes()`: Wi-Fi, MQTT e processamento de mensagens recebidas.
2. `lerBotao()`.
3. A cada `intervaloLeitura`, `lerSensores()`: lê os DHT22, avança a simulação dos pneus, recalcula os alarmes e imprime o estado.
4. `controlarAlertas()` e `atualizarSom()`.
5. Com o MQTT conectado: publica os sensores a cada 5 s, publica `a` quando muda, e publica `s` quando muda e a cada 10 s.

Nada usa `delay()`. Os tempos vêm de `millis()`.

## Leitura dos sensores

`lerDht()` rejeita uma leitura quando `getStatus()` não é `ERROR_NONE` ou quando a temperatura ou a umidade são `NaN`. Nesse caso o código:

- registra `ERRO: Falha na leitura do DHT <nome>` no serial;
- marca o sensor como inválido (`internoValido` ou `motorValido`);
- deixa de publicar os atributos daquele sensor naquele ciclo;
- mantém o estado anterior do alarme daquele sensor.

Não há média móvel nem filtro. Cada leitura válida é usada como veio, com uma casa decimal (`DHT_CASAS_DECIMAIS`).

## Simulação da pressão dos pneus

Os seis pneus são `p1` e `p2` (dianteiros esquerdo e direito), `p3` e `p4` (traseiros esquerdos) e `p5` e `p6` (traseiros direitos).

- No `setup()`, cada pneu começa em 110 PSI mais um valor aleatório entre -3,0 e +3,0, com `randomSeed(esp_random())`.
- A cada leitura, o código aplica `p = p + (110 - p) * 0,05 + ruído`, com ruído aleatório de até ±0,3 PSI.
- O resultado é limitado entre 100 e 120 PSI.

| Constante | Valor |
|---|---|
| `NUM_PNEUS` | 6 |
| `PRESSAO_NOMINAL_PSI` | 110,0 |
| `PRESSAO_RUIDO_PSI` | 0,3 |
| `PRESSAO_RETORNO` | 0,05 |
| `PRESSAO_SIM_MIN_PSI` | 100,0 |
| `PRESSAO_SIM_MAX_PSI` | 120,0 |

A faixa de 100 a 120 PSI é o limite da simulação, não um limite de alarme. O firmware não tem alarme de pressão. Para usar sensores reais, substitua `simularPneus()`.

## Estados e alarmes

| Variável | Significado |
|---|---|
| `alertaAtivo` | Alerta aberto pelo botão. Só cai com `resolverAlerta` |
| `alarmeTempInterna` | Temperatura interna acima de `TEMP_INTERNA_MAX` |
| `alarmeTempMotor` | Temperatura do motor acima de `TEMP_MOTOR_MAX` |
| `remotoTemperatura` | Alarme de temperatura forçado pelo comando `alertTemp` |
| `buzzerForcado` | Tom contínuo ligado por `buzzer_on` |
| `buzzerSilenciado` | Buzzer silenciado por `buzzer_off` até a anomalia terminar |

Duas funções resumem o estado:

- `existeAlerta()` é verdadeira se `alertaAtivo`, `alarmeTempInterna`, `alarmeTempMotor` ou `remotoTemperatura` estiver ligado.
- `atuadoresAtivos()` é verdadeira se `buzzerForcado` estiver ligado, ou se `existeAlerta()` for verdadeira e o buzzer não estiver silenciado. É esse valor que vira `s|on` ou `s|off`.

### Botão de alerta

1. O botão é lido com debounce de 50 ms e a ação ocorre na borda de descida (HIGH para LOW).
2. Se não havia alerta, `alertaAtivo` passa a verdadeiro, o silenciamento é desfeito e o padrão sonoro toca na hora. O serial mostra `ALERTA: acionado pelo motorista (onibus com problema)`.
3. Se o alerta já estava ativo, o serial mostra que o sistema web ainda precisa resolvê-lo e nada muda.
4. O alerta só cai com o comando `resolverAlerta`.

O firmware envia apenas `true` ou `false` em `a`. Ele não diferencia o motivo da emergência.

### Alarmes de temperatura

`alarmeTempInterna` e `alarmeTempMotor` são recalculados a cada leitura válida. Eles caem sozinhos quando a leitura volta ao limite.

O alarme remoto `remotoTemperatura` é ligado por `alertTemp` e desligado por `alertOff`. O código também o zera em `lerSensores()` sempre que nenhum alarme local de temperatura está ativo. Na prática, com os sensores em valores normais, o `alertTemp` dura até a próxima leitura (cerca de 2 s). Ele só se mantém enquanto houver um alarme local.

### Buzzer

O padrão que toca depende da prioridade.

| Prioridade | Condição | Padrão | Repete a cada |
|---|---|---|---|
| 1 | Botão do motorista | 3 bips de 3000 Hz, 250 ms cada, um a cada 350 ms | 1,0 s |
| 2 | Temperatura interna ou `alertTemp` | 2 bips de 1800 Hz, 180 ms cada, um a cada 220 ms | 1,8 s |
| 3 | Temperatura do motor | 1 som de 1000 Hz, 500 ms | 1,8 s |

Outros estados do buzzer:

- Com `buzzer_on`, toca um tom contínuo de 2000 Hz e o padrão de alerta fica suspenso.
- Com `buzzer_off`, o buzzer para. Se ainda houver anomalia, fica silenciado até ela terminar. Quando todos os alertas somem, o silenciamento é desfeito.
- O botão do motorista e o `alertTemp` desfazem um silenciamento anterior e voltam a soar.
- Sem alerta, o buzzer fica em silêncio.

Um novo padrão toca quando o tipo de alerta muda ou quando o intervalo de repetição termina. Os bips seguintes ao primeiro saem em `atualizarSom()`, espaçados por `somPeriodo`, sem `delay()`.

## Conexão

### Wi-Fi

`connectWiFi()` faz no máximo uma tentativa a cada 10 s e não bloqueia. Cada tentativa chama `WiFi.disconnect()` e depois `WiFi.begin()`. As mudanças de estado (conectado, perdido) vão ao serial.

### MQTT

`connectMqtt()` só age com o Wi-Fi conectado e faz no máximo uma tentativa a cada 5 s. O `clientId` é `fiware-<DEVICE_ID>-<MAC do efuse>`, único por placa. Ao conectar:

1. assina `/<APIKEY>/<DEVICE_ID>/cmd`; se a assinatura falhar, desconecta e tenta de novo depois;
2. publica o estado do buzzer;
3. publica as leituras atuais.

Os tópicos e os formatos estão em [protocolo-mqtt.md](protocolo-mqtt.md).

## Funcionamento sem rede

Sem Wi-Fi, o `loop()` continua inteiro, exceto a publicação. O firmware lê os sensores, avalia os limites fixos, toca o buzzer e lê o botão, sem depender do dashboard.

Limites desse modo:

- O alerta do botão não pode ser resolvido, porque só o comando remoto o derruba. O buzzer também só é silenciado por comando.
- Nenhuma leitura é guardada. Na reconexão, só os valores atuais são publicados.
- Comandos não chegam.
- Com o Wi-Fi conectado e o broker inacessível, cada tentativa de `mqtt.connect` pode bloquear o `loop()` por até 3 s (`MQTT_TIMEOUT_S`), a cada 5 s. Nesse tempo os bips seguintes do buzzer atrasam e um clique rápido no botão pode passar sem leitura. O mesmo vale se o nome do broker não resolver no DNS. Esse comportamento foi deduzido do código e não foi medido.

## Saída no serial

A 115200 baud. Exemplos:

```
Temperatura interna: 27.4 C
Umidade interna: 52.0 %
Temperatura do motor: 85.1 C
Pressao dos pneus (PSI): p1=110.3 p2=109.8 p3=111.0 p4=110.1 p5=108.9 p6=110.6
STATUS: OPERACAO NORMAL
ALERTA: acionado pelo motorista (onibus com problema)
Alerta sonoro: BOTAO DO MOTORISTA
ALARME: TEMPERATURA INTERNA ACIMA DO LIMITE!
ERRO: Falha na leitura do DHT motor: ...
-> /TEF/onibus001/attrs/t 27.4 (true)
<- /TEF/onibus001/cmd onibus001@resolverAlerta|
ALERTA: resolvido pelo sistema web
```

`->` marca uma publicação e mostra o retorno do `publish`. `<-` marca uma mensagem recebida.

## Mapa do código

| Função | Papel |
|---|---|
| `connectWiFi()`, `connectMqtt()`, `manterConexoes()` | Conexão sem bloqueio |
| `montarTopico()`, `publicar()`, `publicarAtributo()` | Montagem e envio de mensagens |
| `publicarSensores()`, `publicarAlerta()`, `publicarEstado()` | Publicação dos atributos |
| `executarComando()`, `onMessage()` | Interpretação dos comandos e confirmação em `cmdexe` |
| `lerBotao()` | Debounce e detecção de borda |
| `lerDht()`, `lerSensores()` | Leitura dos DHT22 e cálculo dos alarmes |
| `simularPneus()`, `lerPneus()` | Simulação da pressão dos pneus |
| `controlarAlertas()` | Escolha do padrão sonoro pela prioridade |
| `emitirSom()`, `atualizarSom()` | Sequência de tons sem `delay()` |
| `existeAlerta()`, `atuadoresAtivos()` | Resumo do estado |
