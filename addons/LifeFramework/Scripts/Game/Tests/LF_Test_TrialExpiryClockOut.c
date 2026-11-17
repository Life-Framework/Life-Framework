// red-proof: observed 2026-11-18 with `--red` (RED-OK, same run): the hook skips LF_PoliceTrialSweep.EndIfExpired, so 'the sweep ends the officer's shift', 'an expired trial ends the shift' and 'the desk clock-out paid the shift' fail. Green with the fix.
// tier: WORLD
//! police/trial-expiry-clock-out (#2416): the sweep takes an officer whose trial ran out off duty
//! through the duty desk's own clock-out: the shift is paid, the police gear is stored and the
//! civilian inventory comes back.
class LF_Test_TrialExpiryClockOut : LF_Test
{
	protected static const string UID = "trial-clockout";
	protected static const int START = 2000000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/trial-expiry-clock-out";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PlayerAccount account = LF_TestPoliceKit.Account(UID, true);
		string characterId = account.GetActiveCharacter().GetId();
		IEntity body = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!body || ctx.FailureCount() > 0)
		{
			LF_TestCharacterFixture.Delete(body);
			LF_TestPoliceKit.Release(UID);
			return;
		}

		ctx.EqualStr("", LF_DutyLocker.ClockIn(account, body, LF_Faction.POLICE), "the officer clocks in");
		ctx.False(LF_TestCharacterFixture.HasItem(body, LF_TestCarriedInventoryKit.APPLE_PREFAB), "the civilian apples are stored at the desk");

		// The trial ran out while on duty.
		string path = "$profile:LifeFramework/test_trial_clockout.txt";
		LF_PoliceApplications.SetSavePath(path);
		LF_WhitelistManager.SetSavePath("$profile:LifeFramework/test_trial_clockout_whitelist.txt");
		LF_WhitelistManager.Reset();
		FileIO.DeleteFile(path);
		ctx.EqualStr("", LF_PoliceApplications.Submit(UID, "Officer Clockout", "To help", "Evenings", false), "application filed");
		ctx.True(LF_PoliceApplications.ApproveAt(UID, START, 168), "approved on trial");
		LF_PoliceApplications.ExpireTrials(START + 168 * 3600);

		account.AddUnpaidDutySeconds(100);
		bool ended = false;
		if (!RedProof())
			ended = LF_PoliceTrialSweep.EndIfExpired(0, account, body);

		ctx.True(ended, "the sweep ends the officer's shift");
		ctx.False(account.IsOnDuty(), "an expired trial ends the shift");
		ctx.True(account.GetFaction() == LF_Faction.CIVILIAN, "the officer is a civilian again");
		ctx.Equal(0, account.GetUnpaidDutySeconds(), "the desk clock-out paid the shift");
		ctx.False(account.IsDutyClockRunning(), "the duty clock stops");
		ctx.Equal(3, LF_InventoryUtils.GetAmount(body, LF_TestCarriedInventoryKit.APPLE_PREFAB), "the civilian apples are back");
		ctx.True(LF_StorageSnapshot.Has(characterId, LF_DutyLocker.SLOT_POLICE), "the police inventory is stored in its own slot");
		ctx.False(LF_PoliceTrialSweep.EndIfExpired(0, account, body), "a second sweep finds nothing to end");

		LF_StorageSnapshot.Clear(characterId, LF_DutyLocker.SLOT_POLICE);
		FileIO.DeleteFile(path);
		FileIO.DeleteFile("$profile:LifeFramework/test_trial_clockout_whitelist.txt");
		LF_PoliceApplications.SetSavePath("");
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
		LF_TestPoliceKit.Release(UID);
		LF_TestCharacterFixture.Delete(body);
	}
}
