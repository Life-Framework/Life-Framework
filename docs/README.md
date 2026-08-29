# Life Framework Docs

Project documentation for the Life Framework Arma Reforger mod.

## Project Plan

- [Roadmap](roadmap.md) — the phased development plan (scraped from
  https://life-framework.org/roadmap). Phase 0 is in progress; Phases 1–4 are
  unstarted and open for contributors.
- [Vision](vision.md) — philosophy & vision (scraped from
  https://life-framework.org/philosophy). MIT-licensed, extensible, 1989-era
  base framework.

## Engineering

- [Features](features.md) — ground truth of every implemented feature, its
  behavior contract, and known-fragile / broken paths. Written from a
  read-only code sweep. The contract test cases must prove.
- [Test plan](test-plan.md) — the test format (scalable, low-LOC, Overthrow-
  inspired) and the per-feature test cases that prove the contracts in
  features.md. Pin tests for known bugs go red first.
- [Foundation design](foundation-design.md) — design for the four foundational
  primitives (Economy, Trade, Processing, Real Estate) mined from the MIT
  Overthrow reference implementations, with per-feature sources, LOC targets
  and build order.

## Agent Guidance

When picking up roadmap work, prefer Phase 0 items still in progress first
(Discord, Brand Identity, Roadmap v1, Founding Team Recruitment, GitHub Repo,
Development Environment, Documentation Foundation), then Phase 1 core systems.
All roadmap items are open for contributors — coordinate on Discord before
starting work.

See AGENTS.md for the engineering contract (GUID rules, EnforceScript lessons,
verification ladder) that applies to any code written for this repo.