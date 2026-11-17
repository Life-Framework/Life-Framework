// red-proof: Observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_RejoinSafeSpot`: RED PROOF OK, with the safety verdict skipped the stale, terrain, water, geometry, jail, restricted-zone and locked-house cases fail, and with the connection mark skipped `a respawn on the same connection is not` fails. Green on the real code.
// tier: LOGIC
//! #1778 (second half of #640): a returning character rejoins at the saved spot only when it is safe.
//! Each unsafe case falls back to the spawn point (an empty verdict means safe): no spot, a stale
//! spot, under the terrain, in water, inside geometry, inside the jail, in a restricted zone, in a
//! locked house. The order in GetCreationPosition (jail, hospital wake, saved spot, spawn point) is
//! checked through LF_RejoinSpot.Applies.
class LF_Test_RejoinSafeSpot : LF_Test
{
	protected static const int NOW = 1700100000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "spawn/rejoin-spot";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_LastPositionRow Row(int ageSeconds)
	{
		return LF_LastPositionRow.Create("rejoin-spot-char", "100 20.2 300", 90, NOW - ageSeconds);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_RejoinProbe Ground()
	{
		LF_RejoinProbe probe = new LF_RejoinProbe();
		probe.m_fSurfaceY = 20;
		probe.m_fWaterY = 0;
		return probe;
	}

	//------------------------------------------------------------------------------------------------
	protected string Verdict(LF_LastPositionRow row, LF_RejoinProbe probe)
	{
		if (RedProof())
			return "";

		return LF_RejoinSpot.Verdict(row, NOW, 12, probe);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("", Verdict(Row(600), Ground()), "a fresh spot on open ground is used");
		ctx.EqualStr("", Verdict(Row(12 * 3600), Ground()), "a spot exactly at the age limit is still used");

		ctx.True(!Verdict(null, Ground()).IsEmpty(), "no saved spot falls back");
		ctx.True(!Verdict(Row(12 * 3600 + 1), Ground()).IsEmpty(), "a spot older than the configured hours falls back");

		LF_RejoinProbe under = Ground();
		under.m_fSurfaceY = 25;
		ctx.True(!Verdict(Row(60), under).IsEmpty(), "a spot under the terrain falls back");

		LF_RejoinProbe water = Ground();
		water.m_fSurfaceY = 15;
		water.m_fWaterY = 20;
		ctx.True(!Verdict(Row(60), water).IsEmpty(), "a spot in water falls back");

		LF_RejoinProbe pier = Ground();
		pier.m_fSurfaceY = 15;
		pier.m_fWaterY = 18;
		ctx.EqualStr("", Verdict(Row(60), pier), "a spot well above the water (a pier) is used");

		LF_RejoinProbe blocked = Ground();
		blocked.m_bBlocked = true;
		ctx.True(!Verdict(Row(60), blocked).IsEmpty(), "a spot inside geometry falls back");

		LF_RejoinProbe jail = Ground();
		jail.m_bInJail = true;
		ctx.True(!Verdict(Row(60), jail).IsEmpty(), "a spot inside the jail falls back");

		LF_RejoinProbe restricted = Ground();
		restricted.m_bRestricted = true;
		ctx.True(!Verdict(Row(60), restricted).IsEmpty(), "a spot in a restricted zone falls back");

		LF_RejoinProbe house = Ground();
		house.m_bLockedHouse = true;
		ctx.True(!Verdict(Row(60), house).IsEmpty(), "a spot inside a locked house falls back");

		// Order: the saved spot comes after the jail return and the hospital wake, and only when the
		// setting is on.
		ctx.True(LF_RejoinSpot.Applies(true, false, false), "setting on, free, no wake: the saved spot applies");
		ctx.False(LF_RejoinSpot.Applies(false, false, false), "setting off: vanilla spawn");
		ctx.False(LF_RejoinSpot.Applies(true, true, false), "serving a sentence: the jail return comes first");
		ctx.False(LF_RejoinSpot.Applies(true, false, true), "a pending hospital wake comes first");

		// Only the first spawn of a connection is a rejoin; a respawn after a death is not.
		LF_RejoinSpot.ForgetConnection("rejoin-spot-uid");
		ctx.True(LF_RejoinSpot.NoteSpawn("rejoin-spot-uid"), "the first spawn of a connection is a rejoin");
		if (!RedProof())
			ctx.False(LF_RejoinSpot.NoteSpawn("rejoin-spot-uid"), "a respawn on the same connection is not");
		else
			ctx.True(LF_RejoinSpot.NoteSpawn("rejoin-spot-uid"), "red proof: second spawn reads as a rejoin");
		LF_RejoinSpot.ForgetConnection("rejoin-spot-uid");
		ctx.True(LF_RejoinSpot.NoteSpawn("rejoin-spot-uid"), "after the player leaves, the next spawn is a rejoin again");
		LF_RejoinSpot.ForgetConnection("rejoin-spot-uid");
	}
}
