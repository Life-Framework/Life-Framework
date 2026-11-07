// red-proof: PENDING - not observed yet. On 2026-11-07 the four breaks below were
// applied and `tools\cli test --no-build --tier fast` was run twice, but the test
// server crashed before any test ran: the Steam Server install was mid-restore and
// vanilla GameLib failed to compile (`BanIssuerUnknown: cannot derive from sealed
// type 'BanIssuerBase'`). `tools\cli validate` (validate-scripts) compiles this
// file. To prove red: drop `& WANTED_MASK` in EL_AccountStateBits.GetWantedLevel,
// make EL_LicenseBits.Has return true, replace the MayJoinFaction check in
// EL_CharacterCreationRules.RefuseFaction with `if (false)`, and make
// EL_RobAction.CommitRobbery return payout when the account is missing. Run the fast
// tier, record the observed [ELTEST] lines here, and restore.

// tier: LOGIC
class EL_Test_AccountStateBits : EL_Test
{
	override string GetName()
	{
		return "dedicated/account-state-bits";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.Equal(0, EL_AccountStateBits.Encode(false, EL_Faction.POLICE, true, 3, 1), "no account packs to 0");
		ctx.False(EL_AccountStateBits.HasAccount(0), "0 reads as no account");
		ctx.False(EL_AccountStateBits.IsCivilian(0), "no account is not offered civilian crime");
		ctx.False(EL_AccountStateBits.IsPolice(0), "no account is not police");

		int officer = EL_AccountStateBits.Encode(true, EL_Faction.POLICE, true, 0, 1);
		ctx.True(EL_AccountStateBits.HasAccount(officer), "officer has an account");
		ctx.True(EL_AccountStateBits.IsOnDutyPolice(officer), "on-duty officer decodes as on-duty police");
		ctx.Equal(1, EL_AccountStateBits.GetPoliceRank(officer), "officer rank round trips");
		ctx.Equal(0, EL_AccountStateBits.GetWantedLevel(officer), "officer wanted round trips");

		int offDuty = EL_AccountStateBits.Encode(true, EL_Faction.POLICE, false, 0, 0);
		ctx.True(EL_AccountStateBits.IsPolice(offDuty), "off-duty officer is still police");
		ctx.False(EL_AccountStateBits.IsOnDutyPolice(offDuty), "off-duty officer is not on duty");

		int robber = EL_AccountStateBits.Encode(true, EL_Faction.CIVILIAN, false, 4, 0);
		ctx.True(EL_AccountStateBits.IsCivilian(robber), "civilian decodes as civilian");
		ctx.Equal(4, EL_AccountStateBits.GetWantedLevel(robber), "wanted 4 round trips");
		ctx.Equal(EL_Faction.CIVILIAN, EL_AccountStateBits.GetFaction(robber), "civilian faction round trips");

		int clamped = EL_AccountStateBits.Encode(true, EL_Faction.CIVILIAN, false, 99, 999);
		ctx.Equal(EL_AccountStateBits.WANTED_MASK, EL_AccountStateBits.GetWantedLevel(clamped), "wanted clamps to its field");
		ctx.Equal(EL_AccountStateBits.RANK_MASK, EL_AccountStateBits.GetPoliceRank(clamped), "rank clamps to its field");
		ctx.True(EL_AccountStateBits.IsCivilian(clamped), "an out-of-range value does not spill into the faction");
		ctx.False(EL_AccountStateBits.IsOnDuty(clamped), "an out-of-range value does not spill into the duty bit");

		EL_PlayerAccount account = EL_PlayerAccount.Create("dedicated-state-account");
		account.SetFaction(EL_Faction.POLICE);
		account.SetOnDuty(true);
		account.SetPoliceRank(1);
		account.SetWantedLevel(2);
		int fromAccount = EL_AccountStateBits.FromAccount(account);
		ctx.True(EL_AccountStateBits.IsOnDutyPolice(fromAccount), "FromAccount carries faction and duty");
		ctx.Equal(1, EL_AccountStateBits.GetPoliceRank(fromAccount), "FromAccount carries rank");
		ctx.Equal(2, EL_AccountStateBits.GetWantedLevel(fromAccount), "FromAccount carries wanted");
		ctx.Equal(0, EL_AccountStateBits.FromAccount(null), "a null account is no account");
	}
}

// tier: LOGIC
class EL_Test_LicenseBits : EL_Test
{
	override string GetName()
	{
		return "dedicated/license-bits";
	}

