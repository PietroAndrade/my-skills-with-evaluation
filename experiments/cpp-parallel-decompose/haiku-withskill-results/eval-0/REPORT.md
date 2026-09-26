# Haiku WITH SKILL — decompose eval-0, 40% serial I/O + 60% CPU
Transcribed by the orchestrator.

## Step 0 Amdahl gate (skill framework)
1/(0.40+0.60/8) = 2.1x at 8 cores. THEN corrected with pipelining: "if I/O time <
parsing time / 7, speedup improves to 3-5x"; pipeline factor 2-3x realistic -> 3.6-4.7x.
Verdict GO. Final range given: pessimistic 2.1x, realistic 3-4x, best case 4.5-5x.

## Decomposition
Domain over files (8 buckets) + functional pipeline (1 reader -> queue -> 7 parsers),
hybrid recommended: pool of 8, batches of ~1500-2000 files.
Per-thread accumulators, merge at end.

## Granularity — formula given
batch = total_files / (cores x 4) = 50000/32 ~ 1562, for 4x overdecomposition.

## Task graph
W = R + P; span ~ max(R/1, P/7); ideal parallelism 2.1x without pipelining, up to 7x
with I/O fully hidden.

## Routing
cpp-async-tasks for the pool + producer-consumer; cpp-synchronization for the merge.

## Verification
Baseline serial, then parallel; measure pure I/O and pure parse separately to validate
P ~ 0.60; scaling sweep 1/2/4/8 against the Amdahl curve with measured P; red flag if
speedup peaks then drops (contention); check reader is I/O-bound at 30-50% CPU.

## STILL CARRIES THE SKILL DEFECT
Treats 0.40 as Amdahl's s and recovers only via a pipeline "factor". Never reaches the
Opus-baseline insight that the READS THEMSELVES parallelize on flash (storage
concurrency D), so the real serial fraction is 1-3% and NVMe allows 5-7x.
Never warns that on a spinning disk many readers cause seek thrash and can LOSE.

## Grade
Better than the Haiku baseline on this prompt (which stopped at 2.1x flat), but both
inherit the same structural error about I/O.
