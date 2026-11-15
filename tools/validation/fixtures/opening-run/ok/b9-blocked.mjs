// Fixture beat script: one check passes, one is blocked by slices that are not on main. Exits 0.
console.log("BEAT B9 refuses-while-running PASS refused");
console.log("BEAT B9 wipe-needs-notice BLOCKED host wipe notice check is not on main (#1413)");
console.log("BEAT B9 wipe-backup BLOCKED `host wipe` is not on main (#1274, #1413)");
