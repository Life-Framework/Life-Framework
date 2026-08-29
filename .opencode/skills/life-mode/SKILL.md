---
name: life-mode
description: "The Life Framework rigorous engineering mode. Use when a task needs real rigor on this Arma Reforger mod: bug fixes, new gameplay features, refactors, 'make sure it works', 'do this properly', 'are we sure', performance or runtime issues, in-game verification, or any non-trivial work in addons/LifeFramework. Routes the task to a playbook, applies the engineering principles, and gates completion on in-game proof. Casual one-liners or pure prose tasks do not trigger it."
---

# Life Mode

Life Mode is the Life Framework equivalent of a rigorous engineering discipline. The goal is not more output. It is less, higher-quality output that is proven to work in-game. Every shipped line traces to runtime evidence. A change you cannot verify in the running game is not done.

**Canonical contract.** `AGENTS.md` at the repo root is the portable, tool-agnostic source of the factual rules this skill leans on (golden rules, hard EnforceScript lessons, Enfusion data rules, the verification ladder). Every tool reads it, not just opencode. When this skill and AGENTS.md disagree, AGENTS.md wins. The skill-level playbooks and principles here add the workflow on top.

## Non-negotiables

**Start every multi-step task with a todolist whose first item is to read the Principles index below in full.** The principles ground every trigger in this skill. In your reply, name each principle that shaped a decision and the specific choice it changed. A citation with no decision behind it means you skipped the leaf skill; it must trace to a real choice the leaf rule drove.

Remaining triggers:

- Nontrivial change, architecture decision, or "are we sure" → the **enfusion-how** skill for a read-only walkthrough, or the Investigation playbook.
- About to ask the user a "which approach", "how should I", or "what should this do" question → classify it first. If the answer is a fact you could observe by running something (behavior, timing, output, whether the mod compiles, whether a prefab resolves), it is not the human's to answer. Settle it with the Prototype playbook or by reading base-game behavior and let the result decide. Reserve the question for a genuine product or preference call no experiment can settle.
- Any code or prefab work → name the data shape first (what component, what prefab, what config), and choose its organizing structure per **principle-model-the-domain**.
- Any work that reads or writes an Enfusion API, base-game class, or prefab → research it through the **enfusion-api-research** skill before writing. Never guess an API. The base game is the source of truth.
- Any new script, prefab, or layout → follow the **enfusion-script-authoring**, **enfusion-prefab-authoring**, or **enfusion-config-authoring** skills for conventions.
- Code crossing a function or component boundary → the **enfusion-architect** skill before implementing.
- A small-looking change → the **enfusion-blast-radius** skill to check what else could break.
- Fixing a reported defect → the **Bug fix** playbook. Reproduce from logs first, root-cause, fix with evidence.
- New or changed gameplay behavior → the **Feature** playbook.
- Behavior-preserving restructure (rename, move, split, dedupe) → the **Refactoring** playbook.
- Any prose surface, code comments, PR description, or commit message → the **unslop** skill. Your reply is a prose surface; write it per "Writing the reply".
- Before declaring any task done → the **enfusion-verify** skill. In-game proof is the only proof that counts.
- Fixing a bug with a cheap in-game test path → the **el-tdd** skill (failing test first), or work it into the Bug fix playbook.
- Contested or risky diff → the **interrogate** skill before shipping.
- Long, autonomous, or multi-phase work, or any task the user steps away from to review later → a decision trail via the **show-me-your-work** skill.
- A read-only question about how a subsystem works → the **enfusion-how** skill.

## Principles

Read the leaf skill in full for any principle you apply. Each entry names when it applies. Navigate to the leaf via the skill tool before applying it.

**Core**

- **Laziness Protocol** (`principle-laziness-protocol`). Refactoring, sizing a diff, or tempted to add abstraction. Bias to deletion and the smallest change that solves the problem.
- **Foundational Thinking** (`principle-foundational-thinking`). Before writing logic: what component, what prefab, what config, what data shape. Get the Enfusion data structures right so the script becomes obvious.
- **Subtract Before You Add** (`principle-subtract-before-you-add`). Sequencing an addition or refactor. Remove dead weight first (dead features, redundant validators, stub prefabs), then build on the simpler base.
- **Model the Domain** (`principle-model-the-domain`). Writing stateful logic or code that branches a lot. Encode the domain in a structure (component per concern, manager singleton, state machine, registry, config-driven table) instead of scattered conditionals.
- **Boundary Discipline** (`principle-boundary-discipline`). Wiring replication, RPCs, config parsing, or world interactions. Guards at system boundaries (network, config, resource load), trust internal types, keep business logic pure.
- **Type System Discipline** (`principle-type-system-discipline`). Designing signatures or scripted component classes. Make illegal states unrepresentable, brand semantic primitives, parse external data at boundaries.
- **Make Operations Idempotent** (`principle-make-operations-idempotent`). Designing persistence, save/load, respawn, or loops that run amid disconnects and retries. Converge to the same end state.

**Verification**

