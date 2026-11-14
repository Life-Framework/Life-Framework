// red-proof: expected "2026-11-14 21:40:08" in the FormatTime assertion: [LFTEST] SUMMARY tier=only passed=0 failed=1 total=1; restored, passed=1
// tier: LOGIC
//! Clip stamp (#1173): the time formatter, the id tail, the clock offset and the toggle, with no world.
class LF_Test_ClipStamp : LF_Test
{
	override string GetName()
	{
		return "moderation/clip-stamp";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ClipStamp.ResetStatics();

		// Real server time as LF_IncidentLog stamps it: seconds since 1970, shown in UTC.
		ctx.True(LF_ClipStamp.FormatTime(0) == "1970-01-01 00:00:00", "unix 0 is the epoch");
		ctx.True(LF_ClipStamp.FormatTime(951782400) == "2000-02-29 00:00:00", "a leap day formats");
		ctx.True(LF_ClipStamp.FormatTime(1794692407) == "2026-11-14 21:40:07", "a 2026 time formats with padding");
		ctx.True(LF_ClipStamp.FormatTime(1794614399) == "2026-11-13 23:59:59", "the last second of a day stays in that day");

		ctx.True(LF_ClipStamp.IdTail("76561198012345abcdef") == "ABCDEF", "the id shows its last six characters in upper case");
		ctx.True(LF_ClipStamp.IdTail("ab12") == "AB12", "a short id is shown whole");
		ctx.True(LF_ClipStamp.IdTail("") == "------", "an empty id shows dashes, never blank");
		ctx.True(LF_ClipStamp.ServerName("  ") == LF_ClipStamp.DEFAULT_SERVER, "a blank server name falls back");
		ctx.True(LF_ClipStamp.ServerName("EVERON-1") == "EVERON-1", "a configured name is kept");

		// The clock: the answer's unix time plus the client ticks since, no per-frame replication.
		LF_ClipStamp.Sync(1794692407, "EVERON-1", "ABCDEF", true, 5000);
		ctx.True(LF_ClipStamp.NowUnix(1794692407, 5000, 5000) == 1794692407, "now equals the answer at the moment it lands");
		ctx.True(LF_ClipStamp.NowUnix(1794692407, 5000, 65000) == 1794692467, "sixty ticked seconds later the stamp is sixty seconds on");
		ctx.True(LF_ClipStamp.IsSynced(), "a sync marks the stamp ready");

		// Off by default, the player toggles, the owner can force it off.
		ctx.False(LF_ClipStamp.IsShown(), "the stamp starts hidden");
		ctx.True(LF_ClipStamp.Toggle(), "the player switches it on");
		ctx.True(LF_ClipStamp.IsShown(), "it shows once on and allowed");
		LF_ClipStamp.Sync(1794692407, "EVERON-1", "ABCDEF", false, 5000);
		ctx.False(LF_ClipStamp.IsShown(), "an owner who turned it off hides it");
		ctx.False(LF_ClipStamp.IsAllowed(), "the allow flag is the owner's");

		LF_ClipStamp.ResetStatics();
		ctx.False(LF_ClipStamp.IsSynced(), "the static reset clears the sync");
		ctx.False(LF_ClipStamp.IsShown(), "the static reset hides the stamp");
	}
}
