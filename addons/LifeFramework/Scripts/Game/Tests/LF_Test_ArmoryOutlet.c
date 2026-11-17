// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_ArmoryOutlet`: RedProof() lets the patrol officer's rank read as senior, so `a patrol officer is refused the rifle` fails; hook off: green.
// tier: WORLD
//! The police armory issues from its outlet by rank (#1763, Refs #1066): the armory reads the
//! `police_armory` outlet's issue offers, a patrol officer is refused the rifle and the refusal names
//! the senior rank, a senior officer draws it free with a serial booked to their account, and no
//! money moves.
class LF_Test_ArmoryOutlet : LF_Test
{
	protected static const string PATROL_UID = "armory-outlet-patrol";
	protected static const string SENIOR_UID = "armory-outlet-senior";
	protected static const ResourceName ISSUED_RIFLE = "{1E06C1BB38AFB556}Prefabs/Items/Police/LF_ArmoryM16.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "catalogue/armory-rank";
	}

	//------------------------------------------------------------------------------------------------
	//! The issued rifles an officer holds: in the bags, or equipped (a rifle lands in the weapon slot).
	protected static void FindRifles(IEntity officer, notnull array<IEntity> found)
	{
		found.Clear();
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(officer);
		if (inventory)
			found.InsertAll(LF_InventoryUtils.FindItemsByPrefab(inventory, ISSUED_RIFLE));

		EquipedWeaponStorageComponent weapons = LF_Component<EquipedWeaponStorageComponent>.Find(officer);
		if (!weapons)
			return;

		array<IEntity> equipped = {};
		weapons.GetAll(equipped);
		foreach (IEntity item : equipped)
		{
			if (LF_Utils.SamePrefab(LF_Utils.GetPrefabName(item), ISSUED_RIFLE) && found.Find(item) == -1)
				found.Insert(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ItemCatalogueConfig.Reload();

		LF_ItemCatalogueConfig config = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(config, "the item catalogue loads");
		if (!config)
			return;

		LF_OutletOffer offer = config.FindOffer(LF_PoliceRanks.ARMORY_OUTLET, "rifle_m16a2");
		ctx.True(offer && offer.m_bIssue, "the armory outlet issues the rifle free");
		ctx.True(offer && offer.m_iMinRank == LF_PoliceRanks.SENIOR, "the rifle is gated at the senior rank");

		IEntity patrolOfficer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity seniorOfficer = LF_TestCharacterFixture.Spawn(ctx);
		if (!patrolOfficer || !seniorOfficer)
			return;

		int patrolRank = LF_PoliceRanks.PATROL;
		if (RedProof())
			patrolRank = LF_PoliceRanks.SENIOR;

		LF_PlayerAccount patrolAccount = LF_TestPoliceKit.Account(PATROL_UID, true);
		patrolAccount.SetFaction(LF_Faction.POLICE);
		patrolAccount.SetOnDuty(true);
		patrolAccount.SetPoliceRank(patrolRank);
		LF_PoliceRanks.BindOfficer(patrolOfficer, PATROL_UID);

		LF_PlayerAccount seniorAccount = LF_TestPoliceKit.Account(SENIOR_UID, true);
		seniorAccount.SetFaction(LF_Faction.POLICE);
		seniorAccount.SetOnDuty(true);
		seniorAccount.SetPoliceRank(LF_PoliceRanks.SENIOR);
		LF_PoliceRanks.BindOfficer(seniorOfficer, SENIOR_UID);

		InventoryStorageManagerComponent patrolInventory = LF_InventoryUtils.GetResponsibleStorageManager(patrolOfficer);
		InventoryStorageManagerComponent seniorInventory = LF_InventoryUtils.GetResponsibleStorageManager(seniorOfficer);
		int patrolCash = LF_MoneyUtils.GetCash(patrolInventory);
		int seniorCash = LF_MoneyUtils.GetCash(seniorInventory);

		// A patrol officer is refused the rifle, and the refusal names the senior rank.
		string missing;
		ctx.False(LF_PoliceRanks.IssueItem(PATROL_UID, patrolOfficer, "rifle_m16a2", missing), "a patrol officer is refused the rifle");
		ctx.EqualStr("#" + LF_PoliceRanks.TitleKey(LF_PoliceRanks.SENIOR), missing, "the refusal names the senior rank");
		array<IEntity> rifles = {};
		FindRifles(patrolOfficer, rifles);
		ctx.Equal(0, rifles.Count(), "the refused officer carries no rifle");

		// A senior officer draws it with a serial booked to their account.
		ctx.True(LF_PoliceRanks.IssueItem(SENIOR_UID, seniorOfficer, "rifle_m16a2", missing), "a senior officer draws the rifle");
		ctx.EqualStr("", missing, "an issued item names nothing missing");
		FindRifles(seniorOfficer, rifles);
		ctx.Equal(1, rifles.Count(), "the senior officer carries one issued rifle");
		if (rifles.Count() == 1)
		{
			LF_SerialComponent stamp = LF_SerialComponent.Cast(rifles[0].FindComponent(LF_SerialComponent));
			ctx.NotNull(stamp, "the issued rifle carries a serial component");
			if (stamp)
			{
				ctx.False(stamp.GetSerial().IsEmpty(), "the issued rifle is stamped");
				LF_FenceSerialRow row = LF_FenceSerials.Find(stamp.GetSerial());
				ctx.NotNull(row, "the serial is in the book");
				if (row)
					ctx.EqualStr(SENIOR_UID, row.m_sBuyerUid, "the serial names the officer");
			}
		}

		// Free issue: no money moved for either officer.
		ctx.Equal(patrolCash, LF_MoneyUtils.GetCash(patrolInventory), "the refused officer's cash is untouched");
		ctx.Equal(seniorCash, LF_MoneyUtils.GetCash(seniorInventory), "the issue cost the senior officer nothing");

		// An item the armory does not issue is refused outright.
		ctx.False(LF_PoliceRanks.IssueItem(SENIOR_UID, seniorOfficer, "rifle_ak74", missing), "the armory does not issue what it has no offer for");

		LF_TestPoliceKit.Release(PATROL_UID);
		LF_TestPoliceKit.Release(SENIOR_UID);
		LF_TestCharacterFixture.Delete(patrolOfficer);
		LF_TestCharacterFixture.Delete(seniorOfficer);
	}
}
