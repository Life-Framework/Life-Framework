// red-proof: PENDING - the suite could not run on 2026-11-07 (Arma Reforger game data project
// missing while Steam restores the install). Planned: comment out the HasDrawnArmoryThisShift
// gate in LF_PoliceRanks.Draw and run --tier fast; "a second draw in one shift refuses" must go red.

// tier: LOGIC
//! Police menu reach, the minimum useful fine, and one armory draw per duty shift.
class EL_Test_PoliceAbuse : EL_Test
{
	override string GetName()
	{
		return "security/police-abuse";
	}

	override void Run(EL_TestContext ctx)
	{
		vector officer = "100 0 100";
		vector close = "104.9 0 100";
		vector justOut = "105.1 0 100";
		vector acrossMap = "2100 0 100";
		ctx.True(EL_PoliceUtils.IsWithinReach(officer, close), "a suspect 4.9 m away is within reach");
		ctx.False(EL_PoliceUtils.IsWithinReach(officer, justOut), "a suspect 5.1 m away is out of reach");
		ctx.False(EL_PoliceUtils.IsWithinReach(officer, acrossMap), "a suspect across the map is out of reach");
		ctx.False(EL_PoliceUtils.IsTargetInReach(null, null), "a suspect with no body is out of reach");

		ctx.False(EL_PoliceUtils.FineClearsWanted(999), "a 999 fine clears no wanted level and is refused");
		ctx.False(EL_PoliceUtils.FineClearsWanted(1), "a 1 fine is refused");
		ctx.True(EL_PoliceUtils.FineClearsWanted(1000), "a 1000 fine clears one wanted level");
		ctx.True(EL_PoliceUtils.FineClearsWanted(EL_PoliceUtils.MAX_FINE), "the max fine clears wanted levels");

		string uid = "security-police-armory";
		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(EL_Faction.POLICE);
		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDuty(true);
		LF_PoliceRanks.OnDutyChanged(account, true);
		ctx.False(account.HasDrawnArmoryThisShift(), "a new shift has its draw available");

		account.SetArmoryDrawnThisShift(true);
		ctx.False(LF_PoliceRanks.Draw(uid, null), "a second draw in one shift refuses");
		ctx.True(LF_PoliceRanks.GetLastError().IndexOf("already drew a kit this shift") != -1, "the refusal names the spent shift draw");

		account.SetOnDuty(false);
		LF_PoliceRanks.OnDutyChanged(account, false);
		ctx.False(account.HasDrawnArmoryThisShift(), "going off duty resets the shift draw");

		account.SetOnDuty(true);
		LF_PoliceRanks.OnDutyChanged(account, true);
		LF_PoliceRanks.Draw(uid, null);
		ctx.True(LF_PoliceRanks.GetLastError().IndexOf("already drew") == -1, "the next shift passes the draw gate");
	}
}
