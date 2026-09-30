Story 2 — a linha de contexto virou convite

Eu escrevi "Context: configuration of OS properties via REST API and websocket". O baseline tratou o transporte como assunto. O arquivo dele é uma suíte de teste de API:

When I send a GET request to "/api/v1/os/properties/audio"
Then the response status code is 200
And the response field "activeSource" is "hdmi"

Mais tópicos de websocket, tipos de mensagem, timeouts de 5 segundos. Com skill:

When a user asks the device for its audio configuration
Then the device reports the built-in microphone as the active audio source
And the device reports no other active audio source

A story diz "I want to know if my device is correctly configurated". O usuário quer saber algo; o baseline responde com chamadas HTTP

Substrato — a camada de vocabulário que fica embaixo de um domínio: as palavras de como o sistema guarda, transporta e manipula a coisa, em oposição às palavras do que acontece para alguém. O substrato de uma regra de firewall é a lista de regras, a chain, a ação de aplicar e a linha de log. O de um reembolso é a tabela de pagamentos e o lançamento contábil. Todo domínio tem um; o que varia é a distância.

Proximidade — essa distância. Não é a mesma coisa que "o domínio é técnico": os cinco domínios medidos são técnicos. É se o substrato está alcançável a partir das palavras que o usuário usou. Uma story sobre uma regra deixa a lista de regras a um sinônimo de distância. Uma story sobre se uma rodada está mais lenta que outra não deixa nada ao alcance, porque o que se pede é um juízo, e juízo não é objeto armazenado.

Por que "substrato" — no sentido comum: a superfície sobre a qual algo cresce. O domínio é aquilo de que o usuário fala; o substrato é aquilo sobre o qual está implementado. Um modelo sem guia, quando mandado inventar um cenário, tende a afundar para o substrato, porque essa é a camada concreta e a do domínio é a abstrata.
