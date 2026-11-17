// red-proof: observed 2026-11-17 `--only LF_Test_ArmoryKitCap,...` with the kit-out and cooldown gates in LF_PoliceRanks.DrawAt switched off (tmp unfix run, log test-20261117-224005): FAIL "the refusal names the kit still out", "a lost report clears the serials (expected 0, got 2)". Restored: green.
// tier: LOGIC
//! police/armory-kit-cap (#2416): an officer holds one issued kit. While its serials are out the
//! armory issues nothing, clocking out and in changes nothing, a second use with none of it carried
//! reports it lost, and a kit handed back clears its serials.
class LF_Test_ArmoryKitCap : LF_Test
{
	protected static const string OFFICER = "armory-kit-cap-officer";
	protected static const string SERIAL_A = "police_armory-9001";
	protected static const string SERIAL_B = "police_armory-9002";
	protected static const int T0 = 1800000000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/armory-kit-cap";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_ArmoryKitRow> before = LF_ArmoryKits.ExportAll();
		LF_ArmoryKits.ApplyAll(null);

		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(OFFICER);
		account.SetFaction(LF_Faction.POLICE);
		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDuty(true);
		LF_PoliceRanks.OnDutyChangedAt(account, true, 0);

		array<string> serials = {SERIAL_A, SERIAL_B};
		LF_ArmoryKits.RecordIssue(OFFICER, T0, serials);
		ctx.Equal(2, LF_ArmoryKits.Outstanding(OFFICER), "the issued kit books two serials out");

		// Past the cooldown, the kit still out is what refuses.
		int later = T0 + LF_ArmoryKits.CooldownSeconds() + 100;
		ctx.False(LF_PoliceRanks.DrawAt(OFFICER, null, later), "the armory issues nothing while the kit is out");
		ctx.True(LF_PoliceRanks.GetLastError().Contains("still out"), "the refusal names the kit still out");

		// The old farm: clock out and back in.
		account.SetOnDuty(false);
		LF_PoliceRanks.OnDutyChangedAt(account, false, 1000);
		account.SetOnDuty(true);
		LF_PoliceRanks.OnDutyChangedAt(account, true, 2000);
		ctx.Equal(2, LF_ArmoryKits.Outstanding(OFFICER), "clocking out and in leaves the kit out");
		int afterShift = later + 60;
		ctx.False(LF_PoliceRanks.DrawAt(OFFICER, null, afterShift), "a new shift does not open a second kit");
		ctx.True(LF_PoliceRanks.GetLastError().Contains("still out"), "the new shift's refusal names the kit still out");

		// A second use within the confirm window reports it lost.
		ctx.False(LF_PoliceRanks.DrawAt(OFFICER, null, afterShift + 5), "the confirming use issues nothing either");
		ctx.True(LF_PoliceRanks.GetLastError().Contains("reported lost"), "the second use reports the kit lost");
		ctx.Equal(0, LF_ArmoryKits.Outstanding(OFFICER), "a lost report clears the serials");

		// Cleared, the next use passes the book's gates (it stops later: no officer body).
		LF_PoliceRanks.DrawAt(OFFICER, null, afterShift + 10);
		string passed = LF_PoliceRanks.GetLastError();
		ctx.False(passed.Contains("still out") || passed.Contains("cooldown"), "with nothing out and the cooldown run, the book lets the draw through");

		// Handing kit back clears exactly its serials.
		LF_ArmoryKits.RecordIssue(OFFICER, afterShift + 20, serials);
		array<string> handed = {SERIAL_A, "not-this-officers"};
		ctx.Equal(1, LF_ArmoryKits.TurnIn(OFFICER, handed), "only the officer's own serial is turned in");
		ctx.True(LF_ArmoryKits.IsOutstanding(OFFICER, SERIAL_B), "the item not handed in stays out");
		ctx.Equal(0, LF_ArmoryKits.Outstanding("armory-kit-cap-someone-else"), "another officer has nothing out");

		LF_ArmoryKits.ApplyAll(before);
		account.SetOnDuty(false);
		LF_PoliceRanks.OnDutyChangedAt(account, false, 3000);
	}
}
