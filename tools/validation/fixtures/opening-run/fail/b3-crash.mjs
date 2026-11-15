// Fixture beat script: dies without printing a BEAT line; the runner reports a FAIL for it.
console.error("boom");
process.exit(3);
