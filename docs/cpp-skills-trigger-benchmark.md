# C++ skills — trigger benchmark (a dimensão de *disparo*)

Mede se cada skill **dispara nas queries certas e fica quieta nas erradas**. É uma dimensão diferente dos `docs/<skill>-benchmark.md`, que medem a qualidade do output *dado que* a skill disparou. Uma skill com output perfeito que não dispara é inútil; uma que dispara no lugar da irmã é pior que inútil.

Medido em **2026-09-25** nas 5 skills do conjunto paralelo/concorrente. Eval-sets: `cpp-skills-workspace/trigger-evals/*.json`. Runner: `cpp-skills-workspace/trigger-evals/trigger_probe.py`. Dados brutos: `cpp-skills-workspace/trigger-evals/results-2026-09-25/{pass1,pass2}.json`.

---

## Resultado

94 queries (44 positivas + 50 negativas near-miss), modelo **Opus 5** (o modelo configurado do usuário, sem `--model`). Pass = taxa de disparo ≥ 0,5.

| skill | positivos | negativos | total |
|---|---|---|---|
| `cpp-concurrency-debug` | 9/9 | 10/10 | **19/19** |
| `cpp-async-tasks` | 8/9 | 10/10 | **18/19** |
| `cpp-parallel-decompose` | 8/9 | 10/10 | **18/19** |
| `cpp-synchronization` | 8/9 | 9/10 | **17/19** |
| `cpp-parallel-benchmark` | 7/8 | 10/10 | **17/18** |
| **agregado** | **40/44** | **49/50** | **89/94** |

**Veredito: as descrições escritas à mão estão boas.** Nenhuma skill precisa de tuning de `description`. Confirma a decisão de não rodar o loop de otimização — mas por um motivo oposto ao que estava registrado (ver abaixo).

### Discriminação entre irmãs funciona por mérito

Nos negativos, o comportamento observado não foi "nada disparou" — foi **a skill irmã correta disparou**: `cpp-synchronization` na query de `shared_mutex`, `cpp-async-tasks` na de download + gather, `cpp-parallel-decompose` nas de Amdahl/granularidade. O risco de colisão que motivou demover `cpp-concurrency-reference` para doc (ver `cpp-parallel-skills-decisions.md`) não se materializou: 49/50.

### As 4 falhas em positivos

Três delas com **0/3 disparos** e a mesma forma — pedido direto de "escreva o código", em que o modelo vai para `Bash`/resposta direta sem consultar a skill:

| skill | query | disparos |
|---|---|---|
| `cpp-synchronization` | "add a reader-writer lock to this cache class so readers don't block each other" | 0/3 |
| `cpp-async-tasks` | "fan out one task per file to process them concurrently and then collect all the results" | 0/3 |
| `cpp-parallel-benchmark` | "set up a proper microbenchmark for this function so the compiler doesn't optimize the work away" | 0/3 |
| `cpp-parallel-decompose` | "will adding more threads actually help here or am i going to hit diminishing returns" | 1/3 |

Isto é a versão **verdadeira e muito menor** da hipótese antiga de sub-disparo: 4 em 44 positivos, não 5 em 11. A leitura plausível continua sendo que o modelo só consulta skill para tarefa que não resolve direto — mas agora como exceção localizada, não como característica dominante. Nenhuma das quatro é ambígua quanto à skill correta, então são candidatas legítimas a ajuste de `description` se algum dia incomodarem em uso real.

### O único falso-positivo é uma trade-off aceita de propósito

"should i use a mutex or an atomic for this shared counter? what's the tradeoff" dispara `cpp-synchronization` em **3/3**. O eval-set rotula `should_trigger: false`.

Mas `cpp-parallel-skills-decisions.md` (seção sobre a demoção de `cpp-concurrency-reference`) registra explicitamente o contrário: esse tipo de query "will be handled inline or fall to `cpp-synchronization`. Judged worth it to cut collision". Ou seja, cair em `cpp-synchronization` **é** o comportamento desenhado.

Portanto é divergência entre o rótulo do eval-set e a decisão de projeto, não defeito da skill. **Pendente decidir:** realinhar o rótulo para `true` (e o score de `cpp-synchronization` vira 18/19) ou manter como falha conhecida deliberada.

### Flakiness é real e mede algo

Das 9 queries que falharam no pass 1, **4 viraram passe** com 3 runs. Disparo é um Bernoulli, não determinístico. Consequências práticas:

- n=1 sozinho teria reportado 5 falhas inexistentes.
- Queries de fronteira precisam de n≥3 para serem classificadas.
- O desenho adaptativo (n=1 em tudo, +2 runs só nas falhas) gasta amostra onde a variância decide algo, em vez de reconfirmar passes. Recomendado para repetições futuras.

---

## Método

```
python3 cpp-skills-workspace/trigger-evals/trigger_probe.py \
    cpp-concurrency-debug cpp-synchronization cpp-async-tasks \
    cpp-parallel-decompose cpp-parallel-benchmark > pass1.json 2> pass1.log
```

