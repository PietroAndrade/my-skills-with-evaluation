# C++ Parallel Skills — Handoff / estado atual

## Objetivo
Criar skills a partir de "Parallel and Concurrent Programming with C++.md" + examples/. Concluídas e documentadas; em curso: description-optimization (triggering).

## Skills entregues (5 skills + 1 doc)
Todas C++17, referenciam `docs/cpp-conventions.md`, roteiam entre si, cada uma com `docs/<nome>-benchmark.md`.

1. `cpp-concurrency-debug` — diagnostic (sintoma→causa→fix). Bundle: pares begin/end.
2. `cpp-synchronization` — generator (mutex/atomic/shared_mutex/condvar/semaphore/barrier/latch + fallbacks C++17). Bundle: end demos + `references/cpp17-fallbacks.md`.
3. `cpp-async-tasks` — generator (async/future/promise/packaged_task/thread pool). Bundle: demos + `references/thread-pool.md`.
4. `cpp-parallel-decompose` — advisory (Amdahl/partition/work-span/granularity).
5. `cpp-parallel-benchmark` — generator (harness steady_clock, N runs, warm-up, correção, speedup/efficiency). Bundle: `references/benchmark-harness.md`. **Iteração 2 aplicada:** min + anti-DCE + two-callable reset/target.

DEMOVIDA: `cpp-concurrency-reference` (skill) → `docs/choosing-concurrency-primitives.md` (doc compartilhada), p/ cortar colisão de trigger. Review histórico: `docs/choosing-concurrency-primitives-review.md`. Todas as menções ao nome antigo atualizadas (só resta a nota histórica intencional no review).

## Experimentos concluídos (documentados nos benchmark docs)
- Benchmark skill: loop rigoroso with/baseline + cross-tier (Haiku/Sonnet/Opus).
- Sync + async: cross-tier limpo (Haiku/Sonnet), baseline isolado (proibido ler ~/.claude/skills).
- Achados: with-skill uniforme; skill agrega em **bordas** (DCE, fallback C++17, stateless-recursion) + **idiom** (0/6 baselines seguem convenção) + **consistência**. Onde conhecimento é comum (trap std::async) lift ~0. 2 baselines fracos escreveram bugs reais (`binary_semaphore(4)` quebrado; data race no contador de depth).

## EM CURSO: description-optimization (triggering) — PARADO AQUI
- Script: `skill-creator/scripts/run_loop.py` (via `python -m scripts.run_loop`, cwd = base do skill-creator). CLI `claude` v2.1.179 disponível. `--model claude-opus-4-8` funciona.
- Eval-sets prontos (inglês, 8-9 pos + 9-10 near-miss negativos → irmãs): `cpp-skills-workspace/trigger-evals/{cpp-concurrency-debug,cpp-synchronization,cpp-async-tasks,cpp-parallel-decompose,cpp-parallel-benchmark}.json`.
- **Canário rodado** (cpp-parallel-benchmark, max-iter 2, runs 2): resultado em `trigger-evals/results-benchmark/2026-09-24_160423/results.json`.

### PROBLEMAS a resolver antes de continuar
1. **Trigger score baixo**: best train 6/11, test 4/7. Negativos passam (0 falso-positivo), mas **positivos sub-disparam**. Investigar se é real (description precisa melhorar) ou artefato do harness (`claude -p` não carregando skills locais / queries borderline que o modelo resolve sozinho sem skill).
2. **Poluição de registro**: o loop cria skills temporárias `cpp-parallel-benchmark-skill-<hash>` (vistas ~10 na lista). LIMPAR — achar os dirs temporários (provável /tmp ou dir de skills) e remover. Verificar antes de rodar mais loops.
3. Loop é lento/caro (muitos `claude -p`). Rodar 1 por vez, não os 5 em paralelo.

### Próximos passos sugeridos
1. Limpar skills temporárias do canário.
2. Diagnosticar o sub-trigger: rodar 1-2 queries positivas manualmente via `claude -p` com a skill p/ ver se dispara. Se harness-artefato, ajustar método; se real, deixar o loop propor descrições (aumentar max-iterations p/ 5, runs-per-query 3).
3. Rodar loop nas 5 (sequencial), aplicar `best_description` em cada SKILL.md, registrar before/after + scores.
4. Opcional pendente: empacotar `.skill` (`scripts/package_skill.py`).

## Decisões do usuário (preferências)
- C++17 baseline (discutido 11/14/17/20; escolheu 17).
- Review rápido em 4 skills; benchmark detalhado só na benchmark skill.
- Eval JSONs em inglês.
- Docs de resultado em `docs/`.
- CLAUDE.md global: sem comentários em código gerado.
- Modo caveman ativo nas respostas (não afeta código/docs).
