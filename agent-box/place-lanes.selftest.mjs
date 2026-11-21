// Runs the shop-lane checks' selftest alone (agent-box-selftest runs it; `toolscli audit selftest` runs it inside place-audit's).
// place-audit.mjs loads first on purpose: it and place-lanes.mjs import each other, and only that order initialises both.
import "./place-audit.mjs";
import { selftest } from "./place-lanes.mjs";

selftest();
if (!process.exitCode) console.log("place-lanes selftest: OK");
