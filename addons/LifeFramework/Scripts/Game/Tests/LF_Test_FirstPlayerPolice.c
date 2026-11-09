// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_FirstPlayerPolice` and
// LF_WhitelistManager.ShouldAddFirstPolice returning false:
// [LFTEST]   - playtest host, empty roster: the first player becomes police
// Restored; green.

//! The maintainer's playtest host puts its first player on an empty police roster
//! (LF_MissionHeader.m_bLF_FirstPlayerPolice), so the host can clock in without editing a file.
//! A real server leaves the flag off; a roster that already has an officer is never touched.
// tier: LOGIC
class LF_Test_FirstPlayerPolice : LF_Test
{
	override string GetName()
	{
		return "whitelist/first-player-police";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_WhitelistManager.ShouldAddFirstPolice(true, 0, "00bbbddd-0000-0B38-0000-110F00000F0B"), "playtest host, empty roster: the first player becomes police");
		ctx.False(LF_WhitelistManager.ShouldAddFirstPolice(true, 1, "00bbbddd-0000-0B38-0000-110F00000F0B"), "a roster with an officer is left alone");
		ctx.False(LF_WhitelistManager.ShouldAddFirstPolice(false, 0, "00bbbddd-0000-0B38-0000-110F00000F0B"), "a real server (flag off) never adds anyone");
		ctx.False(LF_WhitelistManager.ShouldAddFirstPolice(true, 0, ""), "no player id, no roster entry");
	}
}