	override void Run(EL_TestContext ctx)
	{
		array<EL_ELicenseType> order = EL_LicenseBits.Order();
		ctx.True(order.Count() <= 31, "every license type fits in one int");

		array<EL_ELicenseType> licenses = {EL_ELicenseType.UNEMPLOYED, EL_ELicenseType.POLICE_ACCESS, EL_ELicenseType.CRIMINAL_CLOTHES};
		int bits = EL_LicenseBits.Encode(licenses);
		ctx.True(EL_LicenseBits.Has(bits, EL_ELicenseType.POLICE_ACCESS), "POLICE_ACCESS (99) survives the mask");
		ctx.True(EL_LicenseBits.Has(bits, EL_ELicenseType.CRIMINAL_CLOTHES), "CRIMINAL_CLOTHES (205) survives the mask");
		ctx.True(EL_LicenseBits.Has(bits, EL_ELicenseType.UNEMPLOYED), "UNEMPLOYED (0) survives the mask");
		ctx.False(EL_LicenseBits.Has(bits, EL_ELicenseType.MEDIC_ACCESS), "an absent license stays absent");

		array<EL_ELicenseType> decoded = {};
		EL_LicenseBits.Decode(bits, decoded);
		ctx.Equal(3, decoded.Count(), "decode returns exactly the encoded licenses");
		ctx.True(decoded.Contains(EL_ELicenseType.POLICE_ACCESS), "decode returns POLICE_ACCESS");

		// Every type round trips on its own bit, so no two types share one.
		int all = EL_LicenseBits.Encode(order);
		array<EL_ELicenseType> everything = {};
		EL_LicenseBits.Decode(all, everything);
		ctx.Equal(order.Count(), everything.Count(), "all license types round trip");
		ctx.Equal(0, EL_LicenseBits.Encode(new array<EL_ELicenseType>()), "no licenses is 0");
	}
}

// tier: LOGIC
class EL_Test_CharacterCreationRules : EL_Test
{
	override string GetName()
	{
		return "dedicated/creation-rules";
	}

