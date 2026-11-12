// red-proof: on 2026-11-07 dropped `& WANTED_MASK` in LF_AccountStateBits.GetWantedLevel,
// made LF_LicenseBits.Has return true, replaced the MayJoinFaction check in
// LF_CharacterCreationRules.RefuseFaction with `if (false)` (that rule is gone with the faction
// screen, M1-11; dedicated/creation-rules now proves a fresh account registers), and made
// LF_RobAction.CommitRobbery return payout without an account. Ran
// `tools\cli test --no-build --tier fast`; observed
// `[LFTEST] SUMMARY tier=fast passed=79 failed=4 total=83` with
// `[LFTEST]   - wanted clamps to its field (expected 7, got 2047)`,
// `[LFTEST]   - an absent license stays absent`,
// `[LFTEST]   - police pick without the server whitelist is refused (expected '#EL-CC_Error_FactionDenied', got '')`,
// `[LFTEST]   - a robbery without an account pays nothing (expected 0, got 150)`.
// Restored all four; the fast tier went green again.
// dedicated/creation-rules red-proof (M1-11), observed 2026-11-08 in the same --only run: a faction gate put
// back into RefuseCharacter failed it with `- a fresh account registers with no faction pick (expected '',
// got '#EL-CC_Error_FactionLocked')`. Restored; green.
// dedicated/replication-world (WORLD) red-proof: see the comment above that class.

// tier: LOGIC
class LF_Test_AccountStateBits : LF_Test
{
	override string GetName()
	{
		return "dedicated/account-state-bits";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(0, LF_AccountStateBits.Encode(false, LF_Faction.POLICE, true, 3, 1), "no account packs to 0");
		ctx.False(LF_AccountStateBits.HasAccount(0), "0 reads as no account");
		ctx.False(LF_AccountStateBits.IsCivilian(0), "no account is not offered civilian crime");
		ctx.False(LF_AccountStateBits.IsPolice(0), "no account is not police");

		int officer = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, 1);
		ctx.True(LF_AccountStateBits.HasAccount(officer), "officer has an account");
		ctx.True(LF_AccountStateBits.IsOnDutyPolice(officer), "on-duty officer decodes as on-duty police");
		ctx.Equal(1, LF_AccountStateBits.GetPoliceRank(officer), "officer rank round trips");
		ctx.Equal(0, LF_AccountStateBits.GetWantedLevel(officer), "officer wanted round trips");

		int offDuty = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, false, 0, 0);
		ctx.True(LF_AccountStateBits.IsPolice(offDuty), "off-duty officer is still police");
		ctx.False(LF_AccountStateBits.IsOnDutyPolice(offDuty), "off-duty officer is not on duty");

		int robber = LF_AccountStateBits.Encode(true, LF_Faction.CIVILIAN, false, 4, 0);
		ctx.True(LF_AccountStateBits.IsCivilian(robber), "civilian decodes as civilian");
		ctx.Equal(4, LF_AccountStateBits.GetWantedLevel(robber), "wanted 4 round trips");
		ctx.Equal(LF_Faction.CIVILIAN, LF_AccountStateBits.GetFaction(robber), "civilian faction round trips");

		int clamped = LF_AccountStateBits.Encode(true, LF_Faction.CIVILIAN, false, 99, 999);
		ctx.Equal(LF_AccountStateBits.WANTED_MASK, LF_AccountStateBits.GetWantedLevel(clamped), "wanted clamps to its field");
		ctx.Equal(LF_AccountStateBits.RANK_MASK, LF_AccountStateBits.GetPoliceRank(clamped), "rank clamps to its field");
		ctx.True(LF_AccountStateBits.IsCivilian(clamped), "an out-of-range value does not spill into the faction");
		ctx.False(LF_AccountStateBits.IsOnDuty(clamped), "an out-of-range value does not spill into the duty bit");

		LF_PlayerAccount account = LF_PlayerAccount.Create("dedicated-state-account");
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		account.SetPoliceRank(1);
		account.SetWantedLevel(2);
		int fromAccount = LF_AccountStateBits.FromAccount(account);
		ctx.True(LF_AccountStateBits.IsOnDutyPolice(fromAccount), "FromAccount carries faction and duty");
		ctx.Equal(1, LF_AccountStateBits.GetPoliceRank(fromAccount), "FromAccount carries rank");
		ctx.Equal(2, LF_AccountStateBits.GetWantedLevel(fromAccount), "FromAccount carries wanted");
		ctx.Equal(0, LF_AccountStateBits.FromAccount(null), "a null account is no account");
	}
}

