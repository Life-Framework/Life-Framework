// red-proof: observed 2026-11-17 with "armorykits" not registered and the DrawAt gates off (log test-20261117-224005): FAIL "armorykits is a registered payload", "the refusal names the cooldown". Restored: green.
// tier: PERSISTENCE
//! police/armory-cooldown-persist (#2416): an officer's 2 h armory cooldown and the serials still out
//! are saved in the "armorykits" payload, come back on load (idempotent), and hold across a clock-out.
//! Across a restart (`tools\cli wt e2e`): every non-reload run books one e2e officer's kit; the
//! reload boot finds it with its issue time and serial.
class LF_Test_ArmoryCooldownPersist : LF_Test
{
	protected static const string OFFICER = "armory-cooldown-officer";
	protected static const string E2E_OFFICER = "armory-cooldown-e2e-officer";
	protected static const string E2E_SERIAL = "police_armory-e2e-1";
	protected static const int T0 = 1800000000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/armory-cooldown-persist";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

#ifdef LF_E2E_LOAD_BOOT
		// The reload boot: the e2e officer's book came back from the real save.
		ctx.True(LF_ArmoryKits.LastIssueUnix(E2E_OFFICER) > 0, "the e2e officer's issue time is back after the restart");
		ctx.True(LF_ArmoryKits.IsOutstanding(E2E_OFFICER, E2E_SERIAL), "the e2e officer's serial is still out after the restart");
		int e2eIssued = LF_ArmoryKits.LastIssueUnix(E2E_OFFICER);
		ctx.Equal(LF_ArmoryKits.CooldownSeconds() - 60, LF_ArmoryKits.CooldownLeft(E2E_OFFICER, e2eIssued + 60), "the restored issue time still runs the cooldown");
		return;
#endif

		ctx.True(LF_PersistenceComponentSerializer.PayloadSinceVersion("armorykits") > 0, "armorykits is a registered payload");

		array<ref LF_ArmoryKitRow> before = LF_ArmoryKits.ExportAll();
		LF_ArmoryKits.ApplyAll(null);

		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(OFFICER);
		account.SetFaction(LF_Faction.POLICE);
		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDuty(true);
		LF_PoliceRanks.OnDutyChangedAt(account, true, 0);

		array<string> serials = {"police_armory-7001"};
		LF_ArmoryKits.RecordIssue(OFFICER, T0, serials);
		ctx.Equal(LF_ArmoryKits.CooldownSeconds() - 600, LF_ArmoryKits.CooldownLeft(OFFICER, T0 + 600), "ten minutes after an issue the cooldown has 1 h 50 left");

		// Round trip through the payload, as a restart does.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, "armorykits");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("police_armory-7001"), "the save carries the serial still out");

		LF_ArmoryKits.ApplyAll(null);
		ctx.Equal(0, LF_ArmoryKits.CooldownLeft(OFFICER, T0 + 600), "a fresh boot has no cooldown");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		if (!RedProof())
			LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "armorykits");

		ctx.Equal(LF_ArmoryKits.CooldownSeconds() - 600, LF_ArmoryKits.CooldownLeft(OFFICER, T0 + 600), "the cooldown is back after the load");
		ctx.True(LF_ArmoryKits.IsOutstanding(OFFICER, "police_armory-7001"), "the serial is still out after the load");
		ctx.Equal(1, LF_ArmoryKits.ApplyAll(LF_ArmoryKits.ExportAll()), "re-applying the rows applies the same one");

		// Reported lost, the cooldown still holds the next issue, through a clock-out.
		LF_PoliceRanks.DrawAt(OFFICER, null, T0 + 700);
		LF_PoliceRanks.DrawAt(OFFICER, null, T0 + 705);
		ctx.Equal(0, LF_ArmoryKits.Outstanding(OFFICER), "the kit is reported lost");
		account.SetOnDuty(false);
		LF_PoliceRanks.OnDutyChangedAt(account, false, 1000);
		account.SetOnDuty(true);
		LF_PoliceRanks.OnDutyChangedAt(account, true, 2000);
		ctx.False(LF_PoliceRanks.DrawAt(OFFICER, null, T0 + 900), "the armory refuses within the cooldown after a clock-out");
		ctx.True(LF_PoliceRanks.GetLastError().Contains("cooldown"), "the refusal names the cooldown");
		LF_PoliceRanks.DrawAt(OFFICER, null, T0 + LF_ArmoryKits.CooldownSeconds() + 1);
		ctx.False(LF_PoliceRanks.GetLastError().Contains("cooldown"), "two hours after the issue the cooldown is over");

		account.SetOnDuty(false);
		LF_PoliceRanks.OnDutyChangedAt(account, false, 3000);
		LF_ArmoryKits.ApplyAll(before);

		// Every non-reload run (the e2e save boot among them) leaves the e2e officer's kit booked.
		if (!LF_ArmoryKits.IsOutstanding(E2E_OFFICER, E2E_SERIAL))
		{
			array<string> e2eSerials = {E2E_SERIAL};
			LF_ArmoryKits.RecordIssue(E2E_OFFICER, System.GetUnixTime(), e2eSerials);
		}
		ctx.True(LF_ArmoryKits.IsOutstanding(E2E_OFFICER, E2E_SERIAL), "the save boot booked the e2e officer's kit");
	}
}