- **Prove It Works** (`principle-prove-it-works`). After a task, before declaring done. Verify against the real artifact: run the feature in the DebugWorld, read the actual log line, inspect the actual diff. "It compiles" is a proxy, not proof.
- **Fix Root Causes** (`principle-fix-root-causes`). Debugging. Reproduce from logs first, ask why until you reach the mechanism, resist nil-check guards that silence crashes.
- **Sequence Work into Verifiable Units** (`principle-sequence-verifiable-units`). Multi-step work and how you stage commits. Break work into small units that each end in a check (EL_Test, boot smoke, in-game pass), verify each before the next.

**Delegation**

- **Guard the Context Window** (`principle-guard-the-context-window`). MCP outputs and prefab files are huge. Route bulk reads and sweeps to subagents, keep summaries in the main thread.
- **Never Block on the Human** (`principle-never-block-on-the-human`). Tempted to ask "should I do X" on reversible work. Proceed, present the result, let the human course-correct.

**Meta**

- **Encode Lessons in Structure** (`principle-encode-lessons-in-structure`). You catch yourself writing the same instruction twice. Encode it as a validation script, lint, or test instead of more prose. This repo already does this: `tools/validation/*`, the pre-commit hook, `tools/cli`.

## Autonomy

**Just do it.** Use any MCP tool (`api_search`, `asset_search`, `prefab`, `game_read`, `wiki_read`, Workbench tools). Reversible work proceeds without asking.

**Always pause** for irreversible writes: force-push, deleting a resource GUID, publishing to Workshop, sending messages to players or customers.

**Session overrides:** "don't stop", "run until done", "I'm going to bed", "trust it when I'm back" → keep going, keep a decision trail.

**No is an acceptable answer.** Asked whether to do something, invited to add scope, or shown an approach, reply with real judgment. Decline, push back, or say "this does not earn its place" when true. Candor over sycophancy.

## Subagents

**Use `subagent_type: "life-agent"` for any code-writing delegate.** Life-agent reads this SKILL.md in full before working, so delegation preserves rigor. Substituting `general` skips that read and drifts.

**Use `subagent_type: "el-reviewer"` for read-only adversarial review.** It has edit denied. Usually invoke it through the **interrogate** skill.

Defaults for every subagent spawn: run in background where possible, pass file paths rather than inlined content, keep the summary small. You own every subagent's work: review the diff and write your own summary, never pass through its words. A second opinion is the same prompt against a different model.

## Writing the reply

Write clean as you go. Never generate the bad sentence in the first place.

- Short declarative sentences. One thought per sentence, ended with a period.
- The long-dash character is banned outright. Write the sentence instead.
- A colon as a mid-sentence connector is out. A colon before a list is fine.
- Terse is not an excuse to drop content. Every section a playbook names stays: details, tradeoffs, choices, open decisions.
- Frame impact for the consumer and the maintainer. Name who the work is for (a server owner, a player, the next modder) and what changes for them before any implementation detail.
- Never fabricate a link, citation, log line, or transcript reference. Quote only what you read or produced this session.
- Every playbook ends with a reply written this way, plus the verification evidence you actually ran.

## Comments

Comments follow the same rule as the reply. Write them clean as you go; a "no narrating comments" ban does not catch them. The case this project keeps catching is a script or test that narrates its phases with a `// phase 1: do X` line above a block. Delete it. The assertion string or log line is the only doc you need. Keep a comment only for a non-obvious why the code cannot show. This applies to every file you produce, including delegate diffs and verify scripts. Follow the existing repo style: `//---` separator banners and `//!` doc comments in Enforce Script are fine.

## Playbooks

Your first todolist actions are the matched playbook's steps, copied in verbatim, before any task-specific todos and before you reason about the task. A step you choose not to do stays in the list with a one-line `skip: <reason>`; skipping silently is not allowed.

Match the task to a playbook, open its file, and copy its steps in verbatim.

- **Investigation.** A read-only question: how does X work, why was Y built this way, are we sure. `playbooks/investigation.md`.
- **Bug fix.** A reported defect to reproduce, root-cause, and fix with runtime evidence. `playbooks/bug-fix.md`.
- **Feature.** New or changed gameplay behavior, built from a named data shape. `playbooks/feature.md`.
- **Refactoring.** A behavior-preserving change to structure or shape (rename, extract, inline, dedupe, move). `playbooks/refactoring.md`.
- **Prototype.** A throwaway sketch to make a design or behavioral decision cheaply, or to settle an empirical fork by observing it instead of asking the human. `playbooks/prototype.md`.
- **Runtime forensics.** Diagnose a live symptom (leak, idle spin, glitch, persistence corruption) from logs and instrumentation. The deliverable is a diagnosis, not a fix. `playbooks/runtime-forensics.md`.
- **Multi-phase plan.** Work that spans phases or stacked commits. `playbooks/multi-phase-plan.md`.
- **Session pickup.** Resume or take over a prior agent's in-flight work. `playbooks/session-pickup.md`.

When no bundled playbook fits, use the **enfusion-architect** skill to design a rigorous plan before starting.