I decided to build my own C++ skills. I started with a few for parallel programming: mutexes, locks, threads. Then I ran a harness across Haiku, Sonnet and Opus to find out whether the skills actually did anything, and what.

Along the way I ran into two kinds of skill. An advisory skill answers a question, like whether to parallelize something or why a speedup is bad. A generator skill emits code. Here is what the harness produced:

![alt text](image.png)

The method: each skill got 3 realistic prompts, graded against assertions I fixed in advance. Every prompt ran twice, once with the skill loaded and once with a baseline that had none, at more than one model tier. The baselines ran in a project root that did not physically contain the skill tree, and afterwards I grepped every output for the skill's own vocabulary to confirm nothing had leaked. I added that step because I needed it. My first round of baselines were told "use no skill" while sitting in the skills directory, read the files anyway, and produced output identical to the skill's own template. Telling an agent to ignore a directory does not isolate it from that directory.

## The finding

> On a strong model, a skill only delivers value if it carries a preference the model has no way to infer. A generator skill has somewhere to put that preference. An advisory skill carries one only when the decision is genuinely local.

What I can measure behind that: _the capability gain is a weak-model phenomenon_, and _the preference gain needs an artifact to ride on_. A generator skill has both. It teaches Haiku, and it carries the conventions at every tier. An advisory skill has only the first, so its value decays with each model release.

## Conclusion

A skill transfers preference far more reliably than it transfers capability, and preference is the part that does not decay as models get stronger.

The evidence splits into two mechanisms.

**Teaching means** filling in what the model does not know. It is real, and it sits entirely in the weak tier. _This mechanism shrinks as models improve. Sonnet and Opus needed almost none of it, so a skill built only on teaching would age out with the next release._

**Transferring preference** _means making the agent work the way this codebase and this engineer work_: which primitive to reach for, which language standard is in force, and so on. This mechanism is flat across tiers. Zero of 9 clean baselines followed the project conventions, Haiku and Sonnet and Opus alike, and 9 of 9 with-skill runs did.

_That is the mechanism behind the intuition that skills give the agent the specialities you already have or prefer_.
