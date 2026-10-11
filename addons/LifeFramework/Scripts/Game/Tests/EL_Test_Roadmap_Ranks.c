// red-proof: 2026-10-11 fast boot logs_2026-10-11_03-48-03 printed [ELTEST] FAIL roadmap/ranks / [ELTEST]   - patrol kit is the rank's prefabs only (expected 0, got 1)

// tier: LOGIC
class EL_Test_Roadmap_Ranks : EL_Test
{
	override string GetName()
	{
		return "roadmap/ranks";
	}

	override void Run(EL_TestContext ctx)
	{
		string sidearm = LF_PoliceRanks.SIDEARM;
		string rifle = LF_PoliceRanks.LONG_GUN;
		ctx.True(sidearm.IndexOf("Handgun_M9.et") != -1, "sidearm constant is the M9");
		ctx.True(rifle.IndexOf("Rifle_M16A2.et") != -1, "long gun constant is the M16A2");
		ctx.EqualStr("EL-Rank_Patrol", LF_PoliceRanks.TitleKey(LF_PoliceRanks.PATROL), "patrol title is the patrol officer key");
		ctx.EqualStr("EL-Rank_Senior", LF_PoliceRanks.TitleKey(LF_PoliceRanks.SENIOR), "senior title is the senior officer key");

		array<ResourceName> rankList = {};
		rankList.Insert(LF_PoliceRanks.SIDEARM);
		rankList.Insert("{1111111111111111}Prefabs/Weapons/NotInCatalog.et");
		array<ResourceName> catalog = {};
		catalog.Insert(LF_PoliceRanks.SIDEARM);
		catalog.Insert(LF_PoliceRanks.LONG_GUN);
		array<ResourceName> intersection = {};
		LF_PoliceRanks.Intersect(rankList, catalog, intersection);
		ctx.Equal(1, intersection.Count(), "kit list is the intersection of rank and config");
		if (intersection.Count() == 1)
			ctx.True(intersection.Get(0) == LF_PoliceRanks.SIDEARM, "intersection keeps the sidearm");

		array<ResourceName> patrolKit = {};
		LF_PoliceRanks.CollectKit(LF_PoliceRanks.PATROL, patrolKit);
		ctx.Equal(1, patrolKit.Count(), "patrol kit is the rank's prefabs only");
		if (patrolKit.Count() == 1)
			ctx.True(patrolKit.Get(0) == LF_PoliceRanks.SIDEARM, "patrol kit is the M9");
		ctx.False(KitHas(patrolKit, LF_PoliceRanks.LONG_GUN), "patrol kit does not include the rifle");

		array<ResourceName> seniorKit = {};
		LF_PoliceRanks.CollectKit(LF_PoliceRanks.SENIOR, seniorKit);
		ctx.Equal(1, seniorKit.Count(), "senior kit is the rank's prefabs only");
		if (seniorKit.Count() == 1)
			ctx.True(seniorKit.Get(0) == LF_PoliceRanks.LONG_GUN, "senior kit is the M16A2");
		ctx.False(KitHas(seniorKit, LF_PoliceRanks.SIDEARM), "senior kit does not include the sidearm");

		array<ResourceName> configured = {};
		configured.Insert(LF_PoliceRanks.SIDEARM);
		configured.Insert("{2222222222222222}Prefabs/Weapons/Missing/NotAWeapon.et");
		array<ResourceName> kept = {};
		int skipsBefore = LF_PoliceRanks.GetMissingPrefabSkips();
		LF_PoliceRanks.FilterLoadable(configured, kept);
		ctx.Equal(skipsBefore + 1, LF_PoliceRanks.GetMissingPrefabSkips(), "a missing prefab drops that line and is logged");
		ctx.Equal(1, kept.Count(), "the loadable line remains");
		ctx.True(LF_PoliceRanks.IsRankUsable(LF_PoliceRanks.PATROL), "a missing prefab leaves the rank usable");

		EL_PlayerAccount self = Police("roadmap-ranks-self", LF_PoliceRanks.PATROL, true);
		int selfBefore = LF_PoliceRanks.GetSelfPromotionRefusalCount();
		ctx.False(LF_PoliceRanks.RequestSelfPromotion("roadmap-ranks-self"), "self-promotion refuses");
		ctx.Equal(selfBefore + 1, LF_PoliceRanks.GetSelfPromotionRefusalCount(), "self-promotion refusal is logged");
		ctx.Equal(LF_PoliceRanks.PATROL, self.GetPoliceRank(), "self-promotion does not change rank");
		ctx.False(LF_PoliceRanks.Promote("roadmap-ranks-self", "roadmap-ranks-self"), "promoting yourself refuses");
		ctx.Equal(LF_PoliceRanks.PATROL, self.GetPoliceRank(), "a self promote still leaves the rank");

		EL_PlayerAccount senior = Police("roadmap-ranks-senior", LF_PoliceRanks.SENIOR, true);
		EL_PlayerAccount junior = Police("roadmap-ranks-junior", LF_PoliceRanks.PATROL, true);
		ctx.True(LF_PoliceRanks.Promote("roadmap-ranks-senior", "roadmap-ranks-junior"), "a higher rank can promote");
		ctx.Equal(LF_PoliceRanks.SENIOR, junior.GetPoliceRank(), "the lower officer becomes senior");

		EL_PlayerAccount low = Police("roadmap-ranks-low", LF_PoliceRanks.PATROL, true);
		EL_PlayerAccount other = Police("roadmap-ranks-other", LF_PoliceRanks.PATROL, false);
		ctx.False(LF_PoliceRanks.Promote("roadmap-ranks-low", "roadmap-ranks-other"), "a lower rank cannot promote");
		ctx.Equal(LF_PoliceRanks.PATROL, other.GetPoliceRank(), "a refused promote leaves the target");

		EL_PlayerAccount offSenior = Police("roadmap-ranks-off-senior", LF_PoliceRanks.SENIOR, false);
		EL_PlayerAccount waiting = Police("roadmap-ranks-waiting", LF_PoliceRanks.PATROL, true);
		ctx.False(LF_PoliceRanks.Promote("roadmap-ranks-off-senior", "roadmap-ranks-waiting"), "an off-duty senior cannot promote");
		ctx.Equal(LF_PoliceRanks.PATROL, waiting.GetPoliceRank(), "off-duty promote leaves the target");

		ctx.False(LF_PoliceRanks.PromotionDue(3599, LF_PoliceRanks.DUTY_SECONDS), "3599 seconds is short of promotion");
		ctx.True(LF_PoliceRanks.PromotionDue(LF_PoliceRanks.DUTY_SECONDS, LF_PoliceRanks.DUTY_SECONDS), "3600 seconds promotes");
		ctx.False(LF_PoliceRanks.PromotionDue(99999, 0), "the top rank has no time promotion");

		EL_PlayerAccount timed = Police("roadmap-ranks-timed", LF_PoliceRanks.PATROL, true);
		ctx.False(LF_PoliceRanks.AccrueDuty("roadmap-ranks-timed", 3599), "3599 seconds on duty does not promote");
		ctx.Equal(LF_PoliceRanks.PATROL, timed.GetPoliceRank(), "the officer is still patrol");
		ctx.Equal(3599, timed.GetOnDutySeconds(), "on-duty seconds accumulate");
		ctx.True(LF_PoliceRanks.AccrueDuty("roadmap-ranks-timed", 1), "the 3600th second promotes");
		ctx.Equal(LF_PoliceRanks.SENIOR, timed.GetPoliceRank(), "time on duty reaches senior");
		ctx.False(LF_PoliceRanks.AccrueDuty("roadmap-ranks-timed", LF_PoliceRanks.DUTY_SECONDS), "senior does not promote again");
		ctx.Equal(LF_PoliceRanks.SENIOR, timed.GetPoliceRank(), "there is no rank above senior");

		EL_PlayerAccount patrol = Police("roadmap-ranks-patrol", LF_PoliceRanks.PATROL, true);
		int arrestBefore = LF_PoliceRanks.GetArrestRefusalCount();
		ctx.False(LF_PoliceRanks.AuthorizeArrest(patrol), "a lower rank's arrest refuses");
		ctx.Equal(arrestBefore + 1, LF_PoliceRanks.GetArrestRefusalCount(), "the lower-rank arrest refusal is logged");
		ctx.EqualStr("arrest refused: rank 0 cannot arrest", LF_PoliceRanks.GetLastError(), "the arrest refusal is the rank log");
		int arrestAfter = LF_PoliceRanks.GetArrestRefusalCount();
		ctx.True(LF_PoliceRanks.AuthorizeArrest(senior), "a senior officer may arrest");
		ctx.Equal(arrestAfter, LF_PoliceRanks.GetArrestRefusalCount(), "an authorized arrest is not a refusal");

		ctx.Equal(LF_PoliceRanks.PATROL_FINE_CAP, LF_PoliceRanks.FineCap(LF_PoliceRanks.PATROL), "patrol fine cap is 1000");
		ctx.Equal(EL_PoliceUtils.MAX_FINE, LF_PoliceRanks.FineCap(LF_PoliceRanks.SENIOR), "senior fine cap is the existing maximum");
		ctx.True(LF_PoliceRanks.AuthorizeFine(patrol, LF_PoliceRanks.PATROL_FINE_CAP), "patrol can fine at the cap");
		int fineBefore = LF_PoliceRanks.GetFineRefusalCount();
		ctx.False(LF_PoliceRanks.AuthorizeFine(patrol, LF_PoliceRanks.PATROL_FINE_CAP + 1), "patrol cannot raise the fine cap");
		ctx.Equal(fineBefore + 1, LF_PoliceRanks.GetFineRefusalCount(), "a fine above the rank cap is logged");
		ctx.True(LF_PoliceRanks.AuthorizeFine(senior, EL_PoliceUtils.MAX_FINE), "senior can fine the existing maximum");

		ctx.True(LF_PoliceRanks.CanViewWantedList(patrol), "a lower rank can see the wanted list");
		ctx.True(LF_PoliceRanks.CanViewWantedList(senior), "a senior officer can see the wanted list");
		ctx.False(LF_PoliceRanks.CanViewWantedList(offSenior), "off duty cannot see the wanted list");

		EL_PlayerAccount civilian = EL_PlayerAccountManager.GetOrCreate("roadmap-ranks-civilian");
		civilian.SetFaction(EL_Faction.CIVILIAN);
		civilian.SetOnDuty(true);
		civilian.SetPoliceRank(LF_PoliceRanks.SENIOR);
		ctx.False(LF_PoliceRanks.Draw("roadmap-ranks-civilian", null), "a rank does not let a civilian draw a kit");
		ctx.False(LF_PoliceRanks.AuthorizeArrest(civilian), "a rank does not let a civilian arrest");
		ctx.False(LF_PoliceRanks.AccrueDuty("roadmap-ranks-civilian", 99999), "duty time does not promote a civilian");
		ctx.Equal(EL_Faction.CIVILIAN, civilian.GetFaction(), "rank does not change faction");
		ctx.Equal(LF_PoliceRanks.SENIOR, civilian.GetPoliceRank(), "a refused accrual does not change the stored rank");
		ctx.False(EL_WhitelistManager.IsWhitelistedForFaction("roadmap-ranks-civilian", EL_EFactionType.POLICE), "rank does not add a police whitelist entry");
		ctx.False(EL_WhitelistManager.IsWhitelistedForFaction("roadmap-ranks-senior", EL_EFactionType.POLICE), "promotion does not add a police whitelist entry");
	}

