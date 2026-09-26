#!/usr/bin/env python3
import json
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

NEUTRAL_CWD = "/tmp/claude-1000/-home-pandrade--claude-skills/8b3f256c-8742-4e20-b10a-2fd5b81e911b/scratchpad/probe-cwd"
MAX_TOOL_EVENTS_BEFORE_GIVEUP = 4
TIMEOUT = 180


def run_query(query: str, target_skill: str) -> dict:
    env = {k: v for k, v in os.environ.items() if k != "CLAUDECODE"}
    cmd = [
        "claude", "-p", query,
        "--output-format", "stream-json",
        "--verbose",
        "--disallowed-tools", "Write", "Edit", "NotebookEdit",
    ]
    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
        cwd=NEUTRAL_CWD, env=env, text=True,
    )

    skills_invoked = []
    tool_names = []
    tool_events = 0
    start = time.time()
    try:
        for line in proc.stdout:
            if time.time() - start > TIMEOUT:
                break
            line = line.strip()
            if not line:
                continue
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue

            if event.get("type") == "assistant":
                for content in event.get("message", {}).get("content", []):
                    if content.get("type") != "tool_use":
                        continue
                    tool_events += 1
                    name = content.get("name", "")
                    tool_names.append(name)
                    inp = content.get("input", {})
                    if name == "Skill":
                        skills_invoked.append(inp.get("skill", ""))
                    elif name == "Read":
                        path = str(inp.get("file_path", ""))
                        if "/.claude/skills/" in path and path.endswith("SKILL.md"):
                            skills_invoked.append(
                                path.split("/.claude/skills/")[1].split("/")[0] + " (via Read)"
                            )
                if skills_invoked or tool_events >= MAX_TOOL_EVENTS_BEFORE_GIVEUP:
                    break
            elif event.get("type") == "result":
                break
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()

    triggered = any(s.split(" ")[0] == target_skill for s in skills_invoked)
    return {
        "query": query,
        "triggered_target": triggered,
        "skills_invoked": skills_invoked,
        "tool_names": tool_names,
        "tool_events": tool_events,
    }


def main():
    eval_dir = Path("/home/pandrade/.claude/skills/cpp-skills-workspace/trigger-evals")
    targets = sys.argv[1:]
    Path(NEUTRAL_CWD).mkdir(parents=True, exist_ok=True)

    jobs = []
    for skill in targets:
        eval_set = json.loads((eval_dir / f"{skill}.json").read_text())
        for item in eval_set:
            jobs.append((skill, item))

    print(f"running {len(jobs)} queries", file=sys.stderr)
    out = []
    with ThreadPoolExecutor(max_workers=6) as pool:
        futures = {
            pool.submit(run_query, item["query"], skill): (skill, item)
            for skill, item in jobs
        }
        done = 0
        for fut in as_completed(futures):
            skill, item = futures[fut]
            res = fut.result()
            res["skill"] = skill
            res["should_trigger"] = item["should_trigger"]
            res["pass"] = res["triggered_target"] == item["should_trigger"]
            out.append(res)
            done += 1
            status = "PASS" if res["pass"] else "FAIL"
            print(
                f"[{done}/{len(jobs)}] {status} {skill} exp={item['should_trigger']} "
                f"got={res['skills_invoked']} :: {item['query'][:60]}",
                file=sys.stderr,
            )

    print(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
