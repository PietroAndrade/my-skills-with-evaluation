# O que os experimentos com skills concluíram

Conclusão consolidada dos benchmarks de skills deste diretório: **o que uma skill entrega, o que ela não entrega, e como saber a diferença antes de escrever uma.** Baseado nas medições em `docs/cpp-skills-harness-report.md` (método e números), nos `docs/<skill>-benchmark.md` (por skill) e na run de completude de 2026-09-25 (12 baselines Opus sob isolamento estrutural, 12/12 grep-clean).

Escrito em português como `docs/service-benchmark-decisions.md` e `docs/cpp-skills-trigger-benchmark.md`.

---

## A tese

> Em modelo forte, uma skill só entrega valor se carregar **preferência que o modelo não tem como inferir**. Skill geradora tem onde carregar; advisory carrega só se a decisão for genuinamente local. Reciclar conhecimento de livro não rende em nenhum formato — e pode render **negativo**, porque congela uma resposta que o modelo daria melhor.

Complemento medido em 2026-09-25 (ver "Run de tier fraco"): **lift de capacidade é fenômeno de modelo fraco; lift de preferência precisa de um artefato para pegar carona.** Geradora tem os dois — ensina o Haiku e carrega as convenções em todo tier. Advisory tem só o primeiro, então o valor dela decai a cada release de modelo.

Duas correções que a run de completude forçou sobre a leitura ingênua ("advisory morre, só idiom sobrevive"):

**1. O que sobrevive é *preferência*, não "idiom".** Idiom é a forma mais visível dela, não a única. Houve lift em tier forte para preferências que não são estilo nenhum.

**2. O fracasso das duas advisory não prova que advisory é inviável.** Prova que *aquele conteúdo* era genérico. Elas perderam porque codificavam conhecimento de livro e, em 3 de 6 prompts, estavam erradas ou mais fracas que o default do modelo.

---

## Três categorias, com as evidências

### A. Preferência que não é idiom — **sobrevive em tier forte**

Decisões de casa que o modelo não adivinha porque não são "certas", são *nossas*.

| Preferência | Evidência | Onde |
|---|---|---|
| Intercalar variantes A/B na medição (regra 8) | baseline Opus **0/3** (todos fizeram bloco sequencial-depois-paralelo); com skill 9/9 | `cpp-parallel-benchmark-benchmark.md` |
| Barreira anti-dead-code-elimination | baseline Haiku 0/3, Sonnet 1/3, Opus 1/3; com skill 9/9 | idem |
| Reportar **mínimo** ao lado de média e stddev | baseline majoritariamente só média; com skill sempre | idem |
| Resultado via `future`, nunca somar em `atomic` compartilhado | baseline Haiku somou em `std::atomic`; com skill sempre gather de futures | `cpp-async-tasks-benchmark.md` |
| Fallback C++17 em vez de API C++20 quando o toolchain é C++17 | baseline Haiku usou `binary_semaphore(4)` — **funcionalmente quebrado, satura em 1** | `cpp-synchronization-benchmark.md` |
| Artefato pronto empacotado (`references/thread-pool.md`, `references/cpp17-fallbacks.md`) | o baseline reescreve um pool equivalente a cada vez; a skill entrega *o mesmo* pool sempre | ambos |
| Roteamento entre skills irmãs ("isto é caso de producer-consumer, não de async") | negativos 49/50 na medição de triggering | `cpp-skills-trigger-benchmark.md` |

Note o padrão: quase tudo aqui é **regra de borda que o modelo esquece sob carga**, ou **escolha entre duas opções ambas defensáveis**. É exatamente onde não existe resposta "óbvia" para o modelo convergir sozinho.

### B. Idiom propriamente dito — **sobrevive em tier forte, mas com uma condição**

| Convenção | Evidência |
|---|---|
| `#endif /* GUARD */` (não `// GUARD`, não `#pragma once`) | baseline Opus **0/3 mesmo com as convenções no prompt** |
| `} // namespace <lib>` no fecho | Opus 2/3 com convenções entregues; eval-2 largou os dois comentários |
| Sufixo `.h`, não `.hpp` | baseline Opus 0/3 quando as convenções **não** estavam no prompt |
| Layout `<lib>/include/<lib>/` + `<lib>/src/` | idem 0/3 |
| `m_` em membros, `namespace` = nome da lib, zero comentários explicativos | Haiku e Sonnet escorregaram para `namespace lib` **mesmo informados**; Opus cumpriu 3/3 quando informado |

