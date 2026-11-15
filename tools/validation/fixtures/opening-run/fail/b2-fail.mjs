// Fixture beat script: one check fails, so the run exits 1.
console.log("BEAT B2 wrong-password-refused FAIL the join was accepted");
console.log("BEAT B2 right-password-joins PASS joined");
process.exit(1);