	override void Run(EL_TestContext ctx)
	{
		string uid = "dedicated-creation-not-whitelisted";
		EL_PlayerAccount fresh = EL_PlayerAccount.Create(uid);

		ctx.EqualStr("", EL_CharacterCreationRules.RefuseFaction(fresh, EL_Faction.CIVILIAN, uid), "civilian pick is accepted");
		ctx.EqualStr("#EL-CC_Error_FactionDenied", EL_CharacterCreationRules.RefuseFaction(fresh, EL_Faction.POLICE, uid), "police pick without the server whitelist is refused");
		ctx.EqualStr("#EL-CC_Error_FactionDenied", EL_CharacterCreationRules.RefuseFaction(fresh, 7, uid), "an unknown faction value is refused");
		ctx.EqualStr("#EL-CC_Error_NoAccount", EL_CharacterCreationRules.RefuseFaction(null, EL_Faction.CIVILIAN, uid), "no account is refused");
		ctx.EqualStr("#EL-CC_Error_NoAccount", EL_CharacterCreationRules.RefuseFaction(fresh, EL_Faction.CIVILIAN, ""), "an empty uid is refused");

		EL_PlayerAccount chosen = EL_PlayerAccount.Create("dedicated-creation-chosen");
		chosen.SetFaction(EL_Faction.CIVILIAN);
		ctx.EqualStr("#EL-CC_Error_FactionLocked", EL_CharacterCreationRules.RefuseFaction(chosen, EL_Faction.POLICE, "dedicated-creation-chosen"), "a chosen faction cannot be re-picked over RPC");

		ctx.EqualStr("", EL_CharacterCreationRules.RefuseSpawnLocation(EL_Faction.POLICE, "station"), "police may spawn at the station");
		ctx.EqualStr("#EL-CC_Error_Location", EL_CharacterCreationRules.RefuseSpawnLocation(EL_Faction.CIVILIAN, "station"), "a civilian may not pick the police station");
		ctx.EqualStr("#EL-CC_Error_Location", EL_CharacterCreationRules.RefuseSpawnLocation(EL_Faction.CIVILIAN, "bogus"), "an unknown location is refused");
		ctx.EqualStr("", EL_CharacterCreationRules.RefuseSpawnLocation(EL_Faction.CIVILIAN, ""), "no location is the degrade path");

		ctx.EqualStr("", EL_CharacterCreationRules.RefuseCharacter(chosen, "Ada", "Lovelace", 36), "a valid form is accepted");
		ctx.EqualStr("#EL-CC_Error_Fields", EL_CharacterCreationRules.RefuseCharacter(chosen, "  ", "Lovelace", 36), "a blank first name is refused");
		ctx.EqualStr("#EL-CC_Error_Age", EL_CharacterCreationRules.RefuseCharacter(chosen, "Ada", "Lovelace", 17), "age 17 is refused");
		ctx.EqualStr("#EL-CC_Error_Age", EL_CharacterCreationRules.RefuseCharacter(chosen, "Ada", "Lovelace", 81), "age 81 is refused");
		ctx.EqualStr("#EL-CC_Error_NameLength", EL_CharacterCreationRules.RefuseCharacter(chosen, "Adaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "Lovelace", 36), "a 33 character name is refused");
		ctx.EqualStr("#EL-CC_Error_FactionLocked", EL_CharacterCreationRules.RefuseCharacter(fresh, "Ada", "Lovelace", 36), "a character needs a chosen faction");

		chosen.AddCharacter(EL_PlayerCharacter.Create("{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et", "Ada", "Lovelace", 36), true);
		ctx.EqualStr("#EL-CC_Error_Exists", EL_CharacterCreationRules.RefuseCharacter(chosen, "Bob", "Builder", 40), "a second character over RPC is refused");
	}
}

// tier: LOGIC
class EL_Test_RobNoAccount : EL_Test
{
	override string GetName()
	{
		return "dedicated/rob-no-account";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_RobAction.ResetSession();
		float next = 0;
		int paid = EL_RobAction.CommitRobbery("dedicated-rob-no-account", null, 150, 1, 0, 0, 60, 0, 100000, next);
		ctx.Equal(0, paid, "a robbery without an account pays nothing");
		ctx.EqualStr("robbery refused: no account", EL_RobAction.GetLastRefusal(), "the no-account refusal is logged");
		ctx.Equal(0, EL_RobAction.GetAlertCount(), "a refused robbery does not alert police");
		ctx.Equal(0, EL_RobAction.GetCashPaid(), "a refused robbery records no cash");
		ctx.True(next == 0, "a refused robbery does not start the cooldown");
		EL_RobAction.ResetSession();
	}
}

// tier: WORLD
class EL_Test_AccountReplicationWorld : EL_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	override string GetName()
	{
		return "dedicated/replication-world";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.True(Replication.IsServer(), "replication test runs on the server");

		EL_GameModeRoleplay gameMode = EL_GameModeRoleplay.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "DebugWorld game mode is EL_GameModeRoleplay");
		if (gameMode)
		{
			EL_AccountReplication.RefreshPoliceOnDuty();
			ctx.Equal(EL_AccountReplication.CountPoliceOnDuty(), gameMode.EL_GetPoliceOnDuty(), "replicated police count matches the server count");
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

		ctx.Equal(0, EL_AccountReplication.GetState(character), "a body with no player reads as no account on the server");
		int state = EL_AccountStateBits.Encode(true, EL_Faction.POLICE, true, 0, 1);
		character.EL_SetAccountState(state);
		ctx.Equal(state, character.EL_GetAccountState(), "the character carries the written account state");
		SCR_EntityHelper.DeleteEntityAndChildren(character);

		IEntity door = GetGame().GetWorld().FindEntityByName("Roadmap_Breach_Door");
		LF_BreachTargetComponent target;
		if (door)
			target = EL_Component<LF_BreachTargetComponent>.Find(door);

		ctx.NotNull(target, "Roadmap_Breach_Door carries LF_BreachTargetComponent");
		if (!target)
			return;

		string uid = "dedicated-breach-mirror";
		string policeUid = "dedicated-breach-mirror-police";
		EL_PlayerAccountManager.GetOrCreate(uid);
		EL_PlayerAccountManager.GetOrCreate(policeUid).SetFaction(EL_Faction.POLICE);

		LF_Breach.Repair("Roadmap_Breach_Door", policeUid);
		ctx.False(target.IsReplicatedOpen(), "repair clears the replicated open flag");
		ctx.True(target.CanArmForOffer(), "a locked door offers arm");
		ctx.True(LF_Breach.Arm("Roadmap_Breach_Door", uid), "zero-fuse arm opens the door");
		ctx.True(target.IsReplicatedOpen(), "arm sets the replicated open flag for clients");
		ctx.False(target.CanArmForOffer(), "an open door does not offer arm");
		ctx.True(LF_Breach.Repair("Roadmap_Breach_Door", policeUid), "police repair runs");
		ctx.False(target.IsReplicatedOpen(), "repair clears the replicated open flag again");
	}
}
