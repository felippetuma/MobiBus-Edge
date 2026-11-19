# Manual de software

Voltar ao [índice](README.md). Antes deste manual: [manual-hardware.md](manual-hardware.md).

Os comandos usam `<IP_DA_EC2>` para o IP público da instância. Não grave senhas nem tokens em arquivos versionados.

## 1. Instância na AWS

| Item | Valor |
|---|---|
| Sistema | Ubuntu Server LTS |
| Mínimo | 1 vCPU, 1 GB de RAM, 20 GB de disco |

Regras de entrada do grupo de segurança:

| Porta | Serviço | Origem |
|---|---|---|
| 22 | SSH | Seu IP |
| 1883 | Mosquitto (ESP32) | IPs do grupo e do laboratório |
| 4041 | IoT Agent, via Nginx (dashboard) | IPs do grupo |
| 8666 | STH-Comet, via Nginx (dashboard) | IPs do grupo |
| 1026 | Orion (dashboard) | IPs do grupo |

Não abra a 27017 (MongoDB). Para o Wokwi funcionar, a 1883 precisa estar acessível pela internet, porque a simulação roda na nuvem.

## 2. Subir o FIWARE

A stack está na pasta `fiware/` deste repositório: `docker-compose.yml`, `mosquitto/mosquitto.conf` e `nginx/default.conf`. Ela parte do repositório FIWARE do professor Fabio Cabrini (https://github.com/fabiocabrini/fiware) e inclui uma modificação que o original não tem: o Nginx (`nginx/default.conf` e o serviço `nginx` do compose), que expõe o IoT Agent e o STH-Comet com CORS aberto. Por isso, suba a stack a partir desta pasta, e não do repositório original. Não comparei o restante da pasta com o original.

```bash
sudo apt update
sudo apt install docker.io docker-compose
git clone <URL_DO_REPOSITORIO_DO_FIRMWARE>
cd <PASTA_DO_REPOSITORIO>/fiware
```

O Mosquitto lê a configuração de `fiware/mosquitto/mosquitto.conf`. O compose monta esse arquivo em `/mosquitto/config/mosquitto.conf` (`fiware/docker-compose.yml:76`, caminho relativo à pasta do compose), então rode os comandos de dentro de `fiware/`. Sem o arquivo no lugar, o Docker cria uma pasta com esse nome e o broker não sobe.

O arquivo atual abre a porta 1883 (MQTT) com `allow_anonymous true`, ou seja, sem usuário e senha, e a 9001 (WebSocket). O compose publica as duas, mas o projeto só usa a 1883. As mensagens do broker vão para o log do container.

Em seguida suba a stack:

```bash
sudo docker-compose up -d
sudo docker-compose ps
```

Todos os containers devem aparecer como `Up`. O compose traz um usuário e senha do MongoDB direto no arquivo. Troque por valores próprios na sua instância e não envie a troca ao repositório público.

Serviços da stack:

| Container | Função | Porta no host |
|---|---|---|
| `fiware-mongo-historical` | MongoDB do histórico, com autenticação | 27017 (não abrir no grupo de segurança) |
| `fiware-mongo-internal` | MongoDB do IoT Agent, sem autenticação | nenhuma |
| `fiware-orion` | Context Broker | 1026 |
| `fiware-sth-comet` | Histórico | 8666, via Nginx |
| `fiware-mosquitto` | Broker MQTT | 1883 e 9001 |
| `fiware-iot-agent` | IoT Agent UltraLight 2.0 | 4041, via Nginx |
| `fiware-nginx` | Proxy com CORS para o IoT Agent e o STH-Comet | 4041 e 8666 |

Verifique o Mosquitto: o log deve mostrar o listener da porta 1883, e uma publicação de teste deve chegar a um assinante (os comandos `mosquitto_sub` e `mosquitto_pub` vêm do pacote `mosquitto-clients`).

```bash
sudo docker logs fiware-mosquitto
mosquitto_sub -h <IP_DA_EC2> -t "teste" -C 1 &
mosquitto_pub -h <IP_DA_EC2> -t "teste" -m "ok"
```

Verifique os demais serviços:

```bash
curl -s http://<IP_DA_EC2>:1026/version
curl -s http://<IP_DA_EC2>:4041/iot/about
curl -s http://<IP_DA_EC2>:8666/version
```

Encerrar e reiniciar:

```bash
sudo docker-compose down
sudo docker-compose up -d
```

Os dados ficam em volumes e sobrevivem ao `down`.

## 3. Provisionar o service group

O IoT Agent só aceita mensagens MQTT de uma `APIKEY` conhecida. O firmware usa `TEF`, `fiware-service: smart` e `fiware-servicepath: /`.

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

Confira:

```bash
curl -s "http://<IP_DA_EC2>:4041/iot/services" \
  -H "fiware-service: smart" -H "fiware-servicepath: /"
```

Este passo não está automatizado no repositório. Valide a resposta na sua instância antes de seguir.

## 4. Cadastrar o dispositivo

O `device_id` precisa ser igual a `DEVICE_ID` do firmware (`onibus001`). Há duas formas de cadastrar:

- Pelo dashboard, se ele tiver essa função. Consulte o repositório do dashboard (campo de link no [índice](README.md#links-do-projeto)).
- Por `curl`, com o exemplo completo em [protocolo-mqtt.md](protocolo-mqtt.md#dispositivo). Ele cria a entidade `urn:ngsi-ld:Onibus:001`, os atributos `t`, `h`, `m`, `p1` a `p6`, `a` e `s`, e os cinco comandos.

Depois crie a subscription que alimenta o histórico, também em [protocolo-mqtt.md](protocolo-mqtt.md#subscription-para-o-histórico). Se o dashboard já cria essa subscription, não crie outra: o histórico passaria a gravar pontos duplicados.

Se o dispositivo for cadastrado sem o atributo `s`, as mensagens de estado do buzzer podem gerar erro no IoT Agent. O efeito não foi verificado: confira o log (`sudo docker logs fiware-iot-agent`) na primeira conexão.

## 5. Dashboard

O dashboard fica em repositório próprio (campo de link no [índice](README.md#links-do-projeto)). As instruções de instalação e execução estão no README dele.

Configurações que ele precisa receber:

| Campo | Valor |
|---|---|
| IP do FIWARE | IP público da EC2. Não use `localhost` |
| `fiware-service` | `smart` |
| `fiware-servicepath` | `/` |
| Tipo da entidade | `Onibus` |
| URL de notificação do STH-Comet | `http://sth-comet:8666/notify` |
| Nomes dos atributos | Os de [protocolo-mqtt.md](protocolo-mqtt.md#mapeamento-no-orion), ou os que foram provisionados |

## 6. Verificar o fluxo

Telemetria chegando no broker:

```bash
mosquitto_sub -h <IP_DA_EC2> -t "/TEF/#" -v
```

Valor atual no Orion:

```bash
curl -s "http://<IP_DA_EC2>:1026/v2/entities/urn:ngsi-ld:Onibus:001?options=keyValues" \
  -H "fiware-service: smart" -H "fiware-servicepath: /"
```

Histórico no STH-Comet:

```bash
curl -s "http://<IP_DA_EC2>:8666/STH/v1/contextEntities/type/Onibus/id/urn:ngsi-ld:Onibus:001/attributes/temperaturaInterna?lastN=5" \
  -H "fiware-service: smart" -H "fiware-servicepath: /"
```

Comando manual, sem o dashboard:

```bash
curl -s -X PATCH "http://<IP_DA_EC2>:1026/v2/entities/urn:ngsi-ld:Onibus:001/attrs" \
  -H "Content-Type: application/json" \
  -H "fiware-service: smart" -H "fiware-servicepath: /" \
  -d '{"buzzer_on": {"type": "command", "value": ""}}'
```

No serial do ESP32 deve aparecer `<- /TEF/onibus001/cmd onibus001@buzzer_on|` e depois `-> /TEF/onibus001/cmdexe onibus001@buzzer_on|ok`. O roteiro completo de testes está em [testes.md](testes.md).

## 7. Reset completo

Apaga entidades, subscriptions e todo o histórico.

```bash
sudo docker-compose down
sudo docker volume ls
sudo docker volume rm fiware_mongo-historical-data
sudo docker volume rm fiware_mongo-internal-data
```

O prefixo `fiware_` vem do nome da pasta do compose. Use o nome que o `docker volume ls` mostrar. Depois do reset, repita as seções 2 a 4.
