// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only EL_Test_JailSlip`:
// made LF_JailSlip.Grant skip the carried slip (`carried = null`); got
// `- a refused arrest and a respawn hand no second slip (expected 1, got 2)`. Restored; green.
// A new prefab needs a build first: the copied main resourceDatabase.rdb lacks its GUID
// (`Wrong GUID ... SentenceSlip.et`, `the prisoner carries one sentence slip (expected 1, got 0)`).

// tier: WORLD
class EL_Test_JailSlip : EL_Test
{
	override string GetName()
	{
		return "jail/slip";
	}

	override void Run(EL_TestContext ctx)
	{
		string officerUid = "jail-slip-officer";
		string prisonerUid = "jail-slip-prisoner";

		LF_Jail.SyncPlacedCell();
		ctx.True(LF_Jail.HasCell(), "the DebugWorld cell is placed");
		if (ctx.FailureCount() > 0)
			return;

		LF_Jail.Configure(60, 300, 1, 8);
		IEntity body = EL_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		EL_PlayerAccount officer = EL_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(EL_Faction.POLICE);
		officer.SetOnDuty(true);
		EL_PlayerAccount prisoner = EL_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(2);

		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, body), "the arrest sentences the stand-in");

		array<LF_JailSlipComponent> slips = {};
		LF_JailSlip.FindCarried(body, slips);
		ctx.Equal(1, slips.Count(), "the prisoner carries one sentence slip");
		if (slips.Count() != 1)
		{
			EL_TestCharacterFixture.Delete(body);
			return;
		}

		LF_JailSlipComponent slip = slips.Get(0);
		ctx.EqualStr(prisonerUid, slip.GetHolderUid(), "the slip names its holder");
		ctx.True(!slip.GetPrisonerName().IsEmpty(), "the slip prints the prisoner name");
		ctx.Equal(2, slip.GetWantedLevel(), "the slip prints the offence (wanted level)");
		ctx.Equal(LF_JailSlipRules.MinutesLeft(LF_Jail.GetRemaining(prisonerUid)), slip.GetMinutesLeft(), "the slip minutes match LF_Jail");
		ctx.Equal(2, slip.GetMinutesLeft(), "120 seconds print as 2 minutes");
		ctx.False(slip.IsReleased(), "a fresh slip is not stamped");

		ctx.False(LF_Jail.Arrest(officerUid, prisonerUid, body), "a second arrest while serving is refused");
		LF_Jail.OnPlayerSpawned(prisonerUid, body);
		LF_JailSlip.FindCarried(body, slips);
		ctx.Equal(1, slips.Count(), "a refused arrest and a respawn hand no second slip");

		array<string> online = {prisonerUid};
		LF_Jail.TickOnline(61, online);
		ctx.Equal(1, slip.GetMinutesLeft(), "the slip refreshes to the online minutes left");

		int doorsBefore = LF_Jail.GetDoorOpenings();
		LF_Jail.TickOnline(59, online);
		ctx.False(LF_Jail.IsServing(prisonerUid), "the sentence is served");
		ctx.True(slip.IsReleased(), "release stamps the slip");
		ctx.Equal(0, slip.GetMinutesLeft(), "the stamped slip shows no time left");
		ctx.Equal(1, LF_Jail.GetReleaseCount(prisonerUid), "the prisoner is released once");
		ctx.Equal(doorsBefore + 1, LF_Jail.GetDoorOpenings(), "release opens the placed cell door");
		ctx.True(vector.Distance(body.GetOrigin(), LF_Jail.GetReleasePoint()) < 0.5, "release places the prisoner outside the cell");
		ctx.True(vector.Distance(body.GetOrigin(), LF_Jail.GetCellOrigin()) > 2, "the release point is not the cell");

		LF_Jail.TickOnline(30, online);
		ctx.Equal(1, LF_Jail.GetReleaseCount(prisonerUid), "a later tick does not release again");
		ctx.Equal(doorsBefore + 1, LF_Jail.GetDoorOpenings(), "a later tick does not open the door again");

		EL_TestCharacterFixture.Delete(body);
	}
}
