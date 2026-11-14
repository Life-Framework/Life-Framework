# Slice A: Two slices that run at once and own the same file
## Card
- type: capability
- tier: T2
- model: sonnet
- budget: 40 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Bank/**`, `docs/features.md`
- depends: none
- acceptance: test LF_Test_BankA

## Capability
Bank A.

# Slice B: The other one
## Card
- type: capability
- tier: T2
- model: sonnet
- budget: 40 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Bank/LF_BankTeller.c`, `docs/features.md`
- depends: none
- acceptance: test LF_Test_BankB

## Capability
Bank B.

# Slice C: Too big, risky on the wrong model, no acceptance
## Card
- type: system
- tier: T1
- model: sonnet
- budget: 90 min / 120 calls
- owns: `addons/LifeFramework/Scripts/Game/Persistence/**`
- depends: D
- risk: persistence

## Capability
Everything at once.

# Slice D: Cycle partner
## Card
- type: system
- tier: T2
- model: sonnet
- budget: 30 min
- owns: `tools/x.mjs`
- depends: C
- acceptance: test LF_Test_D

## Capability
D.