**A condição, e ela é uma correção ao report original:** o headline "0 de 9 baselines seguiram as convenções" vale para runs em que as convenções **não estavam no prompt**. Quando estão, o baseline Opus cumpre a substância. Ou seja, a frase mais forte do §6 — "ser *informado* da preferência não transferiu" — vale para Haiku e Sonnet e **falha para Opus**.

O argumento durável não é "modelo forte não consegue seguir convenção informada". É: **ninguém repete as convenções em todo prompt.** A skill é o que as torna sempre presentes, de graça, sem o engenheiro lembrar.

### C. Conhecimento de livro — **não sobrevive em formato nenhum**

| Conteúdo | Resultado |
|---|---|
| Armadilha do `std::async` com future descartada | **6/6** configurações, todos os tiers, com e sem skill, diagnosticaram certo |
| Usar pool em vez de 200 threads crus | todas as configurações escolheram pool |
| `shared_mutex` para dado read-heavy | todos os baselines acertaram sozinhos |
| `steady_clock`, descartar warm-up, média de N runs, reset in-place | Sonnet 3/3 e Opus 3/3 sem ajuda; só Haiku precisou |
| Deadlock AB-BA → `scoped_lock` | baseline Opus acertou e listou **11** não-fixes com o motivo de cada um (a skill listava 3) |
| Lock abandonado em early-return → RAII | baseline Opus reproduziu com `timeout 5` → exit 124, confirmou via `/proc/*/wchan` |
| Mecânica de Amdahl | baseline Opus calculou e **descartou** o número de um ponto só, propôs Karp–Flatt no lugar |

Regra prática de autoria que sai daqui: **se um engenheiro competente sem acesso ao nosso repo escreveria a mesma frase, corte a frase.** Ela é peso morto que dilui as partes que funcionam.

### D. Pior que nada — onde a skill **congelou** uma resposta ruim

Três defeitos que só apareceram porque o baseline limpo foi rodado:

| Skill | O que a skill diz | O que o baseline limpo disse |
|---|---|---|
| `cpp-parallel-decompose` eval-0 | 40% de I/O ⇒ Amdahl `s=0.40` ⇒ **2,1×, teto 2,5×** | "serial *hoje* ≠ inerentemente serial": os 50.000 arquivos são independentes, flash serve leituras concorrentes. Teto duplo `T ≥ s + max(0.6/C, 0.4/D)`; serial real 1–3%; **5–7× em NVMe**. E o aviso que a skill não tem: em HDD, 8 leitores viram seek thrash, `D` cai abaixo de 1 e a versão paralela **perde** |
| `cpp-parallel-decompose` eval-2 | inverte Amdahl de um ponto ⇒ `P ≈ 0.71` | mesmo cálculo, depois rejeitado: um ponto não separa seção serial de recurso saturado de overhead que cresce com N. **Karp–Flatt em sweep N=1…16, lido por tendência** (constante ⇒ código serial, subindo ⇒ contenção, caindo ⇒ granularidade) |
| `cpp-concurrency-debug` eval-1 | "ordenação, não acesso" ⇒ `barrier`/`latch`/redesenho | mecanismo nomeado (`+` de ponto flutuante é comutativo mas **não associativo**, ordem de aquisição reagrupa a soma), discriminador dado (**jitter de bits baixos = ordem de redução; valor rasgado = race real**), repro com coluna instável ao lado de coluna estável sob TSan silencioso, e o alerta: barrier/token de vez **compra determinismo serializando** — parciais por thread em faixas estáticas dão determinismo *e* paralelismo |

Esses três seguem o mesmo padrão que rendeu a barreira DCE e o reporte de mínimo na iteração 1: **o baseline limpo se paga não quando perde, mas quando ganha em algo específico.**

---

## Por que geradora carrega preferência e advisory (quase) não

