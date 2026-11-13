// red-proof: 2026-11-10 `tools/cli test --no-build --only LF_Test_BackRoomRules` with the on-duty check disabled failed `off duty: refused`. Restored; green.
// tier: LOGIC
//! Back room police rules (#600): only an on-duty officer in reach may search a real back room,
//! a raided room stays shut for the configured shut time, and a back room seizure is kept in the evidence locker
//! without a crime report.
class LF_Test_BackRoomRules : LF_Test
{
	override string GetName()
	{
		return "shop/backroom-police";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(LF_EBackRoomRefusal.NONE, LF_BackRoomRules.Check(true, true, 2.0, 0, 1000), "on-duty officer in reach: allowed");
		ctx.Equal(LF_EBackRoomRefusal.NOT_ALLOWED, LF_BackRoomRules.Check(false, true, 2.0, 0, 1000), "off duty: refused");
		ctx.Equal(LF_EBackRoomRefusal.NOT_A_BACK_ROOM, LF_BackRoomRules.Check(true, false, 2.0, 0, 1000), "an ordinary shop is not a target");
		ctx.Equal(LF_EBackRoomRefusal.TOO_FAR, LF_BackRoomRules.Check(true, true, 40.0, 0, 1000), "out of reach: refused");
		ctx.Equal(LF_EBackRoomRefusal.ALREADY_SHUT, LF_BackRoomRules.Check(true, true, 2.0, 5000, 1000), "a shut room is not searched twice");

		int until = LF_BackRoomRules.ClosedUntil(1000);
		ctx.True(LF_BackRoomRules.IsClosed(until, 1001), "shut right after the raid");
		ctx.False(LF_BackRoomRules.IsClosed(until, until), "open again once the shutter time has passed");
		ctx.False(LF_BackRoomRules.IsClosed(0, 0), "a room never raided is open at world time 0");

		LF_EvidenceLocker.ResetStatics();
		LF_EvidenceLockerRow row = LF_EvidenceLocker.LogSeizure("{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et", "Back room", "uid-1");
		if (Replication.IsServer())
		{
			ctx.NotNull(row, "the seizure is logged");
			ctx.Equal(1, LF_EvidenceLocker.Count(), "one locker row");

			array<ref LF_EvidenceLockerRow> exported = LF_EvidenceLocker.ExportAll();
			LF_EvidenceLocker.ApplyAll(exported);
			ctx.Equal(1, LF_EvidenceLocker.Count(), "a reportless back room row survives a load");
		}
		LF_EvidenceLocker.ResetStatics();
	}
}
