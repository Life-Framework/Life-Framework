// Stand-in server that exits with code 3 before it writes a log (host-start-nolog-selftest).
setTimeout(() => process.exit(3), 6000);