Skill geradora emite um artefato. Todo artefato tem cem decisões de forma embutidas — caminho do arquivo, nome do guard, prefixo de membro, ordem dos includes, o que vai no header vs no .cpp — e o exemplo trabalhado da skill ancora todas de uma vez. É por isso que **mostrar** funciona melhor que **mandar**.

Skill advisory devolve prosa. A prosa não tem forma para ancorar, então o único conteúdo possível é raciocínio — e raciocínio genérico é precisamente o que um modelo forte já faz bem ou melhor.

Sai disso o teste de viabilidade para advisory: **a decisão é local?**

| Advisory genérica (não rende) | Advisory local (renderia) |
|---|---|
| "Use Amdahl para decidir se vale paralelizar" | "Nosso target de deploy é VM de 2 vCPU: qualquer plano que só ganha acima de 8 cores é no-go" |
| "Decomponha por domínio quando os itens são independentes" | "Nosso storage é NFS: meça `D` com um passe read-and-discard antes de discutir threads" |
| "Considere `std::execution::par`" | "GCC 9 + C++17 no build: parallel STL não existe aqui, e `tbb` não está no conanfile" |
| "Deadlock costuma ser ordem de lock invertida" | "3 dos últimos 5 incidentes de concorrência foram lock abandonado em validação com early-return — checar isso primeiro" |
| "Meça percentis, não média" | "SLO do serviço é p99 < 40 ms com 200 conexões; abaixo disso ninguém aprova o PR" |
| "Prefira o primitivo mais simples" | "Nesta base, `recursive_mutex` é proibido: duas vezes foi usado para esconder reentrância acidental" |

À direita, nada é derivável do código nem de conhecimento geral. É aí que advisory sobrevive.

---

## Limites do que foi medido

Registrar isto importa tanto quanto os números, porque a tese acima é forte e a amostra não é.

| Limite | Consequência |
|---|---|
| 3 prompts por skill, n=1 por prompt | detecta piso, não dá intervalo de confiança |
| ~~Lift zero das duas advisory medido só em Opus~~ | **FECHADO 2026-09-25** — 12 runs, os dois braços em Haiku. Previsão do §6 **confirmada**: o lift aparece no tier fraco. Falta Sonnet. Ver seção "Run de tier fraco" abaixo |
| Baselines Haiku/Sonnet nunca verificados por grep | plausivelmente limpos, não confirmados |
| Eval sets escritos por quem escreveu as descriptions | viés para queries que a description já cobre |
| Triggering medido em um modelo, um dia | 89/94 descreve Opus 5 em 2026-09-25 |
| `service-benchmark` nunca avaliado | a alegação de genérico (HTTP/gRPC/brokers a partir de um caso DNS/UDP) é não testada |

~~O experimento não distingue "advisory não funciona" de "essas duas advisory tinham conteúdo genérico".~~ **Resolvido pela run de tier fraco abaixo:** era a segunda. Advisory funciona — em tier fraco.

---

## Run de tier fraco — resposta à questão aberta (2026-09-25)

Rodado: **os dois braços em Haiku**, 3 prompts por skill, 12 runs. Os dois braços porque comparar baseline Haiku contra os runs com-skill Opus confundiria tier com skill — mesmo tipo de erro de instrumento que este harness já documentou duas vezes. Os 6 baselines sob isolamento estrutural, todos grep-clean.

| Skill | baseline Haiku | com-skill Haiku | baseline Opus | com-skill Opus |
|---|:---:|:---:|:---:|:---:|
| `cpp-concurrency-debug` | **2/3** | 3/3 | 3/3 | 3/3 |
| `cpp-parallel-decompose` | **0/3 sólidos** | 3/3 | 3/3 | 3/3 |

**Advisory tem lift — só no tier de baixo.** Mesma forma dos geradores. A diferença é que advisory tem *só isso*: não emite artefato, logo não carrega idiom, que é o que sobrevive em tier forte.

### A vitória de correção, e é o caso de manual

`cpp-concurrency-debug` eval-1. O baseline Haiku abriu com *"race condition na leitura final — você quase certamente lê o resultado sem segurar o lock"* como causa nº 1. Isso é **data race**, contradiz a evidência dada (TSan silencioso), e o fix de manchete é no-op. Pior: listou `result += value;` sob **"COMMUTATIVE (safe, order doesn't matter)"** — para ponto flutuante está invertido, o problema é não ser **associativo**. A própria regra dele abençoa o bug.

