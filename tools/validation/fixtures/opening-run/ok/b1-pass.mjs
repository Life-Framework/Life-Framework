// Fixture beat script: every check passes. With --host it checks that the runner passed it on, as the flag and as
// LF_HOST_DIR, and fails when it did not. The runner gives each beat its own folder under the --host base: <base>/<beat> (#1691).
const i = process.argv.indexOf("--host");
const host = i === -1 ? "none" : process.argv[i + 1];
console.log("some chatter that is not a BEAT line");
console.log("BEAT B1 config-apply PASS wrote config.json");
if (host !== "none" && /[\\/]B1$/.test(host) && process.env.LF_HOST_DIR === host) console.log(`BEAT B1 host-arg PASS host=${host}`);
else console.log(`BEAT B1 host-arg FAIL host=${host} LF_HOST_DIR=${process.env.LF_HOST_DIR}`);
