// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_ArmoryKitTurnIn`: RED PROOF OK, fails "the rifle's serial leaves the book (expected 0, got 1)" and "the officer no longer carries the rifle" with the DrawAt skipped.
// tier: WORLD
//! police/armory-kit-turn-in (#2416): an officer carrying their issued rifle uses the armory: the
//! rifle is taken off them and its serial leaves the book, and nothing new is issued in that use.
class LF_Test_ArmoryKitTurnIn : LF_Test
{
	protected static const string OFFICER_UID = "armory-turn-in-senior";
	protected static const ResourceName ISSUED_RIFLE = "{1E06C1BB38AFB556}Prefabs/Items/Police/LF_ArmoryM16.et";
	protected static const int T0 = 1800000000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/armory-kit-turn-in";
	}

	//------------------------------------------------------------------------------------------------
	//! The serial of the first issued rifle the officer holds in a bag or a weapon slot.
	protected static string SerialOf(IEntity holder)
	{
		array<IEntity> found = {};
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(holder);
		if (inventory)
			found.InsertAll(LF_InventoryUtils.FindItemsByPrefab(inventory, ISSUED_RIFLE));

		EquipedWeaponStorageComponent weapons = LF_Component<EquipedWeaponStorageComponent>.Find(holder);
		if (weapons)
		{
			array<IEntity> equipped = {};
			weapons.GetAll(equipped);
			foreach (IEntity item : equipped)
			{
				if (item && LF_Utils.SamePrefab(LF_Utils.GetPrefabName(item), ISSUED_RIFLE) && found.Find(item) == -1)
					found.Insert(item);
			}
		}

		foreach (IEntity weapon : found)
		{
			if (!weapon)
				continue;

			LF_SerialComponent stamp = LF_SerialComponent.Cast(weapon.FindComponent(LF_SerialComponent));
			if (stamp && !stamp.GetSerial().IsEmpty())
				return stamp.GetSerial();
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ItemCatalogueConfig.Reload();

		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer)
			return;

		array<ref LF_ArmoryKitRow> booksBefore = LF_ArmoryKits.ExportAll();
		array<ref LF_FenceSerialRow> serialsBefore = LF_FenceSerials.ExportAll();
		LF_ArmoryKits.ApplyAll(null);

		LF_PlayerAccount account = LF_TestPoliceKit.Account(OFFICER_UID, true);
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		account.SetPoliceRank(LF_PoliceRanks.SENIOR);
		LF_PoliceRanks.BindOfficer(officer, OFFICER_UID);

		string missing;
		ctx.True(LF_PoliceRanks.IssueItem(OFFICER_UID, officer, "rifle_m16a2", missing), "the officer is issued the rifle");
		string serial = SerialOf(officer);
		ctx.False(serial.IsEmpty(), "the issued rifle carries a serial");

		array<string> kit = {serial};
		LF_ArmoryKits.RecordIssue(OFFICER_UID, T0, kit);
		ctx.Equal(1, LF_ArmoryKits.Outstanding(OFFICER_UID), "the rifle is booked out to the officer");

		bool drew = true;
		if (!RedProof())
			drew = LF_PoliceRanks.DrawAt(OFFICER_UID, officer, T0 + 30);

		ctx.False(drew, "a turn-in use issues nothing");
		ctx.Equal(0, LF_ArmoryKits.Outstanding(OFFICER_UID), "the rifle's serial leaves the book");
		ctx.EqualStr("", SerialOf(officer), "the officer no longer carries the rifle");
		ctx.True(LF_PoliceRanks.GetLastError().Contains("turned in"), "the log names the turn-in");

		LF_ArmoryKits.ApplyAll(booksBefore);
		LF_FenceSerials.ApplyAll(serialsBefore);
		LF_TestPoliceKit.Release(OFFICER_UID);
		LF_TestCharacterFixture.Delete(officer);
	}
}