O run com-skill abriu com a classificação certa e ofereceu computar local e combinar depois. É a tabela data-race vs race-condition da skill fazendo exatamente o que foi escrita para fazer, no tier que precisa.

### Onde o lift de advisory é menor do que parece

Em `cpp-parallel-decompose`, nenhum baseline Haiku errou o **plano**. Errou os números:
- intensidade aritmética como **14,7 ops/byte** onde o loop não-blocado dá **~0,125 FLOP/B** — duas ordens de grandeza, e ainda assim concluiu "bandwidth bound". Veredito que sobrevive à própria aritmética quebrada é hábito pior que veredito errado.
- duas regras inventadas e apresentadas como fato ("speedup ∝ √threads ⇒ memory bound"; "miss ratio > 25% ⇒ saturação de banda"), e utilização por core como discriminador — que não separa o próprio suspeito nº 1 dele, já que código limitado por banda mostra 100% em todo core.

Logo o lift é **confiabilidade aritmética e ter um gate quantitativo**, não corrigir decisão errada.

### O achado mais importante: defeito de skill se propaga para o próprio output

`cpp-parallel-decompose` eval-0, agora medido em quatro configurações:

| Config | Resposta |
|---|---|
| baseline Haiku | 2,1×, seco |
| com-skill Haiku | 2,1×, depois corrigido a 3–5× via "fator de pipeline" |
| com-skill Opus | 2,1×, teto 2,5× |
| **baseline Opus** | **rejeita o modelo** — leituras paralelizam, fração serial é 1–3%, 5–7× em NVMe, e muitos leitores em HDD podem deixar *mais lento* |

A única configuração que acertou é a **sem skill e com modelo forte**. Ensinar o modelo correto melhoraria os dois braços nos dois tiers. **Não é lift faltando; é conteúdo que limita o output.** Instância mais clara do corolário: regra que duplica senso comum não é neutra — ela **prende** a resposta ao senso comum.

### Decisão de demoção

| Skill | Decisão | Por quê |
|---|---|---|
| `cpp-concurrency-debug` | **manter skill** | no tier que precisa, impede que um modelo fraco prescreva no-op para bug real *e* enuncie regra que abençoa o bug |
| `cpp-parallel-decompose` | **manter, mas corrigir antes** | lift fraco é real mas menor (números, não decisões), e o conteúdo do eval-0 comprovadamente limita o próprio output nos dois tiers |

Nenhuma demovida. A demoção de `cpp-concurrency-reference` segue correta pelos motivos dela — colisão de trigger e roteamento puro — que são outros motivos.

### O que ainda falta

Baseline **Sonnet** para as duas advisory: o lift é conhecido em Haiku (real) e Opus (zero); o meio está interpolado, não medido. ### Defeitos aplicados (2026-09-25)

Os 3 defeitos da seção D estão corrigidos nos `SKILL.md`, mais um herdado pela skill de benchmark. Detalhe por mudança em cada benchmark doc; `cpp-skills-harness-report.md` §9 resume.

| Skill | Iteração | Mudança |
|---|---|---|
| `cpp-concurrency-debug` | 2 | ordem dos fixes **invertida** — parciais por thread em faixas estáticas primeiro, barrier/latch rebaixado com o custo dito (determinismo serializando). Mais: mecanismo de associatividade FP, discriminador jitter-de-bits-baixos vs valor-rasgado, "TSan silencioso é evidência, não ponto cego", lista de não-fixes, lista de outros combines sensíveis a ordem, teste de detecção de 20 runs |
| `cpp-parallel-decompose` | 2 | "serial *hoje* ≠ inerentemente serial" + teto duplo `T ≥ s + max(f_cpu/C, f_io/D)` com tabela de `D` e aviso de seek thrash em HDD. Karp–Flatt promovido de rodapé a diagnóstico prescrito, com tabela de tendência e sweep. Output pede faixa atrelada ao recurso limitante, mais checklist |
| `cpp-parallel-benchmark` | 3 | regra 10 nova (controlar page cache; declarar cold vs warm; rotear serviço para `service-benchmark`) e "eficiência baixa" agora exige sweep Karp–Flatt em vez de um número |

