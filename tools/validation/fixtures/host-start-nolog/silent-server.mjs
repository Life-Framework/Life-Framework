// Stand-in server that starts and never writes a log (host-start-nolog-selftest).
// It ends itself after 60 s so a failed selftest leaves nothing behind.
setTimeout(() => process.exit(0), 60000);
