// Stand-in for tools/cli.mjs in opening-b2-selftest: answers `host start --flagship --wait` and `host watch`.
//   FAKE_START  up (default) | nopid (exit 0, no `host: up` line) | fail (exit 1) | hang (never answers: the beat's --timeout kills it)
//   FAKE_WATCH  watch (default: say it adopted the host, then stay up 8 s) | die (exit 1 at once)
const args = process.argv.slice(2);
const env = process.env;

if (args[0] === "host" && args[1] === "start") {
  const mode = env.FAKE_START || "up";
  if (mode === "hang") setTimeout(() => process.exit(0), 60000);
  else if (mode === "fail") {
    console.log("FAIL  the server exited during startup");
    process.exit(1);
  } else {
    if (mode === "up") console.log("host: up (pid 4242); log fixture/console.log");
    else console.log("host: starting");
    console.log("join: direct connect to <this PC's address>:2001 (UDP)");
    process.exit(0);
  }
}

if (args[0] === "host" && args[1] === "start") {
  // answered above
} else if (args[0] === "host" && args[1] === "watch") {
  if ((env.FAKE_WATCH || "watch") === "die") process.exit(1);
  console.log("[LFDebug:HostWatch] watching the running host (pid 4242)");
  setTimeout(() => process.exit(0), 8000);
} else {
  console.log(`fake-cli: unexpected call ${args.join(" ")}`);
  process.exit(3);
}