O runner roda cada query via `claude -p --output-format stream-json --verbose` e conta disparo quando a skill **real** é invocada — pelo nome no tool `Skill`, ou por um `Read` de `~/.claude/skills/<nome>/SKILL.md`. Detalhes que importam:

- **cwd neutro**, fora de `~/.claude/skills` (mesma lição de isolamento estrutural do `cpp-parallel-benchmark-benchmark.md`).
- **Tolera tools preliminares:** acumula até 4 tool-events antes de concluir "não disparou". `Bash` antes de `Skill` é padrão comum e não é ausência de disparo.
- **Registra qual skill disparou**, não só pass/fail — é o que revela se a irmã correta assumiu a query.
- **Mata o processo assim que a decisão é observável** e proíbe `Write`/`Edit`. Cada run custa o primeiro turno da sessão, não uma implementação C++ completa. As 112 execuções (94 + 18) saíram baratas por isso.

---

## Por que a primeira medição (canário de 2026-09-24) era inválida

O canário reportou `cpp-parallel-benchmark` em **best train 6/11, test 4/7**, com negativos em 100%. Medida corretamente: **17/18**. A diferença é o instrumento, não a descrição.

O `skill-creator/scripts/run_eval.py` tem três defeitos que se somam **quando a skill sob teste já está instalada** — que é exatamente este caso:

1. **Não testa a skill real.** `run_eval.py:51-68` escreve um *slash command* descartável em `.claude/commands/<skill>-skill-<hash>.md` cujo corpo inteiro é `This skill handles: <description>`. É um stub, não a skill.
2. **A detecção exige o nome com hash.** `run_eval.py:147` e `164-167` só contam disparo se `<skill>-skill-<hash>` aparecer no input do tool. Quando o Claude invoca a skill **real** (instalada em `~/.claude/skills/`), o nome não casa e o run é contado como *não disparou*. Na prática o teste media "o modelo prefere meu comando duplicado e vazio à skill real?" — cuja resposta é naturalmente não.
3. **Aborta no primeiro tool que não seja `Skill`/`Read`.** `run_eval.py:140-141` retorna `False` de imediato. Como `Bash` antes de `Skill` é comum, isso é uma segunda fonte independente de falso-negativo.

**Evidência direta**, antes de qualquer sweep: a query *"write a reader-writer lock class with shared_mutex for a config cache"* invocou `Skill(cpp-synchronization)` e em seguida leu `cpp-conventions.md`. O harness antigo registraria isso como miss. Nas re-execuções do pass 2, `tools=['Bash','Skill']` apareceu repetidamente, confirmando o defeito 3 de forma independente.

**Os 100% em negativos também eram artefato**, e isso é o mais enganoso: o comando falso nunca é invocado para nada, então todo negativo passa por construção. Zero falso-positivo não era evidência de boa discriminação — era ausência de medição. Os 49/50 atuais passam por mérito.

Nota de escopo: o `run_eval.py` não é inutilizável em geral. Os defeitos 1 e 2 só produzem falso-negativo sistemático quando existe uma cópia real e instalada da skill competindo com o stub. O defeito 3 vale sempre.

### A conclusão que caiu

`cpp-parallel-skills-decisions.md` registrava que o sub-disparo era "largely inherent, not a description defect" porque "Claude only consults a skill for tasks it can't handle directly". A decisão de parar o loop assumiu isso sem verificar o instrumento — e `cpp-skills-workspace/HANDOFF.md:28` deixava a hipótese de artefato de harness explicitamente em aberto, nunca diagnosticada.

A decisão final (não rodar o loop de otimização) permanece correta. A justificativa, não: as descrições não estavam sub-disparando a ponto de precisar de tuning — 89/94 é bom.

---

## Limitações desta medição

- **n=1 nos passes.** Só as falhas do pass 1 receberam 3 runs. Uma query que passou pode ser flaky e não teria sido detectada. Dado que 4 de 9 falhas eram flaky, é provável que exista flakiness não observada entre os passes.
- **Um modelo, uma máquina, uma data.** Disparo depende do modelo; 89/94 descreve Opus 5 em 2026-09-25. Tiers mais fracos podem discriminar pior. Não extrapolar.
- **`should_trigger` é rótulo humano.** O caso "mutex or atomic" mostra que o próprio rótulo pode divergir da intenção de projeto. Um score alto contra rótulos errados não significa nada.
- **Os eval-sets foram escritos pelos mesmos autores das descrições**, o que enviesa na direção de queries que as descrições já cobrem.

## Erros de processo durante a execução (para não repetir)

- `until ! pgrep -f "trigger_probe.py"` usado como loop de espera **casa com o próprio shell do loop**, porque o padrão está na linha de comando dele. O loop girou ~29 min depois do sweep já ter terminado. O `pkill -f "until ! pgrep"` seguinte repetiu o mesmo auto-casamento e matou o comando que o executava, antes do Python de agregação rodar. Para esperar um processo por nome, usar um padrão que não apareça no próprio comando (ex.: `pgrep -f "trigger_[p]robe"`) ou guardar o PID.
- Nenhum dos dois afetou o sweep nem os dados; custaram só tempo de parede.
