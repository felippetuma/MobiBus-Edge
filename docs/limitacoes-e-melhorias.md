# Limitações e melhorias

Voltar ao [índice](README.md).

## Limitações do firmware

| Limitação | Efeito | Origem |
|---|---|---|
| Alarme do motor a 1 °C do fundo de escala do sensor | O limite de 79 °C só dispara entre 79,1 e 80 °C | DHT22 lê até 80 °C |
| `alertTemp` dura cerca de 2 s sem alarme local | Um botão de dashboard para esse comando quase não tem efeito | O código zera `remotoTemperatura` a cada leitura sem alarme local |
| Alarmes de temperatura não são publicados | O dashboard precisa repetir os limites | Só `a` (botão) e `s` (buzzer) saem |
| `a` não diferencia o motivo da emergência | O suporte técnico vê só `true` ou `false` | O firmware envia um booleano |
| Pressão dos pneus é simulada e sem alarme | Nenhum aviso de pneu baixo | Não há sensor nem limite |
| Sem GPS nem lotação | Não há localização nem contagem de passageiros | Fora do escopo implementado |
| Alerta do botão só cai por comando remoto | Sem rede, o alerta não pode ser encerrado no ônibus | Sem saída offline |
| Alerta perdido ao reiniciar | `alertaAtivo` fica só na RAM | Sem memória não volátil |
| Sem armazenamento durante a falta de rede | Os dados desse período se perdem | Publicação só com o MQTT conectado |
| `mqtt.connect` pode bloquear o `loop()` por até 3 s | Bips atrasados e cliques rápidos perdidos com Wi-Fi mas sem broker | `MQTT_TIMEOUT_S = 3`. Deduzido do código, não medido |
| `buzzer_off` silencia também o alerta do botão | `a` continua `true` e o buzzer fica mudo | Silenciamento vale até a anomalia terminar |
| Alarme preso por sensor com defeito | Um DHT22 que falha depois de um alarme mantém o alarme | Leitura inválida não recalcula o alarme |
| Sem testes automatizados | Nada protege contra regressões | |
| Não compilado nem executado | Podem existir avisos ou erros de compilação | Sem toolchain no ambiente de escrita |

## Limitações da stack FIWARE

Estes pontos estão em `fiware/` e merecem atenção antes de qualquer uso fora do laboratório.

| Ponto | Detalhe |
|---|---|
| MQTT sem autenticação | `allow_anonymous true` no Mosquitto. Qualquer cliente com acesso à porta 1883 publica e comanda |
| Credenciais do MongoDB no compose | Usuário `admin` e senha `admin123` no `docker-compose.yml`. Troque na instância e não envie a troca ao repositório público |
| MongoDB exposto no host | O compose publica a 27017. O manual manda não abrir essa porta no grupo de segurança |
| Sem TLS | MQTT e HTTP em texto claro |
| `iotagent-ul:latest` | Versão não fixada: uma atualização da imagem pode mudar o comportamento |
| Orion depende do MongoDB do histórico | O compose faz o Orion usar `mongo-db-historical`, apesar do comentário dizer que seria o interno |

## Decisões em aberto

### Como o dashboard detecta um limite ultrapassado

| Opção | Vantagem | Desvantagem |
|---|---|---|
| a) O dashboard compara a leitura com os limites | Funciona hoje, sem mudar o firmware | O limite fica duplicado e pode divergir do firmware |
| b) O firmware publica atributos de alarme (por exemplo, `at` para a temperatura interna e `am` para o motor, com `true` ou `false`) | O limite fica só no ESP32 | Exige mudança no firmware e novos atributos no IoT Agent |

A opção (b) não existe hoje. A opção (a) é a única possível sem alterar o firmware.

### Limites de pressão dos pneus

O firmware não tem limite de pressão. Antes de qualquer alarme, a equipe precisa definir se o limite é mínimo, máximo ou ambos, e os valores.

### Comportamento do `alertTemp`

Se o comando deve persistir até um `alertOff`, remova a linha que zera `remotoTemperatura` em `lerSensores()` (`if (!alarmeTempInterna && !alarmeTempMotor) remotoTemperatura = false;`). Se o comportamento atual for o desejado, o dashboard deve tratá-lo como um aviso curto.

### Nomes dos atributos no Orion

Os nomes de [protocolo-mqtt.md](protocolo-mqtt.md#mapeamento-no-orion) são sugestões. Alinhe com o dashboard antes de provisionar.

## Melhorias sugeridas

1. Publicar os alarmes de temperatura (`at`, `am`) para o dashboard não duplicar limites.
2. Alarme de pressão baixa e alta, com padrão sonoro próprio e um comando para simular um furo.
3. Tipo de emergência no botão, ou um atributo extra com o motivo.
4. Clique longo no botão para encerrar o alerta sem rede (já citado em comentário no código).
5. Persistir `alertaAtivo` na memória não volátil.
6. Tornar a conexão MQTT sem bloqueio, ou reduzir o timeout, para não atrasar o buzzer e o botão.
7. Posição GPS no formato `lat,lon` e lotação, se forem requisitos do sistema.
8. Sensor de temperatura do motor com faixa adequada, para um motor real.
9. Alarme também para temperatura mínima.
10. Credenciais de Wi-Fi e do broker fora do código-fonte.
11. Autenticação no Mosquitto e senhas próprias no MongoDB.
12. Testes automatizados da lógica de alarmes.
