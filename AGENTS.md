# How to Create and Validate a New Skill

This document describes the workflow used to create, benchmark, and document skills in this directory.

---

## 1. Explore the codebase

Before writing any skill, read the actual code. Patterns in skill docs must come from real usage, not assumptions.

Spawn an Explore agent over the relevant module(s):

```
In <project_path>, read all files in <module>. I need to understand:
- File naming conventions
- Class/struct usage patterns
- Member naming (prefixes, casing)
- Constructor/destructor patterns
- Include guard format
- Namespace conventions
- Any CMakeLists.txt specifics
```

---

## 2. Write the skill

Create `~/.claude/skills/<skill-name>/SKILL.md` following this structure:

```markdown
---
name: <skill-name>
description: <what it does + when to trigger — be specific about trigger phrases>
---

## Goal
One sentence. What does the skill produce and which conventions doc to read first.

## Workflow
Numbered steps asking the user for info before generating anything.

## Rules
Specific conventions the model must follow. Explain *why* each rule exists.

## Examples
Real code showing the expected output format.
```

**Guidelines:**
- Reference shared convention docs (`docs/cpp-conventions.md`, etc.) instead of repeating rules inline — keeps the skill lean
- Descriptions must be "pushy" — include concrete trigger phrases so the skill fires reliably
- Rules explain *why*, not just *what* — models follow reasoning better than mandates
- Examples are the most load-bearing part: they anchor format, style, and file paths

---

## 3. Create canonical experiment files (`experiments/<skill-name>/skills-results/`)

Generate the canonical example by following the skill yourself (or via a with-skill agent). This becomes the reference implementation for future comparisons.

Structure:
```
experiments/
└── <skill-name>/
    ├── skills-results/     ← generated following the skill
    └── <tier>-baseline-results/  ← baseline, copied in from the scratch dir (step 4)
```

The experiment files should demonstrate all skill features: naming, file paths, include guards, serialization patterns, etc.

---

## 4. Run the benchmark

Two arms per prompt: one with the skill loaded, one baseline with no skill. Use
2-3 test cases covering a simple case, a case with optional features, and an edge
case. Run both arms on the same model — comparing a weak-tier baseline against a
strong-tier skill run measures the tier, not the skill.

**Write the assertions before running anything.** Fix them in a file and do not
edit them after seeing output. Phrase every repair assertion as *"X is repaired
and still present"* — a baseline can pass "X is fixed" by deleting X.

Baselines write to a scratch directory, never into `experiments/` directly —
writing into the skill tree puts the baseline back inside it. Copy the finished
outputs in afterwards, once grading is done.

**With-skill arm** — an ordinary subagent is fine:

```
Execute this task using the skill at ~/.claude/skills/<skill-name>/SKILL.md.
Read the skill file first, then follow its instructions. Do NOT ask clarifying questions.
Task: <concrete task>
Save outputs to: <scratch dir>/<eval>/withskill/
```

**Baseline arm — do NOT use a subagent.** A subagent inherits this session's
working directory and receives the full list of installed skills, descriptions
included, in its system prompt. Run a separate headless process instead:

```
cd <a directory outside ~/.claude>
claude -p --model <same tier> --disable-slash-commands "<task prompt>"
```

`--disable-slash-commands` removes the skills listing; the neutral cwd removes the
skill tree from reach. Residual leaks to state rather than fix: the global
`CLAUDE.md` still loads, and a SessionStart hook may inject one skill. Validate
by asking an isolated agent to list its skills before you trust a single number.

### The three rules that cost the most to learn

1. **Verify what reaches the control's input, not only what leaves its output.**
   Grepping baseline output for the skill's vocabulary proves the skill's *text*
   did not leak. It says nothing about the skill's *description*, which the
   harness shows to every agent and which usually states the skill's whole
   thesis. When the assertion under test is the thing the description names, a
   subagent baseline cannot measure it.

2. **Read the transcripts, not the agents' reports.** Agents describe their own
   work favourably and omit what they never noticed. The isolation defect above
   was visible only as `cwd` metadata inside the transcript JSONL — no report
   would ever have mentioned it. Grep transcripts for tool arguments touching the
   skill tree (`"file_path":"…/.claude/…"`), and count them per arm.

3. **Strip meta-instructions from the baseline prompt.** *"Complete the task
   without any style guide"* and *"do not read ~/.claude"* both tell the model
   that a style guide exists and is relevant here. That is a hint, and it lands
   in the arm that is supposed to have none. Give the baseline the task and
   nothing else; isolation is the harness's job, not the prompt's.

Full account of how these were found: `docs/cpp-skills-harness-report.md` section
10.

---

## 5. Grade the results

Compare each generated file against these criteria:

| Category | What to check |
|----------|--------------|
| **File structure** | Correct lib path (`<lib>/include/<lib>/`), correct file suffix |
| **Include guards** | Format matches project convention |
| **Naming** | `m_` prefix for members, `camelCase` for methods, `snake_case` for files |
| **Structural conventions** | `final`, `override`, blank lines between methods, brace style |
| **Feature-specific** | JSON uses `j.value()` not `j.at()`, `std::size_t` not bare `size_t`, etc. |

---

## 6. Document the benchmark

Write `docs/<skill-name>-benchmark.md` with:

1. Summary table (skill score vs baseline score)
2. Per-eval comparison table with ✅/❌ per criterion
3. "Key observations" section — what baseline got right, where skill wins

---

## File layout summary

```
~/.claude/skills/
├── README.md                        ← skill index + benchmark scores
├── AGENTS.md                        ← this file
├── <skill-name>/
│   └── SKILL.md
├── docs/
│   ├── cpp-conventions.md           ← shared C++ rules
│   ├── cpp-cmake-conventions.md     ← shared CMake rules
│   └── <skill-name>-benchmark.md   ← benchmark results
└── experiments/
    └── <skill-name>/
        ├── skills-results/               ← canonical example (with skill)
        ├── <tier>-baseline-results/      ← baseline, run in isolation, copied in
        └── ASSERTIONS.md + GRADING.md    ← pre-registered, then scored
```

---

## What makes a good skill

- **Signal over noise**: only encode rules that a capable model consistently gets wrong without guidance. Don't repeat things the model already does correctly.
- **Benchmark-driven**: if the skill doesn't improve the score meaningfully, the rule isn't load-bearing — remove it.
- **Shared docs**: rules shared across multiple skills belong in `docs/`, not duplicated in each SKILL.md.
- **Trigger accuracy**: a skill that doesn't fire is useless. Test trigger phrases and tune the description if it under-fires.