	//------------------------------------------------------------------------------------------------
	protected EL_PlayerAccount Police(string uid, int rank, bool onDuty)
	{
		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(EL_Faction.POLICE);
		account.SetPoliceRank(rank);
		account.SetOnDuty(onDuty);
		return account;
	}

	//------------------------------------------------------------------------------------------------
	protected bool KitHas(array<ResourceName> kit, ResourceName prefab)
	{
		if (!kit)
			return false;

		foreach (ResourceName candidate : kit)
		{
			if (candidate == prefab)
				return true;
		}

		return false;
	}
}

// tier: WORLD
class EL_Test_Roadmap_RanksWorld : EL_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const ResourceName JACKET_PREFAB = "{9F546CCA2582D16F}Prefabs/Characters/Uniforms/Jacket_M88.et";
	protected static const ResourceName BACKPACK_PREFAB = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";

	override string GetName()
	{
		return "e2e/roadmap-ranks";
	}

	override void Run(EL_TestContext ctx)
	{
		Resource sidearm = Resource.Load(LF_PoliceRanks.SIDEARM);
		Resource rifle = Resource.Load(LF_PoliceRanks.LONG_GUN);
		ctx.True(sidearm.IsValid(), "Handgun_M9.et loads");
		ctx.True(rifle.IsValid(), "Rifle_M16A2.et loads");

		IEntity armory = GetGame().GetWorld().FindEntityByName("Roadmap_Police_Armory");
		ctx.True(armory != null, "Roadmap_Police_Armory is placed");
		if (ctx.FailureCount() > 0)
			return;

		vector origin = armory.GetOrigin();
		ctx.EqualFloat(207, origin[0], 0.1, "armory is at X 207");
		ctx.EqualFloat(0, origin[1], 0.1, "armory is on the ground");
		ctx.EqualFloat(120, origin[2], 0.1, "armory is at Z 120");

		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(armory);
		ctx.NotNull(actions, "armory carries ActionsManagerComponent");
		if (ctx.FailureCount() > 0)
			return;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		LF_ArmoryDrawAction drawAction;
		bool promoteWired = false;
		foreach (BaseUserAction action : actionList)
		{
			if (LF_ArmoryDrawAction.Cast(action))
				drawAction = LF_ArmoryDrawAction.Cast(action);

			if (LF_RankPromoteAction.Cast(action))
				promoteWired = true;
		}

		ctx.True(drawAction != null, "draw action is on the armory");
		ctx.True(promoteWired, "promote action is on the armory");
		if (!drawAction)
			return;

		IEntity patrol = SpawnOfficer(ctx, "roadmap-ranks-e2e-patrol", LF_PoliceRanks.PATROL);
		if (ctx.FailureCount() > 0 || !patrol)
			return;

		ctx.True(drawAction.CanBePerformedScript(patrol), "on-duty patrol can use the armory action");
		drawAction.PerformAction(armory, patrol);
		ctx.Equal(1, HeldCount(patrol, LF_PoliceRanks.SIDEARM), "on-duty lower rank receives the M9");
		ctx.Equal(0, HeldCount(patrol, LF_PoliceRanks.LONG_GUN), "the lower rank does not receive the rifle");

		IEntity senior = SpawnOfficer(ctx, "roadmap-ranks-e2e-senior", LF_PoliceRanks.SENIOR);
		if (ctx.FailureCount() > 0 || !senior)
			return;

		ctx.True(drawAction.CanBePerformedScript(senior), "on-duty senior can use the armory action");
		drawAction.PerformAction(armory, senior);
		ctx.Equal(1, HeldCount(senior, LF_PoliceRanks.LONG_GUN), "higher rank receives Rifle_M16A2.et");
		ctx.Equal(0, HeldCount(senior, LF_PoliceRanks.SIDEARM), "the higher rank does not receive the sidearm");
	}

	//------------------------------------------------------------------------------------------------
	//! Deposit count misses an equipped weapon, and the weapon slot misses a
	//! backpack insert. The larger of the two is how many of this prefab the
	//! officer actually received.
	protected int HeldCount(IEntity officer, ResourceName prefab)
	{
		int deposited = EL_InventoryUtils.GetAmount(officer, prefab);
		if (deposited < 0)
			deposited = 0;

		int equippedCount = 0;
		EquipedWeaponStorageComponent weaponStorage = EL_Component<EquipedWeaponStorageComponent>.Find(officer);
		if (weaponStorage)
		{
			array<IEntity> equipped = {};
			weaponStorage.GetAll(equipped);
			foreach (IEntity item : equipped)
			{
				if (EL_Utils.GetPrefabName(item) == prefab)
					equippedCount++;
			}
		}

		if (equippedCount > deposited)
			return equippedCount;

		return deposited;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnOfficer(EL_TestContext ctx, string uid, int rank)
	{
		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(EL_Faction.POLICE);
		account.SetPoliceRank(rank);
		account.SetOnDuty(true);

		Resource characterResource = Resource.Load(CHARACTER_PREFAB);
		ctx.True(characterResource.IsValid(), "character prefab loads");
		if (!characterResource.IsValid())
			return null;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;

		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointForFaction("CIVILIAN");
		if (!spawnPoint)
			spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();
		ctx.True(spawnPoint != null, "a spawn point resolves for the officer");
		if (!spawnPoint)
			return null;

		vector position, yawPitchRoll;
		spawnPoint.GetPositionAndRotation(position, yawPitchRoll);
		Math3D.AnglesToMatrix(yawPitchRoll, params.Transform);
		params.Transform[3] = position + "0 0.1 0";

		IEntity character = GetGame().SpawnEntityPrefab(characterResource, GetGame().GetWorld(), params);
		ctx.True(character != null, "officer character spawns");
		if (!character)
			return null;

		InventoryStorageManagerComponent storage = EL_InventoryUtils.GetResponsibleStorageManager(character);
		ctx.NotNull(storage, "officer resolves an inventory storage manager");
		if (!storage)
			return null;

		IEntity jacket = EL_Utils.SpawnEntityPrefab(JACKET_PREFAB, character.GetOrigin());
		ctx.True(jacket != null, "jacket spawns for the officer");
		if (!jacket)
			return null;

		ctx.True(storage.TryInsertItem(jacket, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "jacket inserts into the officer loadout");
		if (ctx.FailureCount() > 0)
			return null;

		IEntity backpack = EL_Utils.SpawnEntityPrefab(BACKPACK_PREFAB, character.GetOrigin());
		ctx.True(backpack != null, "backpack spawns for the officer");
		if (!backpack)
			return null;

		ctx.True(storage.TryInsertItem(backpack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "backpack inserts into the officer loadout");
		if (ctx.FailureCount() > 0)
			return null;

		LF_PoliceRanks.BindOfficer(character, uid);
		return character;
	}
}
