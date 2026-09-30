# Allure reporting from Behave

Read this when setting up Allure for a Behave suite, when the report tree does
not look the way the tags suggest it should, or when choosing between Allure
versions.

## Installation — pick the right Allure

Two independent pieces:

**Python adapter**: `pip install allure-behave` (put it in `requirements.txt`).
This writes the raw result JSON into `allure-results/`.

**CLI**: `npm install -g allure` — the Allure 3 CLI, Node-based, no Java.

Traps worth knowing:

- The npm package `allure-commandline` is the **Allure 2** wrapper and needs a
  JVM. Not the same thing.
- The Arch Linux AUR `allure-commandline` package is also Allure 2 / Java.
- Allure 3 has no `--clean` flag, takes no positional results path (it finds
  `allure-results/` itself), and uses `--config` / `-c` for the config file.

```bash
allure generate -c .allurerc.mjs -o allure-report   # static report
allure open allure-report                            # serve it
allure serve allure-results                          # generate + serve, temporary
```

## `.allurerc.mjs`

Allure 3's config is a plain JS default export. Do **not** write
`import { defineConfig } from "allure"` — that fails under a global npm install.

```javascript
export default {
  name: "E2E Firewall Tests",
  output: "./allure-report",
  plugins: {
    awesome: {
      options: {
        groupBy: ["parentSuite", "suite", "subSuite"],
      },
    },
  },
};
```

`allure generate` must be given `-c .allurerc.mjs`. Without it the config is
ignored and you get the default Suites view only — which looks like the tags
failed, when actually the config never loaded.

## Hierarchy limitations

These are the ones that cost time to rediscover:

**The auto-generated `feature` label collides.** `allure-behave` adds a `feature`
label from the Gherkin `Feature:` name. Adding `@allure.label.feature:X` yields
two `feature` labels; the tree renders both as sibling nodes and the test count
doubles. This is why the convention uses `parentSuite` / `suite` / `subSuite`
instead of `epic` / `feature` / `story`.

**Repeated `subSuite` tags do not nest.** Two `@allure.label.subSuite:` tags on
one scenario produce sibling nodes, not a two-level path. Deeper categorisation
is expressed by hyphenating into a single value:
`@allure.label.subSuite:MultiCountry-MultiWan-AppUsage`.

**Custom labels do not extend the tree.** Adding a fourth `groupBy` entry —
`{ label: "layer" }`, `{ label: "behavior" }` — stores the label in the result
JSON but the tree widget ignores it. Three levels is the ceiling. Custom labels
are useful for category grouping, not hierarchy.

## Debugging report data

When the tree does not match the tags, look at what actually got written rather
than re-reading the feature file.

**Labels on a single test result:**

```bash
cat allure-report/data/test-results/$(ls allure-report/data/test-results/ | head -1) | python3 -c "
import json, sys
data = json.load(sys.stdin)
print(f'name: {data[\"name\"]}')
for l in data['labels']:
    print(f'  {l[\"name\"]}: {l[\"value\"]}')
"
```

If the labels are missing here, the problem is the tags or the adapter — not the
config. If they are present but the tree is flat, the problem is `groupBy` / the
missing `-c` flag.

**The generated tree:**

```bash
cat allure-report/widgets/tree.json | python3 -c "
import json, sys
data = json.load(sys.stdin)
def print_tree(node, indent=0):
    for gid in node.get('groups', []):
        g = data['groupsById'][gid]
        print(' ' * indent + f'{g[\"name\"]} ({g[\"statistic\"][\"total\"]})')
        print_tree(g, indent + 2)
print_tree(data['root'])
"
```

The awesome plugin may write its own tree at
`allure-report/widgets/default/tree.json`; check both if one looks stale.

A duplicated node name with split test counts at the top level is the signature
of the `feature` label collision described above.

## Running with both formatters

Keep the terminal readable while collecting results:

```bash
behave -f pretty -o - -f allure -o allure-results features/
```

`-o -` sends the pretty formatter to stdout; the second `-o` directs the allure
formatter at the results directory. In a Makefile target, follow the behave call
with `|| true` so a failing suite still gets a report generated.
