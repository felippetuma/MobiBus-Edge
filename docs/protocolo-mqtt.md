# Protocolo MQTT e integração com o FIWARE

Voltar ao [índice](README.md).

O firmware usa o protocolo UltraLight 2.0 do IoT Agent, com o Mosquitto como broker. Nos tópicos, `<apikey>` é `APIKEY` (`TEF`) e `<device>` é `DEVICE_ID` (`onibus001`).

## Publicação (ESP32 para o broker)

| Tópico | Payload | Unidade | Quando |
|---|---|---|---|
| `/<apikey>/<device>/attrs/t` | `27.5` | °C | A cada 5 s, se a leitura for válida |
| `/<apikey>/<device>/attrs/h` | `60.0` | % | A cada 5 s, se a leitura for válida |
| `/<apikey>/<device>/attrs/m` | `82.0` | °C | A cada 5 s, se a leitura for válida |
| `/<apikey>/<device>/attrs/p1` a `p6` | `110.3` | PSI, simulado | A cada 5 s |
| `/<apikey>/<device>/attrs/a` | `true` ou `false` | | A cada 5 s e ao mudar |
| `/<apikey>/<device>/attrs` | `s\|on` ou `s\|off` | | Ao mudar e a cada 10 s |
| `/<apikey>/<device>/cmdexe` | `onibus001@<cmd>\|ok` | | Depois de cada comando |

Observações:

- `t`, `h`, `m` e os `p` usam um tópico por atributo, com valor puro e uma casa decimal.
- `s` usa o tópico base `attrs` com o formato `chave|valor`.
- Cada ciclo de 5 s envia dez mensagens (`t`, `h`, `m`, seis de pressão e `a`). `s` sai a cada 10 s.
- Ao conectar ao broker, o firmware publica `s` e as leituras atuais sem esperar o intervalo.
- Se um DHT22 falha, os atributos dele não saem naquele ciclo.
- Não há timestamp na mensagem. O IoT Agent registra o horário (`IOTA_TIMESTAMP=true` no compose).

### Significado dos atributos

| Chave | Medida | Observação |
|---|---|---|
| `t` | Temperatura interna | DHT22 do GPIO 22 |
| `h` | Umidade interna | Mesmo sensor de `t` |
| `m` | Temperatura do motor | DHT22 do GPIO 21 |
| `p1`, `p2` | Pneus dianteiros esquerdo e direito | Simulados |
| `p3`, `p4` | Pneus traseiros esquerdos | Simulados |
| `p5`, `p6` | Pneus traseiros direitos | Simulados |
| `a` | Alerta do botão do motorista | Não reflete os alarmes de temperatura |
| `s` | Estado do buzzer | `on` com qualquer alerta tocando ou com `buzzer_on`; `off` se silenciado, mesmo com alarme ativo |

## Comandos (broker para o ESP32)

O firmware assina `/<apikey>/<device>/cmd`. O formato é `<device>@<comando>|<parâmetros>`. Comandos para outro `device_id` são ignorados, e mensagens vazias ou com 64 bytes ou mais são descartadas.

| Comando | Efeito |
|---|---|
| `resolverAlerta` | Encerra o alerta aberto pelo botão. `a` volta a `false` |
| `buzzer_on` | Tom contínuo de 2000 Hz |
| `buzzer_off` | Para o buzzer e silencia até a anomalia atual terminar |
| `alertTemp` | Liga o alarme remoto de temperatura (veja a ressalva abaixo) |
| `alertOff` | Desliga o alarme remoto de temperatura |

Ressalva sobre `alertTemp`: o firmware zera esse alarme a cada leitura em que nenhum alarme local de temperatura está ativo. Com os sensores normais, ele dura cerca de 2 s. Um botão de dashboard para `alertTemp` só terá efeito duradouro se houver um alarme local, ou se essa regra for removida do firmware.

A resposta vai em `/<apikey>/<device>/cmdexe`:

- `onibus001@<comando>|ok` quando o comando existe;
- `onibus001@<comando>|unknown command` quando não existe.

Depois de cada comando, o firmware reaplica os alertas e republica `a` e `s`.

Exemplo com o cliente do Mosquitto:

```bash
mosquitto_pub -h <IP_DA_EC2> -t "/TEF/onibus001/cmd" -m "onibus001@resolverAlerta|"
```

## Mapeamento no Orion

O nome que o Orion guarda vem do `object_id` para `name` provisionado no IoT Agent. O firmware só conhece as chaves curtas. Os nomes abaixo são uma sugestão e precisam coincidir com os que o dashboard usa.

| `object_id` | Atributo no Orion | Tipo |
|---|---|---|
| `t` | `temperaturaInterna` | Number |
| `h` | `umidadeInterna` | Number |
| `m` | `temperaturaMotor` | Number |
| `p1` a `p6` | `pressaoPneu1` a `pressaoPneu6` | Number |
| `a` | `alerta` | Text (`"true"` ou `"false"`) |
| `s` | `buzzer` | Text (`"on"` ou `"off"`) |

