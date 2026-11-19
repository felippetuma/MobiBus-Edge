# Testes

Voltar ao [índice](README.md).

Nenhum destes testes foi executado durante a escrita da documentação. Os resultados esperados saem da leitura do código. Anote aqui as divergências encontradas na primeira execução.

## Executar no Wokwi

1. Crie um projeto ESP32 no Wokwi e importe `firmware/Mobibus/mobibus.ino`, `wokwi/diagram.json` e `wokwi/libraries.txt`.
2. No `.ino`, use o Wi-Fi de teste do Wokwi:
   ```cpp
   #define WIFI_SSID      "Wokwi-GUEST"
   #define WIFI_PASSWORD  ""
   ```
3. Aponte `MQTT_BROKER` para um Mosquitto acessível pela internet. O Wokwi roda na nuvem e não alcança `localhost`. Sem broker, os testes locais da próxima seção ainda funcionam.
4. Inicie a simulação e abra o monitor serial a 115200 baud.
5. Cole o link do projeto no campo do [índice](README.md#links-do-projeto).

## Testes locais (só o ESP32)

| # | Ação | Resultado esperado |
|---|---|---|
| 1 | Iniciar a simulação | O serial mostra o banner "MOBIBUS" e "Sistema iniciado!" |
| 2 | Esperar 2 s com os sensores em valores normais | A cada 2 s aparecem temperatura, umidade, motor, pressão dos pneus e `STATUS: OPERACAO NORMAL` |
| 3 | Subir a temperatura do `dht1` acima de 35 °C | `ALARME: TEMPERATURA INTERNA ACIMA DO LIMITE!` e o buzzer toca 2 bips a cada 1,8 s |
| 4 | Voltar o `dht1` para um valor normal | O alarme termina e o buzzer para |
| 5 | Subir o `dht2` para 79,5 °C ou mais | `ALARME: TEMPERATURA DO MOTOR ACIMA DO LIMITE!` e o buzzer toca 1 som grave e longo a cada 1,8 s |
| 6 | Apertar o botão vermelho | `ALERTA: acionado pelo motorista (onibus com problema)` e 3 bips agudos a cada 1 s |
| 7 | Apertar o botão de novo com o alerta ativo | O serial informa que o alerta já está ativo, sem mudança |
| 8 | Alarme de temperatura e botão ao mesmo tempo | Toca o padrão do botão, que tem prioridade |
| 9 | Observar as pressões por alguns minutos | `p1` a `p6` ficam entre 100 e 120 PSI e variam pouco a cada leitura |
| 10 | Deixar o Wi-Fi com o SSID de exemplo | `WiFi: conectando a ...` a cada 10 s, e os testes 2 a 8 continuam funcionando |

O teste 10 confirma o funcionamento sem rede descrito em [firmware.md](firmware.md#funcionamento-sem-rede).

## Testes de integração (com o FIWARE no ar)

Pré-requisitos: stack no ar [manual-software.md](manual-software.md), service group e dispositivo provisionados [protocolo-mqtt.md](protocolo-mqtt.md#provisionamento), `MQTT_BROKER` apontando para a instância.

| # | Ação | Resultado esperado |
|---|---|---|
| 1 | `mosquitto_sub -h <IP_DA_EC2> -t "/TEF/#" -v` | A cada 5 s chegam `t`, `h`, `m`, `p1` a `p6` e `a`; `s` chega a cada 10 s |
| 2 | Consultar a entidade no Orion | Os atributos aparecem com os valores recentes |
| 3 | Consultar o STH-Comet | O histórico traz os pontos mais recentes |
| 4 | Apertar o botão | `a = true` chega ao broker em seguida, e a entidade mostra `alerta` verdadeiro |
| 5 | Enviar `resolverAlerta` pelo Orion | O serial mostra `ALERTA: resolvido pelo sistema web`, `cmdexe` traz `onibus001@resolverAlerta\|ok` e `a = false` é publicado |
| 6 | Enviar `buzzer_on` | O buzzer toca um tom contínuo e `s\|on` é publicado |
| 7 | Enviar `buzzer_off` | O buzzer para e `s\|off` é publicado |
| 8 | Enviar `alertTemp` com sensores normais | O padrão de temperatura toca e o alarme cai na leitura seguinte (cerca de 2 s) |
| 9 | Enviar `alertTemp` com a temperatura interna acima de 35 °C | O alarme continua enquanto o alarme local existir |
| 10 | Enviar um comando inexistente | `cmdexe` traz `unknown command` |
| 11 | Enviar um comando com outro `device_id` | O serial mostra `<- ignorado: device_id diferente` e nada é publicado em `cmdexe` |

O teste 8 documenta o comportamento atual: o `alertTemp` não persiste sem alarme local. Veja [limitacoes-e-melhorias.md](limitacoes-e-melhorias.md).

## Quando o serial do Wokwi não mostra nada

Siga na ordem:

1. Confira se a simulação compilou. Um erro de compilação aparece fora do monitor serial.
2. Se aparece o banner mas não aparecem as leituras, confira se o sensor está ligado ao pino correto ([manual-hardware.md](manual-hardware.md)).
3. Se nada aparece, teste trocar `esp:TX` e `esp:RX` por `esp:TX0` e `esp:RX0` no `diagram.json`. Essa causa não foi confirmada.
4. Confira se a velocidade do monitor é 115200 baud.