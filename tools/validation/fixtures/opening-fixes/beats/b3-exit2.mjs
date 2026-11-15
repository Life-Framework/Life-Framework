// Fixture beat for opening-fixes-selftest: what B10 does when the site is not built: BLOCKED on stdout, no BEAT line, exit 2.
console.log("BLOCKED site not built: website/dist/servers/index.html is missing");
console.log("SUMMARY b3-exit2 blocked");
process.exit(2);