Se o IoT Agent já foi provisionado com outros nomes (por exemplo, `temperatura` no lugar de `temperaturaInterna`), o dashboard usa os nomes provisionados.

## Provisionamento

### Service group

O IoT Agent só aceita mensagens de uma `APIKEY` conhecida.

```bash
curl -s -X POST "http://<IP_DA_EC2>:4041/iot/services" \
  -H "Content-Type: application/json" \
  -H "fiware-service: smart" \
  -H "fiware-servicepath: /" \
  -d '{
    "services": [{
      "apikey": "TEF",
      "cbroker": "http://orion:1026",
      "entity_type": "Onibus",
      "resource": ""
    }]
  }'
```

### Dispositivo

```bash
curl -s -X POST "http://<IP_DA_EC2>:4041/iot/devices" \
  -H "fiware-service: smart" -H "fiware-servicepath: /" \
  -H "Content-Type: application/json" -d '{
    "devices": [{
      "device_id": "onibus001",
      "entity_name": "urn:ngsi-ld:Onibus:001",
      "entity_type": "Onibus",
      "protocol": "PDI-IoTA-UltraLight",
      "transport": "MQTT",
      "attributes": [
        {"object_id": "t", "name": "temperaturaInterna", "type": "Number"},
        {"object_id": "h", "name": "umidadeInterna", "type": "Number"},
        {"object_id": "m", "name": "temperaturaMotor", "type": "Number"},
        {"object_id": "p1", "name": "pressaoPneu1", "type": "Number"},
        {"object_id": "p2", "name": "pressaoPneu2", "type": "Number"},
        {"object_id": "p3", "name": "pressaoPneu3", "type": "Number"},
        {"object_id": "p4", "name": "pressaoPneu4", "type": "Number"},
        {"object_id": "p5", "name": "pressaoPneu5", "type": "Number"},
        {"object_id": "p6", "name": "pressaoPneu6", "type": "Number"},
        {"object_id": "a", "name": "alerta", "type": "Text"},
        {"object_id": "s", "name": "buzzer", "type": "Text"}
      ],
      "commands": [
        {"name": "resolverAlerta", "type": "command"},
        {"name": "alertTemp", "type": "command"},
        {"name": "alertOff", "type": "command"},
        {"name": "buzzer_on", "type": "command"},
        {"name": "buzzer_off", "type": "command"}
      ]
    }]
  }'
```

Os nomes das entidades e dos atributos são sugestões. Valide a resposta do IoT Agent na primeira execução.

### Subscription para o histórico

O STH-Comet só grava o que o Orion notifica. O exemplo abaixo cria uma subscription para todas as entidades do tipo `Onibus`. Se o dashboard já cria essa subscription, não crie outra: o histórico passaria a gravar pontos duplicados.

```bash
curl -s -X POST "http://<IP_DA_EC2>:1026/v2/subscriptions" \
  -H "Content-Type: application/json" \
  -H "fiware-service: smart" -H "fiware-servicepath: /" \
  -d '{
    "description": "Histórico dos ônibus no STH-Comet",
    "subject": {
      "entities": [{"idPattern": ".*", "type": "Onibus"}],
      "condition": {"attrs": []}
    },
    "notification": {
      "http": {"url": "http://sth-comet:8666/notify"},
      "attrsFormat": "legacy"
    }
  }'
```

## Consultas úteis

Valor atual no Orion:

```bash
curl -s "http://<IP_DA_EC2>:1026/v2/entities/urn:ngsi-ld:Onibus:001?options=keyValues" \
  -H "fiware-service: smart" -H "fiware-servicepath: /"
```

Últimas cinco leituras de um atributo no STH-Comet:

```bash
curl -s "http://<IP_DA_EC2>:8666/STH/v1/contextEntities/type/Onibus/id/urn:ngsi-ld:Onibus:001/attributes/temperaturaInterna?lastN=5" \
  -H "fiware-service: smart" -H "fiware-servicepath: /"
```

Enviar um comando pelo Orion:

```bash
curl -s -X PATCH "http://<IP_DA_EC2>:1026/v2/entities/urn:ngsi-ld:Onibus:001/attrs" \
  -H "Content-Type: application/json" \
  -H "fiware-service: smart" -H "fiware-servicepath: /" \
  -d '{"resolverAlerta": {"type": "command", "value": ""}}'
```

## Como o dashboard sabe que um limite foi ultrapassado

O firmware não publica o estado dos alarmes de temperatura. O atributo `a` vale só para o botão, e `s` diz apenas se o buzzer está tocando. Hoje o dashboard precisa comparar as leituras com os limites do firmware (35 °C para a temperatura interna e 79 °C para o motor). Esses valores ficam duplicados e podem divergir. Veja a alternativa em [limitacoes-e-melhorias.md](limitacoes-e-melhorias.md#decisões-em-aberto).