// tier: LOGIC
class LF_Test_LicenseBits : LF_Test
{
	override string GetName()
	{
		return "dedicated/license-bits";
	}

	override void Run(LF_TestContext ctx)
	{
		array<LF_ELicenseType> order = LF_LicenseBits.Order();
		ctx.True(order.Count() <= 31, "every license type fits in one int");

		array<LF_ELicenseType> licenses = {LF_ELicenseType.UNEMPLOYED, LF_ELicenseType.POLICE_ACCESS, LF_ELicenseType.CRIMINAL_CLOTHES};
		int bits = LF_LicenseBits.Encode(licenses);
		ctx.True(LF_LicenseBits.Has(bits, LF_ELicenseType.POLICE_ACCESS), "POLICE_ACCESS (99) survives the mask");
		ctx.True(LF_LicenseBits.Has(bits, LF_ELicenseType.CRIMINAL_CLOTHES), "CRIMINAL_CLOTHES (205) survives the mask");
		ctx.True(LF_LicenseBits.Has(bits, LF_ELicenseType.UNEMPLOYED), "UNEMPLOYED (0) survives the mask");
		ctx.False(LF_LicenseBits.Has(bits, LF_ELicenseType.MEDIC_ACCESS), "an absent license stays absent");

		array<LF_ELicenseType> decoded = {};
		LF_LicenseBits.Decode(bits, decoded);
		ctx.Equal(3, decoded.Count(), "decode returns exactly the encoded licenses");
		ctx.True(decoded.Contains(LF_ELicenseType.POLICE_ACCESS), "decode returns POLICE_ACCESS");

		// Every type round trips on its own bit, so no two types share one.
		int all = LF_LicenseBits.Encode(order);
		array<LF_ELicenseType> everything = {};
		LF_LicenseBits.Decode(all, everything);
		ctx.Equal(order.Count(), everything.Count(), "all license types round trip");
		ctx.Equal(0, LF_LicenseBits.Encode(new array<LF_ELicenseType>()), "no licenses is 0");
	}
}

