// The in-game credits screen's strings and row list must match docs/credits.md (#481).
// inputs: docs/credits.md, addons/**
//
// tools\cli credits builds LF-Credits_* string rows and Feature/Credits/LF_CreditsData.c from the
// credits source. This fails when either differs, so the game cannot drift from the website and README.
// red-proof: tools/validation/credits-selftest.mjs edits a row in the source text and asserts the
// generated outputs change, and a hand-edited generated file fails --check.
import { main } from "../commands/credits.mjs";

process.exit(await main(["--check"]));
