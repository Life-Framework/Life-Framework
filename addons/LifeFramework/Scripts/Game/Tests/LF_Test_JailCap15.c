// red-proof: #4207. Written before the code: the first run failed to compile (no LF_JailCellComponent.ClampMaxSeconds, no LF_JailWork.SetSentenceCap). RedProof() skips the clamp and the cap, so the 1800 s cap stays, the 15 minute sentence checks fail, and the chores credit unscaled. RED-OK 2026-11-23 `test --no-build --red LF_Test_JailCap15`: 3 checks failed (cap 1800 not 900, sentence 1800 not 900, chore 1790 not 870), then green.

//! #4207: a sentence tops out at 15 online minutes in the holding cell, and the cell's work stations take time off in
//! proportion (a chore authored for the old 5 minute cap credits 3x at 15 minutes, never more than 5% of the cap).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_JailCap15 : LF_Test
{
	protected static const ResourceName CELL = "{AA32D528D592E54C}Prefabs/Police/EL_JailCell.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jail/cap-15";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string officerUid = "jail-cap15-officer";
		string prisonerUid = "jail-cap15-prisoner";

		// A configured cap above 15 minutes clamps to 15 (the cell logs the clamp).
		int cap = 1800;
		if (!RedProof())
			cap = LF_JailCellComponent.ClampMaxSeconds(1800);

		ctx.Equal(900, cap, "a cap above 15 minutes clamps to 900 seconds");
		ctx.Equal(900, LF_JailCellComponent.ClampMaxSeconds(900), "15 minutes itself is kept");
		ctx.Equal(300, LF_JailCellComponent.ClampMaxSeconds(300), "a lower cap is kept");

		// The core cell prefab ships the 15 minute cap.
		ctx.Equal(900, PrefabMaxSeconds(), "the cell prefab's sentence cap is 900 seconds");

		// A long sentence tops out at the cap.
		LF_Jail.Configure(600, cap, 1, 5);
		LF_Jail.SetCellOrigin("48 0 64");
		LF_JailWork.ResetStatics();
		if (!RedProof())
			LF_JailWork.SetSentenceCap(cap);

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		LF_PlayerAccount prisoner = LF_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(3);

		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, null), "the arrest starts a sentence");
		ctx.EqualFloat(900, LF_Jail.GetRemaining(prisonerUid), 0.01, "a sentence of 1800 seconds is held at 15 minutes");

		// Chores scale with the cap: the 1989 stations (8, 10 and 8 s) credit 24, 30 and 24 s.
		ctx.EqualFloat(24, LF_JailWorkRules.ScaledSecondsOff(8, 900), 0.01, "an 8 s chore credits 24 s at the 15 minute cap");
		ctx.EqualFloat(30, LF_JailWorkRules.ScaledSecondsOff(10, 900), 0.01, "a 10 s chore credits 30 s at the 15 minute cap");
		ctx.EqualFloat(8, LF_JailWorkRules.ScaledSecondsOff(8, 300), 0.01, "at the old 5 minute cap a chore credits what it says");
		ctx.EqualFloat(8, LF_JailWorkRules.ScaledSecondsOff(8, 0), 0.01, "no cap set leaves the chore alone");
		ctx.EqualFloat(45, LF_JailWorkRules.ScaledSecondsOff(100, 900), 0.01, "one chore never takes off more than 5% of the cap");

		// A full shift (every station once) takes off at most the round share of the cap.
		float shift = LF_JailWorkRules.ScaledSecondsOff(8, 900) + LF_JailWorkRules.ScaledSecondsOff(10, 900) + LF_JailWorkRules.ScaledSecondsOff(8, 900);
		ctx.True(shift <= 900 * LF_JailWorkRules.SHIFT_SHARE_MAX, string.Format("a full shift takes off at most %1 of the cap (%2 s of 900)", LF_JailWorkRules.SHIFT_SHARE_MAX, shift));

		// The real path: Complete credits the scaled amount.
		ctx.Equal(LF_EJailWorkCheck.OK, LF_JailWork.Complete(prisonerUid, "laundry_tub", 10, 30, 100), "a chore is accepted");
		ctx.EqualFloat(870, LF_Jail.GetRemaining(prisonerUid), 0.01, "a 10 s chore takes 30 s off a 15 minute sentence");

		LF_JailWork.ResetStatics();
		LF_Jail.SyncPlacedCell();
	}

	//------------------------------------------------------------------------------------------------
	protected int PrefabMaxSeconds()
	{
		Resource res = Resource.Load(CELL);
		if (!res || !res.IsValid())
			return 0;

		IEntitySource source = res.GetResource().ToEntitySource();
		if (!source)
			return 0;

		for (int i = 0, n = source.GetComponentCount(); i < n; i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (component.GetClassName() != "LF_JailCellComponent")
				continue;

			int maxSeconds;
			component.Get("m_iMaxSeconds", maxSeconds);
			return maxSeconds;
		}

		return 0;
	}
}
