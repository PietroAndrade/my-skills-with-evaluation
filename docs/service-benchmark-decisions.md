# `service-benchmark` — origem e decisões de projeto

Registro de por que a skill `service-benchmark` foi criada, o que a
`cpp-parallel-benchmark` não cobria, e onde a lei de Amdahl deixou de ser útil.
Escrito para que as escolhas não sejam re-discutidas depois.

Origem: comparação A/B entre o DNS content filter legado e a reescrita, no repo
`dnsproxy`. Os relatórios completos estão em `docs/benchmarks/` daquele repo; as
referências a testes 001/002/003 abaixo apontam para lá.

---

## O gatilho

A tarefa era medir se a reescrita superava o código antigo. A `cpp-parallel-benchmark`
foi invocada — ela é a skill de benchmark do conjunto — e a primeira coisa que
ficou evidente é que o enquadramento não correspondia ao problema.

A skill assume duas implementações **dentro do mesmo binário**: uma sequencial,
outra paralela, ambas chamáveis como função, tempo medido em volta da chamada.
Aqui eram **dois processos separados**, falados por socket UDP. Não existe
chamada para cercar com `steady_clock`; existe requisição, rede, e resposta
correlacionada.

Isso não invalidou a skill — as regras de medição dela continuam corretas. Mas
ela não tem nada a dizer sobre a decisão que, neste tipo de medição, determina
todo o resto: **a forma do gerador de carga**.

---

## O que transferiu e o que faltou

| regra da `cpp-parallel-benchmark` | transferiu? | observação |
|---|---|---|
| 1. Média de N rodadas, mean + stddev + min | parcial | Correta, mas insuficiente. Ver "percentis" abaixo. |
| 2. Warm-up executado e descartado | sim | Vale igual; cache e páginas aquecem do mesmo jeito. |
| 3. Cronômetro só em volta da chamada medida | sim | Traduz para "só em volta do `send`/`recv`". |
| 4. `steady_clock`, não `system_clock` | sim | Idêntico. |
| 5. Verificar correção antes de velocidade | sim | Transferiu bem e foi valiosa — comparar respostas entre os binários é fácil e barato. |
| 6. Controlar o ambiente | sim, com acréscimo | Surge uma preocupação nova: cliente e servidor disputam as mesmas CPUs quando rodam no mesmo host. |
| 7. Cuidado com mutação in-place | não se aplica | Não há entrada mutada através de um socket. |
| 9. Impedir eliminação de código morto | não se aplica | O otimizador não elimina chamadas de sistema. |

O que **não existia em lugar nenhum** da skill:

### Forma do harness — a omissão cara

O harness inicial era closed-loop: N threads cliente, cada uma esperando a
resposta antes de enviar a próxima. É o desenho natural e o que a intuição de
"medir latência" produz.

Ele reportou **`drop_pct=0.000` em absolutamente todas as configurações** —
inclusive com o binário legado a 128 ms de latência média e concorrência 32.
A leitura ingênua seria "o serviço não perde nada".

A causa é estrutural: um cliente que só envia após receber nunca oferece mais
carga do que o servidor já está absorvendo. Sob sobrecarga as requisições
enfileiram e a latência sobe, mas nada se perde, porque nada foi oferecido que
pudesse se perder. O harness não estava medindo zero — estava **incapaz de fazer
a pergunta**.

Com um gerador open-loop (taxa fixa, independente das respostas), o mesmo
binário legado mostrou 24,9% de perda a 500 req/s e 100% a partir de 2000.

O que torna isso digno de virar skill: a falha é **silenciosa**. Não há erro, não
há aviso, o número sai com três casas decimais e uma aparência de medição. Um
`0.000` de closed-loop deve ser reportado como "não medido", nunca como zero.

### Correlação de resposta com requisição

Com mais de uma requisição em voo, uma resposta atrasada precisa ser casada com
a *sua* requisição, não com a que está sendo cronometrada no momento. Protocolos
carregam identificador para isso (ID de transação DNS, stream id em HTTP/2).

Sem isso o harness parece funcionar — envia, lê o próximo pacote do socket, chama
aquilo de resposta. O erro só aparece sob carga, e aparece como alargamento da
distribuição, indistinguível de jitter comum. Ou seja: os números ficam errados
exatamente quando o sistema está mais interessante, e nada parece quebrado.

### Percentis

A regra 1 pedia mean + stddev + min. Foi insuficiente de um jeito que quase
produziu a conclusão errada.

No Teste 002, o binário legado apresentou **média idêntica até a terceira casa
decimal** (4,022 / 4,023 / 4,024 ms) em três workloads substancialmente
diferentes, enquanto a **mediana respondia** ao workload (0,312 / 0,518 / 0,314 ms).

Só com a média, a conclusão seria "o legado é 19x mais lento". Com a mediana
junto, o quadro muda: o legado atende a requisição típica quase tão rápido quanto
o novo, e a diferença mora inteiramente na cauda (p99 de 100–121 ms contra
0,45 ms). Não é lentidão uniforme, é travamento periódico — outro defeito, com
outra correção.

A divergência entre média e mediana é, ela própria, uma medição. Reportar só a
média torna os dois casos indistinguíveis.

### Isolamento de caminho e dependências fixadas

Um serviço tem caminhos de custo diferente (acerto de cache, falha de cache,
encaminhamento). Medir a mistura produz um número que não descreve workload
nenhum e que muda sempre que a mistura muda. Custos saem por subtração, com um
workload por caminho.

E qualquer dependência externa que não seja o objeto da medição precisa ser
fixada: um upstream pela internet contribui dezenas de ms de variância que
afogam o que se está comparando.

