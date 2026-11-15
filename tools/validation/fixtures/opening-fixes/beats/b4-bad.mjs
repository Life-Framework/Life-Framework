// Fixture beat for opening-fixes-selftest: dies with exit 3 and no BEAT line: still a FAIL (only exit 2 is BLOCKED).
console.error("boom");
process.exit(3);
