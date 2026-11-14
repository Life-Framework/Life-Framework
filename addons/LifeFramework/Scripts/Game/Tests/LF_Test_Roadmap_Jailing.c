// red-proof: expected wanted 0 after expiry and ran `tools\cli test --no-build --tier fast`
// on 2026-10-11. The suite reported
// `[LFTEST] - expiry reduces wanted by the configured amount (expected 0, got 1)`.
// Restored the expected value to 1.
// manual: dedicated server, officer on one client, suspect on another.
// The officer uses Send to jail on the suspect. The suspect appears at
// Roadmap_Jail_Cell. Leaving the volume while Roadmap_Jail_Door is locked
// returns them. An open door does not. Restart mid-sentence keeps the
// remaining time. Vehicle escort is the next slice and is not covered here.

// tier: WORLD (needs DebugWorld's placed cell, hideout door or plaza zone; LOGIC runs on any map, #1047)
class LF_Test_Roadmap_Jailing : LF_Test
{
	override string GetName()
	{
		return "roadmap/jailing";
	}

	override void Run(LF_TestContext ctx)
	{
		string officerUid = "roadmap-jailing-officer";
		string prisonerUid = "roadmap-jailing-prisoner";
		string cleanUid = "roadmap-jailing-clean";
		string civilianUid = "roadmap-jailing-civilian";
		string zeroUid = "roadmap-jailing-zero";
		vector placed = "48 0 64";
		vector legacy = "127 1 137";

		LF_Jail.Configure(80, 150, 1, 5);
		LF_Jail.SetCellOrigin(placed);

		vector jail = LF_PoliceUtils.GetJailPosition();
		ctx.True(SamePlace(jail, placed), "GetJailPosition reads the placed cell origin");
		ctx.False(SamePlace(jail, legacy), "GetJailPosition is not the literal 127 1 137");

		ctx.Equal(0, LF_PoliceUtils.SentenceSeconds(0, 80, 150), "a clean level has no sentence");
		ctx.Equal(80, LF_PoliceUtils.SentenceSeconds(1, 80, 150), "one wanted level is one config step");
		ctx.Equal(150, LF_PoliceUtils.SentenceSeconds(2, 80, 150), "sentence length clamps to the config max");
		ctx.Equal(150, LF_Jail.SentenceSeconds(2), "the jail config uses that clamp");

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		LF_PlayerAccount civilian = LF_PlayerAccountManager.GetOrCreate(civilianUid);
		civilian.SetFaction(LF_Faction.CIVILIAN);
		civilian.SetOnDuty(true);
		LF_PlayerAccount clean = LF_PlayerAccountManager.GetOrCreate(cleanUid);
		clean.SetWantedLevel(0);
		LF_PlayerAccount prisoner = LF_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(2);

		ctx.False(LF_Jail.Arrest(officerUid, cleanUid, null), "arrest of a clean player refuses");
		ctx.Equal(0, clean.GetWantedLevel(), "a refused arrest leaves a clean player clean");
		ctx.False(LF_Jail.IsServing(cleanUid), "a clean player is not serving");
		ctx.False(LF_Jail.Arrest(civilianUid, prisonerUid, null), "a civilian arrest refuses");
		ctx.Equal(2, prisoner.GetWantedLevel(), "a refused arrest does not change wanted");

		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, null), "arrest of a wanted player starts the sentence");
		ctx.EqualFloat(150, LF_Jail.GetRemaining(prisonerUid), 0.01, "remaining time is the clamped sentence");
		ctx.Equal(2, prisoner.GetWantedLevel(), "the sentence does not clear wanted");
		ctx.False(LF_Jail.ReleaseEarly(prisonerUid), "early release is refused");
		ctx.EqualFloat(150, LF_Jail.GetRemaining(prisonerUid), 0.01, "a refused release keeps the remaining time");

		ctx.EqualFloat(0, LF_Jail.Advance(prisonerUid, 150), 0.01, "advancing the full sentence ends it");
		ctx.Equal(1, prisoner.GetWantedLevel(), "expiry reduces wanted by the configured amount");
		ctx.False(LF_Jail.IsServing(prisonerUid), "expiry frees the player");
		ctx.EqualFloat(0, LF_Jail.Advance(prisonerUid, 10), 0.01, "a finished sentence does not advance again");
		ctx.Equal(1, prisoner.GetWantedLevel(), "a second advance does not reduce wanted again");

		LF_Jail.Configure(0, 0, 1, 5);
		LF_PlayerAccount zero = LF_PlayerAccountManager.GetOrCreate(zeroUid);
		zero.SetWantedLevel(3);
		ctx.True(LF_Jail.Arrest(officerUid, zeroUid, null), "a zero-duration arrest is accepted");
		ctx.False(LF_Jail.IsServing(zeroUid), "a zero-duration sentence releases immediately");
		ctx.Equal(2, zero.GetWantedLevel(), "a zero-duration release reduces wanted");

		LF_Jail.SyncPlacedCell();
	}

	protected bool SamePlace(vector a, vector b)
	{
		if (Math.AbsFloat(a[0] - b[0]) > 0.05)
			return false;
		if (Math.AbsFloat(a[1] - b[1]) > 0.05)
			return false;
		if (Math.AbsFloat(a[2] - b[2]) > 0.05)
			return false;
		return true;
	}
}