Em todos os casos o que **não** foi mexido diz tanto quanto o que foi: seções de deadlock/livelock/starvation/data-race/abandoned-lock e Steps 1–4 do decompose ficaram intactas porque todo baseline, em todo tier, acertou sozinho. Mexer nelas violaria o próprio corolário 1 da lista abaixo.

**As correções estão NÃO VERIFICADAS.** Foram escritas a partir dos gaps medidos, não validadas por run novo. Vale o corolário 2 na sua forma irmã: nunca suponha que uma edição teve o efeito pretendido. Toda mudança deixou a skill **mais longa**, e conteúdo que duplica senso comum dilui as partes que funcionam — o que justifica cada adição é que um baseline limpo bateu a skill exatamente naquele ponto, o que é argumento, não medição.

Falta também baseline **Sonnet** para as duas advisory: lift conhecido em Haiku (real) e Opus (zero), o meio interpolado.

---

## Checklist para escrever a próxima skill

1. **Escreva o baseline primeiro.** Rode a tarefa sem skill, isolado de verdade (ver item 6), com grep de verificação. O que o baseline já acerta não entra na skill.
2. **Corte toda frase que um estranho ao repo escreveria.** Conhecimento de livro é peso morto e pode congelar uma resposta pior que a do modelo.
3. **Prefira mostrar a mandar.** Exemplo trabalhado ancora forma melhor que instrução — medido: convenções no prompt não bastaram em Haiku/Sonnet.
4. **Se for advisory, aponte a decisão local.** Não achou nenhuma? É doc, não skill.
5. **Empacote artefatos**, não descrições de artefatos (o pool C++17, a tabela de fallbacks).
6. **Valide o instrumento antes de confiar em qualquer número** — três braços deste harness produziram números confiantes e errados antes de serem checados, todos silenciosamente. Três regras saíram disso, cada uma de um erro cometido:

   - **Verifique o que entra no input do controle, não só o que sai no output.** Grep do output prova que o *texto* da skill não vazou. Não diz nada sobre a *description* dela, que o harness mostra a todo agente e que normalmente enuncia a tese inteira da skill. Quando a assertion sob teste é justamente a coisa que a description nomeia, baseline via subagente não consegue medir. Foi o que aconteceu com "say what, not how": a linha `in business language that hides implementation detail` estava no system prompt do controle.
   - **Leia os transcripts, não os relatórios dos agentes.** Agente descreve o próprio trabalho de forma favorável e omite o que não percebeu. O defeito de isolamento só aparecia como metadado de `cwd` dentro do JSONL — nenhum relatório mencionaria. Grep por argumento de tool tocando a árvore (`"file_path":"…/.claude/…"`), contado por braço.
   - **Tire meta-instrução do prompt do baseline.** "Complete a tarefa sem nenhum guia de estilo" e "não leia ~/.claude" informam que existe um guia de estilo e que ele é relevante ali. É dica, e cai justamente no braço que deveria não ter nenhuma. Dê a tarefa e nada mais; isolamento é trabalho do harness, não do prompt.

   Receita corrigida e o que ela ainda não cobre: `cpp-skills-harness-report.md` §10; forma operacional em `AGENTS.md` passo 4.

### Documentos fonte

- `cpp-skills-harness-report.md` — método, matriz por tier, §7 run de completude
- `cpp-parallel-benchmark-benchmark.md` — contaminação de baseline: causa raiz e correção
- `cpp-synchronization-benchmark.md` — experimento "convenções entregues no prompt"
- `cpp-async-tasks-benchmark.md` — o resultado de lift zero
- `cpp-concurrency-debug-benchmark.md`, `cpp-parallel-decompose-benchmark.md` — primeiros baselines, lift zero, defeitos achados
- `cpp-skills-trigger-benchmark.md` — triggering 89/94 e os defeitos do harness de stub
- `cpp-parallel-skills-decisions.md` — decisões de design e conclusões superadas
- `cpp-skills-benchmark.md` — comparação inicial interface/concrete/lib (15/15 vs 5/15)
