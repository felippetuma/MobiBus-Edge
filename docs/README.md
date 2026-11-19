# Documentação do MobiBus

O MobiBus monitora um ônibus com um ESP32: temperatura interna, umidade, temperatura do motor, pressão dos pneus (simulada) e um botão de emergência para o motorista. Os dados vão por MQTT para uma stack FIWARE, e um dashboard lê e comanda o ônibus a partir dela.

## Links do projeto

Preencha os campos abaixo quando os links existirem.

| Item | Link |
|---|---|
| Simulação no Wokwi |  |
| Dashboard hospedado na Vercel | [`Link do Site do MobiBus na Vercel`](https://mobi-bus-gs.vercel.app/) |
| Vídeo no YouTube | [`Link do Video no Youtube`](https://youtu.be/duI55tUqWwA) |
| Repositório do firmware (este projeto) | [`Repositório MobiBus-Edge`](https://github.com/felippetuma/MobiBus-Edge) |
| Repositório do dashboard | [`Repositório do projeto MobiBus`](https://github.com/felippetuma/MobiBus-GS) |
| FIWARE original (professor Fabio Cabrini), só como referência | <https://github.com/fabiocabrini/fiware> |

## Índice

| Documento | Conteúdo |
|---|---|
| [visao-geral.md](visao-geral.md) | O que o sistema faz, arquitetura e o que ficou de fora |
| [manual-hardware.md](manual-hardware.md) | Componentes, pinos, ligação e circuito do Wokwi |
| [firmware.md](firmware.md) | Como o código funciona: constantes, leituras, alarmes, buzzer e comportamento sem rede |
| [protocolo-mqtt.md](protocolo-mqtt.md) | Tópicos, payloads, comandos e provisionamento no IoT Agent |
| [manual-software.md](manual-software.md) | Subir a stack FIWARE na AWS, provisionar e verificar o fluxo |
| [testes.md](testes.md) | Roteiro de testes no Wokwi e de ponta a ponta |
| [limitacoes-e-melhorias.md](limitacoes-e-melhorias.md) | Limitações conhecidas, decisões em aberto e melhorias |

## Ordem de leitura sugerida

1. Para entender o projeto: [visao-geral.md](visao-geral.md).
2. Para rodar a simulação: [manual-hardware.md](manual-hardware.md) e a seção "Executar no Wokwi" de [testes.md](testes.md).
3. Para integrar com o FIWARE: [manual-software.md](manual-software.md) e [protocolo-mqtt.md](protocolo-mqtt.md).
4. Para alterar o código: [firmware.md](firmware.md).

## Estrutura do repositório

| Caminho | Conteúdo |
|---|---|
| `firmware/Mobibus/mobibus.ino` | Firmware do ESP32 |
| `wokwi/diagram.json` | Circuito da simulação |
| `wokwi/libraries.txt` | Bibliotecas instaladas pelo Wokwi |
| `fiware/` | `docker-compose.yml`, configuração do Mosquitto e do Nginx (stack do projeto, baseada no [repositório do professor Fabio Cabrini](https://github.com/fabiocabrini/fiware), com o Nginx acrescentado) |
| `docs/` | Esta documentação |
| `README.md` | Resumo do firmware e ponto de entrada |

## Estado da verificação

O firmware e esta documentação foram escritos a partir da leitura do código. O firmware não foi compilado nem executado em nenhuma ferramenta, e a stack FIWARE não foi subida durante a escrita. Os comandos de `curl` e de `docker` seguem o padrão do FIWARE e precisam ser validados na primeira execução. Os pontos que dependem dessa validação estão marcados nos documentos.

Autores do firmware: Fabio Carini  