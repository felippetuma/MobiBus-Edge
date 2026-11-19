# Manual de hardware

Voltar ao [índice](README.md). Próximo: [manual-software.md](manual-software.md).

## Componentes

| Componente | Quantidade | Uso |
|---|---|---|
| ESP32 DevKit (no Wokwi: `board-esp32-devkit-v1`) | 1 | Microcontrolador com Wi-Fi |
| DHT22 | 2 | Um para a temperatura e a umidade internas, outro para a temperatura do motor |
| Buzzer | 1 | Avisos sonoros |
| Botão (no Wokwi, vermelho) | 1 | Alerta de emergência do motorista |
| Protoboard mini | 1 | Distribuição de 3V3 e GND no circuito do Wokwi |

## Pinos

| Pino do ESP32 | Componente | Constante no código | Observação |
|---|---|---|---|
| GPIO 22 | DHT22 interno (SDA) | `PIN_DHT_INTERNO` | Temperatura e umidade do interior |
| GPIO 21 | DHT22 do motor (SDA) | `PIN_DHT_MOTOR` | Temperatura do motor |
| GPIO 19 | Buzzer (pino 2) | `PIN_BUZZER` | Saída, tocada com `tone()` |
| GPIO 18 | Botão (pino 1) | `PIN_BOTAO` | `INPUT_PULLUP`: solto é HIGH, apertado é LOW |
| 3V3 | VCC dos dois DHT22 | | Pela linha de 3V3 da protoboard |
| GND | GND dos DHT22, do buzzer e do botão | | Pela linha de GND da protoboard |

## Ligação no circuito do Wokwi

O arquivo [`wokwi/diagram.json`](../wokwi/diagram.json) descreve o circuito.

| De | Para |
|---|---|
| `esp:3V3` | linha de 3V3 da protoboard |
| `esp:GND.1` | linha de GND da protoboard |
| `dht1:VCC` e `dht2:VCC` | linha de 3V3 |
| `dht1:GND` e `dht2:GND` | linha de GND |
| `dht1:SDA` | `esp:D22` |
| `dht2:SDA` | `esp:D21` |
| `bz1:1` | linha de GND |
| `bz1:2` | `esp:D19` |
| `btn1:1.l` | `esp:D18` |
| `btn1:2.l` | linha de GND |
| `esp:TX` e `esp:RX` | `$serialMonitor` (RX e TX cruzados) |

Os dois DHT22 têm rótulos de texto no diagrama: "Sensor Interno" (`dht1`) e "Sensor Motor" (`dht2`). O volume do buzzer está em 0,1.

## Faixas de medição

| Sensor | Faixa do DHT22 | Limite de alarme no firmware |
|---|---|---|
| Temperatura interna | -40 a 80 °C | acima de 35,0 °C |
| Temperatura do motor | -40 a 80 °C | acima de 79,0 °C |
| Umidade | 0 a 100 % | sem limite |

O DHT22 não passa de 80 °C. Por isso o alarme do motor só dispara entre 79,1 e 80 °C. Um motor real passa disso com facilidade, então a versão final precisa de outro sensor (por exemplo, um termopar com amplificador). Só a leitura do `dhtMotor` muda: o restante do código aceita qualquer fonte de temperatura.

## Pressão dos pneus

Não há sensor de pressão no circuito. Os valores de `p1` a `p6` vêm de uma simulação em software descrita em [firmware.md](firmware.md#simulação-da-pressão-dos-pneus). Para usar sensores reais, troque a função `simularPneus()`.

## Notas para montagem em hardware real

Estas notas são orientações gerais de montagem. Não foram testadas neste projeto.

- Muitos módulos DHT22 já trazem o resistor de pull-up da linha de dados. Se o módulo for o componente solto de quatro pinos, use um resistor de cerca de 10 kΩ entre a linha de dados e o 3V3.
- O código varia a frequência com `tone()`, então o buzzer precisa ser passivo. Um buzzer ativo emite um tom fixo e os padrões dos três alertas não se distinguiriam pela altura.
- O botão usa o pull-up interno do ESP32. A outra perna vai direto ao GND, sem resistor.
- O ESP32 trabalha com 3,3 V. Alimente os DHT22 em 3V3.

## Problemas conhecidos do circuito

- Os nomes `esp:TX` e `esp:RX` na ligação com o monitor serial podem não ser os nomes dos pinos desta placa no Wokwi (o nome pode ser `TX0` e `RX0`). Isso não foi confirmado. Se o monitor serial do Wokwi ficar vazio, teste trocar os nomes no `diagram.json`.
