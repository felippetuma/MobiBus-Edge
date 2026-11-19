# Visão geral

Voltar ao [índice](README.md).

## Objetivo

Dar à operação de uma frota de ônibus uma leitura remota do estado de cada veículo e um canal de emergência. O motorista aperta um botão quando o ônibus quebra, e o sistema web recebe o alerta e o encerra depois que o suporte técnico atua.

## O que o firmware faz

- Mede a temperatura e a umidade internas e a temperatura do motor, com dois sensores DHT22.
- Simula a pressão de seis pneus. Não há sensor de pressão no hardware.
- Lê um botão de emergência e mantém o alerta aberto até o sistema web resolvê-lo.
- Toca um buzzer local com padrões diferentes para o botão, a temperatura interna e a temperatura do motor.
- Publica as medidas por MQTT no formato UltraLight 2.0, que o IoT Agent do FIWARE entende.
- Recebe comandos do sistema web (resolver alerta, ligar e desligar o buzzer, forçar o alarme de temperatura) e confirma cada um.
- Continua medindo, decidindo os alarmes e tocando o buzzer quando não há Wi-Fi ou broker. Veja [firmware.md](firmware.md#funcionamento-sem-rede).

## O que o firmware não faz

- Não envia posição GPS.
- Não envia lotação de passageiros.
- Não envia JSON nem timestamp. O formato é UltraLight 2.0 e o horário vem do IoT Agent.
- Não guarda leituras enquanto está sem rede.
- Não publica o estado dos alarmes de temperatura. O dashboard precisa comparar as leituras com os limites, ou o firmware precisa ganhar atributos de alarme (veja [limitacoes-e-melhorias.md](limitacoes-e-melhorias.md)).

## Arquitetura

![Arquitetura em camadas do MobiBus: dashboard, back-end FIWARE e ESP32](arquitetura/arquitetura-camadas.png)


Fluxo de telemetria: o ESP32 publica em `/TEF/onibus001/attrs/...`, o IoT Agent converte para a entidade `Onibus` no Orion, e o Orion notifica o STH-Comet, que guarda o histórico no MongoDB.

Fluxo de comando: o dashboard escreve um atributo de comando na entidade no Orion, o IoT Agent publica em `/TEF/onibus001/cmd`, o ESP32 executa e responde em `/TEF/onibus001/cmdexe`.

## Componentes

| Componente | Onde está | Função |
|---|---|---|
| Firmware | `firmware/Mobibus/mobibus.ino` | Leitura, alarmes e publicação |
| Circuito de simulação | `wokwi/` | Testar sem hardware |
| Stack FIWARE | `fiware/` | Mosquitto, IoT Agent, Orion, STH-Comet, MongoDB e Nginx |
| Dashboard | repositório próprio (veja o campo de link no [índice](README.md#links-do-projeto)) | Visualização e comandos |

## Identificadores

| Item | Valor |
|---|---|
| `APIKEY` | `TEF` |
| `DEVICE_ID` | `onibus001` |
| Entidade sugerida | `urn:ngsi-ld:Onibus:001` do tipo `Onibus` |
| `fiware-service` | `smart` |
| `fiware-servicepath` | `/` |

A `APIKEY` e o `DEVICE_ID` precisam ser iguais no firmware e no IoT Agent. A entidade e o tipo são sugestões e devem coincidir com o que o dashboard consulta.

## Glossário

| Termo | Significado |
|---|---|
| UltraLight 2.0 | Protocolo textual leve do IoT Agent. Uma medida é `chave\|valor`, ou um valor puro quando o tópico já nomeia o atributo |
| IoT Agent | Traduz as mensagens MQTT do dispositivo em atualizações NGSI no Orion e o inverso para comandos |
| Orion | Context Broker do FIWARE: guarda o estado atual das entidades |
| STH-Comet | Guarda o histórico das mudanças das entidades |
| Service group | Cadastro no IoT Agent que associa uma `APIKEY` a um tipo de entidade |
| PSI | Libra-força por polegada quadrada, a unidade da pressão dos pneus |
