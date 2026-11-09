---
name: principles
description: "Use when deciding how to approach, size, sequence, or finish Life Framework work: the six engineering principles (prove it works, fix root causes, subtract before you add, sequence verifiable units, guard the context window, never block on the human). The life-mode playbooks cite them by name."
---

# Principles

## Prove it works

Done means the changed behavior was observed, not that it compiles. Inspect
the real diff, run `tools\cli validate`, run the cheapest test that exercises
the change, and drive the actual path when behavior changed. Read the specific
log line or assertion; an error count is not evidence. Add restart,
dedicated-server, or late-join proof when the change crosses those
boundaries. Say plainly what stayed unverified.

## Fix root causes

Reproduce from the real log or runtime path, trace the first wrong state to
its mechanism, and fix that. A guard that turns a crash into a silent no-op
hides the bug; add one only where invalid input is genuinely expected (a
config, an RPC payload, a save), and then log an `LF_Debug.Error`. When the
evidence refutes a hypothesis, revert what the hypothesis motivated.

## Subtract before you add

Look at what already owns the behavior before extending it. Delete dead
paths, duplicate state, redundant validators, and half-finished stubs first;
reuse the vanilla class or the existing component; prefer a prefab
inheritance delta over a copy. Count real callers before adding an
abstraction. A diff that removes more than it adds is usually the better one.

## Sequence verifiable units

Order the work so every step leaves something checkable: data shape (prefab,
config), then logic with its `LF_Test`, then UI, then runtime proof. Check each
step before building the next. In git, the failing test lands before the
fix, so the history proves the fix.

## Guard the context window

Read narrowly first. Delegate broad scans and bulky MCP output, pass file
paths rather than pasted files, and keep only decisions and evidence. A
delegate's summary is a claim: inspect the diff or artifact it points at
before trusting it.

## Never block on the human

Settle reversible technical uncertainty yourself, with the tools, the code, or
a small experiment, then present the result. Ask only for product choices,
authorization, or irreversible actions (deleting data, force-pushing,
touching a shared branch).
