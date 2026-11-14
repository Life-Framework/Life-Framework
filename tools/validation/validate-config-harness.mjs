// The static config harness as a validate check (#301): every fixture in tools/test/config-fixtures
// goes through parse and validate and must match its sidecar. A planted broken preset, or a rule that
// changed under a fixture, fails `validate`, so `wt gate`, `ci` and CI all stop on it.
// inputs: tools/config/**, tools/test/config-fixtures/**, tools/economy/**, docs/config/**
import { main } from "../config/harness.mjs";
const code = main(["--stage", "all"]);
console.log(code === 0 ? "PASS  validate-config-harness" : "FAIL  validate-config-harness");
process.exit(code === 0 ? 0 : 1);