// tier: WORLD
class LF_Test_Roadmap_JailingWorld : LF_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	override string GetName()
	{
		return "e2e/roadmap-jailing";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity cell = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.JAIL_CELL);
		ctx.True(cell != null, "Annex_Jail_Cell is placed");
		IEntity desk = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.POLICE_DESK);
		ctx.True(desk != null, "Annex_Police_Desk is placed");
		if (ctx.FailureCount() > 0)
			return;

		LF_Jail.SetCellOrigin(cell.GetOrigin());
		LF_Jail.SetDoorTarget(LF_TestAnnex.JAIL_DOOR);

		vector legacy = "127 1 137";
		ctx.True(SamePlace(LF_PoliceUtils.GetJailPosition(), cell.GetOrigin()), "GetJailPosition reads the placed cell origin");
		ctx.False(SamePlace(LF_PoliceUtils.GetJailPosition(), legacy), "the placed cell is not the literal desk");

		Resource characterRes = Resource.Load(CHARACTER_PREFAB);
		ctx.True(characterRes.IsValid(), "suspect character prefab loads");
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = "10 0 10";
		IEntity standIn = GetGame().SpawnEntityPrefab(characterRes, GetGame().GetWorld(), params);
		ctx.True(standIn != null, "suspect stand-in spawns");
		if (ctx.FailureCount() > 0)
			return;

		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(standIn);
		ctx.NotNull(actions, "suspect carries ActionsManagerComponent");
		if (ctx.FailureCount() > 0)
			return;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		bool arrestWired = false;
		foreach (BaseUserAction action : actionList)
		{
			if (LF_JailArrestAction.Cast(action))
				arrestWired = true;
		}

		ctx.True(arrestWired, "the arrest action is on the suspect");
		ctx.False(SamePlace(standIn.GetOrigin(), cell.GetOrigin()), "the stand-in starts away from the cell");

		LF_Jail.Configure(0, 0, 1, 8);
		string officerUid = "roadmap-jailing-e2e-officer";
		string prisonerUid = "roadmap-jailing-e2e-prisoner";
		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		LF_PlayerAccount prisoner = LF_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(2);

		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, standIn), "arrest of a wanted test account runs");
		ctx.True(SamePlace(standIn.GetOrigin(), LF_Jail.GetReleasePoint()), "a zero-duration arrest releases the stand-in outside Annex_Jail_Cell");
		ctx.False(LF_Jail.IsServing(prisonerUid), "zero-duration config releases");
		ctx.Equal(1, prisoner.GetWantedLevel(), "zero-duration config reduces wanted");
	}

	protected bool SamePlace(vector a, vector b)
	{
		if (Math.AbsFloat(a[0] - b[0]) > 0.05)
			return false;
		if (Math.AbsFloat(a[1] - b[1]) > 0.05)
			return false;
		if (Math.AbsFloat(a[2] - b[2]) > 0.05)
			return false;
		return true;
	}
}

// tier: PERSISTENCE
class LF_Test_Roadmap_JailingPersistence : LF_Test
{
	override string GetName()
	{
		return "roadmap/jailing-save";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Jail.SyncPlacedCell();
		LF_Jail.Configure(10, 100, 1, 4);

		string officerUid = "roadmap-jailing-save-officer";
		string prisonerUid = "roadmap-jailing-save-prisoner";

		// Sentences are saved since version 5, so the reload boot of the two-boot e2e
		// restores this prisoner's sentence from the save boot. Start from no sentence for
		// this uid, keeping every other sentence.
		array<ref LF_JailRecord> live = LF_Jail.ExportAll();
		array<ref LF_JailRecord> others = new array<ref LF_JailRecord>();
		foreach (LF_JailRecord record : live)
		{
			if (record && record.m_sPlayerUid != prisonerUid)
				others.Insert(record);
		}
		LF_Jail.ApplyAll(others);

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		LF_PlayerAccount prisoner = LF_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(2);

		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, null), "a sentence exists to save");
		ctx.EqualFloat(16, LF_Jail.Advance(prisonerUid, 4), 0.01, "four seconds are served before the save");

		LF_JailRecord saved = LF_Jail.Export(prisonerUid);
		ctx.NotNull(saved, "export returns the remaining sentence");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualFloat(16, saved.m_fRemaining, 0.01, "the saved record keeps the remaining seconds");
		ctx.True(saved.m_bServing, "the saved record is still serving");
		ctx.False(LF_Jail.Apply(null), "a null record is rejected");
		ctx.False(LF_Jail.Apply(LF_JailRecord.Create("", 16, true)), "a record with no player is rejected");

		ctx.EqualFloat(12, LF_Jail.Advance(prisonerUid, 4), 0.01, "live time moves on after the save");
		ctx.True(LF_Jail.Apply(saved), "apply accepts the saved record");
		ctx.EqualFloat(16, LF_Jail.GetRemaining(prisonerUid), 0.01, "remaining seconds survive apply-save");
		ctx.True(LF_Jail.IsServing(prisonerUid), "serving survives apply-save");
		ctx.True(LF_Jail.Apply(saved), "apply is idempotent");
		ctx.EqualFloat(16, LF_Jail.GetRemaining(prisonerUid), 0.01, "a second apply keeps the same remaining seconds");
	}
}