// tier: LOGIC
class LF_Test_CharacterCreationRules : LF_Test
{
	override string GetName()
	{
		return "dedicated/creation-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		// No faction screen (M1-11): a fresh account registers without any faction pick. The
		// faction gate #130 left in RefuseCharacter is gone, or new players never reach the form.
		LF_PlayerAccount fresh = LF_PlayerAccount.Create("dedicated-creation-fresh");
		ctx.EqualStr("", LF_CharacterCreationRules.RefuseCharacter(fresh, "Ada", "Lovelace", 36), "a fresh account registers with no faction pick");
		ctx.EqualStr("#EL-CC_Error_NoAccount", LF_CharacterCreationRules.RefuseCharacter(null, "Ada", "Lovelace", 36), "no account is refused");

		LF_PlayerAccount chosen = fresh;
		ctx.EqualStr("#EL-CC_Error_Fields", LF_CharacterCreationRules.RefuseCharacter(chosen, "  ", "Lovelace", 36), "a blank first name is refused");
		ctx.EqualStr("#EL-CC_Error_Age", LF_CharacterCreationRules.RefuseCharacter(chosen, "Ada", "Lovelace", 17), "age 17 is refused");
		ctx.EqualStr("#EL-CC_Error_Age", LF_CharacterCreationRules.RefuseCharacter(chosen, "Ada", "Lovelace", 81), "age 81 is refused");
		ctx.EqualStr("#EL-CC_Error_NameLength", LF_CharacterCreationRules.RefuseCharacter(chosen, "Adaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "Lovelace", 36), "a 33 character name is refused");

		chosen.AddCharacter(LF_PlayerCharacter.Create("{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et", "Ada", "Lovelace", 36), true);
		ctx.EqualStr("#EL-CC_Error_Exists", LF_CharacterCreationRules.RefuseCharacter(chosen, "Bob", "Builder", 40), "a second character over RPC is refused");
	}
}

// tier: LOGIC
class LF_Test_RobNoAccount : LF_Test
{
	override string GetName()
	{
		return "dedicated/rob-no-account";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_RobAction.ResetSession();
		float next = 0;
		int paid = LF_RobAction.CommitRobbery("dedicated-rob-no-account", null, 150, 1, 0, 0, 60, 0, 100000, next);
		ctx.Equal(0, paid, "a robbery without an account pays nothing");
		ctx.EqualStr("robbery refused: no account", LF_RobAction.GetLastRefusal(), "the no-account refusal is logged");
		ctx.Equal(0, LF_RobAction.GetAlertCount(), "a refused robbery does not alert police");
		ctx.Equal(0, LF_RobAction.GetCashPaid(), "a refused robbery records no cash");
		ctx.True(next == 0, "a refused robbery does not start the cooldown");
		LF_RobAction.ResetSession();
	}
}

// red-proof: on 2026-11-07 removed the `m_iLF_AccountState = state;` write in
// SCR_ChimeraCharacter.LF_SetAccountState and the SyncTarget call in LF_Breach.Open, ran
// `tools\cli test --no-build --tier all`, observed
// `[LFTEST] SUMMARY tier=all passed=132 failed=1 total=133` with
// `[LFTEST]   - the character carries the written account state (expected 263, got 0)` and
// `[LFTEST]   - arm sets the replicated open flag for clients`. Restored both.
// tier: WORLD
class LF_Test_AccountReplicationWorld : LF_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	override string GetName()
	{
		return "dedicated/replication-world";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(Replication.IsServer(), "replication test runs on the server");

		LF_GameModeRoleplay gameMode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "DebugWorld game mode is LF_GameModeRoleplay");
		if (gameMode)
		{
			LF_AccountReplication.RefreshPoliceOnDuty();
			ctx.Equal(LF_AccountReplication.CountPoliceOnDuty(), gameMode.LF_GetPoliceOnDuty(), "replicated police count matches the server count");
		}

		Resource res = Resource.Load(CHARACTER_PREFAB);
		ctx.True(res.IsValid(), "character prefab loads");
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = "0 500 0";
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params));
		ctx.True(character != null, "character spawns");
		if (!character)
			return;

		ctx.Equal(0, LF_AccountReplication.GetState(character), "a body with no player reads as no account on the server");
		int state = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, 1);
		character.LF_SetAccountState(state);
		ctx.Equal(state, character.LF_GetAccountState(), "the character carries the written account state");
		SCR_EntityHelper.DeleteEntityAndChildren(character);

		IEntity door = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.BREACH_DOOR);
		LF_BreachTargetComponent target;
		if (door)
			target = LF_Component<LF_BreachTargetComponent>.Find(door);

		ctx.NotNull(target, "Annex_Breach_Door carries LF_BreachTargetComponent");
		if (!target)
			return;

		string uid = "dedicated-breach-mirror";
		string policeUid = "dedicated-breach-mirror-police";
		LF_PlayerAccountManager.GetOrCreate(uid);
		LF_PlayerAccountManager.GetOrCreate(policeUid).SetFaction(LF_Faction.POLICE);

		LF_Breach.Repair(LF_TestAnnex.BREACH_DOOR, policeUid);
		ctx.False(target.IsReplicatedOpen(), "repair clears the replicated open flag");
		ctx.True(target.CanArmForOffer(), "a locked door offers arm");
		ctx.True(LF_Breach.Arm(LF_TestAnnex.BREACH_DOOR, uid), "zero-fuse arm opens the door");
		ctx.True(target.IsReplicatedOpen(), "arm sets the replicated open flag for clients");
		ctx.False(target.CanArmForOffer(), "an open door does not offer arm");
		ctx.True(LF_Breach.Repair(LF_TestAnnex.BREACH_DOOR, policeUid), "police repair runs");
		ctx.False(target.IsReplicatedOpen(), "repair clears the replicated open flag again");
	}
}