Nada disso aparece na skill original porque uma função não tem caminhos
alternativos nem dependências de rede.

---

## Onde a lei de Amdahl deixou de ser útil

A `cpp-parallel-decompose` ensina a estimar o teto de speedup por Amdahl, e a
`cpp-parallel-benchmark` se descreve como a skill que *confirma* essa previsão.
Neste caso a previsão não pôde ser confirmada nem refutada — o modelo parou de
descrever o sistema.

### O que foi medido

Varredura de workers no binário novo, carga cliente fixa e saturante (16 threads),
duas rodadas:

| workers | qps (`hit`) | speedup | fração serial implícita |
|---:|---:|---:|---:|
| 1 | 3562 | 1,00x | — |
| 2 | 5178 | 1,45x | 0,376 |
| 4 | **9436** | **2,65x** | 0,170 |
| 8 | 7752 | 2,18x | 0,382 |
| 16 | 7748 | 2,17x | 0,424 |

| workers | qps (`miss`) | speedup | fração serial implícita |
|---:|---:|---:|---:|
| 1 | 2002 | 1,00x | — |
| 2 | 3390 | 1,69x | 0,181 |
| 4 | 5944 | 2,97x | 0,116 |
| 8 | **7253** | **3,62x** | 0,173 |
| 16 | 6164 | 3,08x | 0,280 |

Fração serial invertida de Amdahl: `s = (N/S - 1) / (N - 1)`.

### Por que o modelo falhou

Amdahl pressupõe que a seção serial é um **custo fixo** — o mesmo com 2 threads
ou com 32. Contenção não é: espera em lock, ping-pong de linha de cache e pressão
no alocador crescem com o número de threads.

Dois sintomas mostram que a premissa caiu, e ambos aparecem nos dados acima:

**O speedup atinge pico e regride.** Amdahl é monótona em `S` — não consegue
produzir escalabilidade negativa. Uma curva que sobe até 4 workers e cai depois
não é fração serial, é contenção.

**A fração serial implícita sobe com N.** Se `s` fosse uma fração fixa real,
sairia aproximadamente constante em todo `N`. Sair 0,170 → 0,382 → 0,424 é a
própria aritmética avisando que a premissa é falsa. Não é ruído de medição: é o
modelo sendo aplicado fora do domínio dele.

### O que foi feito

A estimativa no ponto de pico (0,12–0,17) foi reportada como ordem de grandeza
do que não é paralelizável, e ficou registrado explicitamente que os valores além
do pico **não são interpretáveis**. A conclusão acionável virou "localizar e
reduzir a contenção", não "a fração serial é X".

### O critério extraído

Foi acrescentado ao Step 0 da `cpp-parallel-decompose`: quando o speedup regride
ou a fração serial implícita cresce com N, o modelo parou de valer, e a leitura
deve mudar de "estimar o teto" para "investigar contenção".

Vale notar que isso só é detectável **varrendo** o número de threads. Uma medição
em um único ponto de paralelismo não teria revelado nada — teria produzido um
número plausível e falso.

---

## Por que uma skill nova em vez de estender a existente

As lições se dividiram limpo em duas naturezas:

**Gerais de medição** — intercalar variantes entre rodadas, reportar percentis,
usar invariância ao workload como diagnóstico. Valem para função ou serviço.
Foram para a `cpp-parallel-benchmark`, que já é a skill de medição.

**Específicas de serviço** — forma do gerador, correlação de resposta,
isolamento de caminho, fixação de dependências, leitura de curvas de saturação.
Pressupõem um processo acessível por socket. Viraram skill própria.

A alternativa seria enfiar tudo na `cpp-parallel-benchmark`. Foi descartada por
dois motivos. Primeiro, a skill é sobre sequencial-vs-paralelo no mesmo binário,
e o conteúdo de serviço é volumoso o bastante (duas formas de harness, tabela de
correlação por protocolo, diagnósticos de curva) para diluir esse foco. Segundo,
e mais importante: a decisão de forma do harness precisa aparecer **antes** de
qualquer outra, e como apêndice de outra skill ela seria lida tarde demais — que
é exatamente o que aconteceu neste projeto.

A `service-benchmark` é genérica para request/response (DNS, HTTP, gRPC, brokers)
e não específica de DNS, porque o erro que ela previne é de forma de harness, não
de protocolo. O caso DNS ficou como exemplo trabalhado em
`references/harness-design.md`.

---

## Alterações resultantes

| skill | mudança |
|---|---|
| `service-benchmark` | criada — `SKILL.md` + `references/harness-design.md` |
| `cpp-parallel-benchmark` | nova regra 8 (intercalar variantes); percentis e o critério `mean ≫ p50` na regra 1; ponteiro para `service-benchmark` quando o alvo é um serviço |
| `cpp-parallel-decompose` | seção "quando o modelo deixa de aplicar" no Step 0 |

---

## Estado da avaliação

A `service-benchmark` **não passou pelo loop de avaliação** da `skill-creator`
(casos de teste, execuções com e sem a skill, grading, revisão). Foi decisão
consciente: será avaliada em uso, no próximo benchmark real.

O risco conhecido: a skill foi escrita a partir de **um** caso (DNS sobre UDP) e
se declara genérica. Se a orientação não transferir para HTTP ou gRPC, é aí que
vai aparecer. As partes mais expostas a esse risco são a tabela de identificadores
de correlação (HTTP/1.1 é a exceção, sem correlação explícita) e a suposição de
que o transporte é sem conexão — custo de estabelecer conexão não é tratado.
