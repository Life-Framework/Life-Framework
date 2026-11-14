// red-proof: changed `playersOnline == 1` to `playersOnline >= 1` in LF_WorldReset.MayAutoReset and ran
// `tools\cli test --no-build --only LF_Test_WorldResetGate`; "a respawn with company on the server leaves the world alone" failed.
// Restored it and it passed.

// tier: LOGIC
//! The world reset gate: a respawn resets the world only in a playtest mission, with the dev gate
//! open and nobody else online. The menu and globe request needs the mission and the dev gate.
class LF_Test_WorldResetGate : LF_Test
{
	override string GetName()
	{
		return "worldreset/gate";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_WorldReset.MayAutoReset(true, true, 1), "a lone tester's respawn in a playtest mission resets the world");
		ctx.False(LF_WorldReset.MayAutoReset(true, true, 2), "a respawn with company on the server leaves the world alone");
		ctx.False(LF_WorldReset.MayAutoReset(false, true, 1), "a real server (no playtest mission) never resets on a respawn");
		ctx.False(LF_WorldReset.MayAutoReset(true, false, 1), "a closed dev gate never resets on a respawn");
		ctx.False(LF_WorldReset.MayAutoReset(true, true, 0), "no one online is not a lone tester");

		ctx.True(LF_WorldReset.MayRequestReset(true, true), "the menu and globe reset needs the playtest mission and the dev gate");
		ctx.False(LF_WorldReset.MayRequestReset(false, true), "the menu and globe reset is refused on a real server");
		ctx.False(LF_WorldReset.MayRequestReset(true, false), "the menu and globe reset is refused with the dev gate closed");
	}
}
