// red-proof: Observed 2026-11-16: `--only LF_Test_OutletSerialWired` with the armory caller reverted to `Issue(uid, prefab)` failed `the issued rifle's serial names the armory`; with the outlet passed it is green. `--red LF_Test_OutletSerialWired` (RedProof() sells with no outlet): RED-OK, fails `the sold pistol's serial names the gun counter`.
// tier: WORLD
//! The serial names the outlet that issued or sold the weapon (#2153, Refs #2123): a rifle drawn from
//! the armory reads "LF-police_armory-NNNN", and a weapon the gun counter sells reads
//! "LF-gun_counter-NNNN", so a seized gun traces back to where it came from.
class LF_Test_OutletSerialWired : LF_Test
{
	protected static const string SENIOR_UID = "outlet-wired-senior";
	protected static const string BUYER_UID = "outlet-wired-buyer";
	protected static const string COUNTER = "gun_counter";
	protected static const ResourceName ISSUED_RIFLE = "{1E06C1BB38AFB556}Prefabs/Items/Police/LF_ArmoryM16.et";
	protected static const ResourceName SERIALLED_PISTOL = "{C95F0E5226F71550}Prefabs/Items/Police/LF_ArmoryM9.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "catalogue/outlet-serial-wired";
	}

	//------------------------------------------------------------------------------------------------
	//! The serial of the first item of a prefab a character holds in its bags or equipped weapon slot.
	protected static string SerialOf(IEntity holder, ResourceName prefab)
	{
		array<IEntity> found = {};
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(holder);
		if (inventory)
			found.InsertAll(LF_InventoryUtils.FindItemsByPrefab(inventory, prefab));

		EquipedWeaponStorageComponent weapons = LF_Component<EquipedWeaponStorageComponent>.Find(holder);
		if (weapons)
		{
			array<IEntity> equipped = {};
			weapons.GetAll(equipped);
			foreach (IEntity item : equipped)
			{
				if (LF_Utils.SamePrefab(LF_Utils.GetPrefabName(item), prefab) && found.Find(item) == -1)
					found.Insert(item);
			}
		}

		foreach (IEntity weapon : found)
		{
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
		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer || !buyer)
			return;

		array<ref LF_FenceSerialRow> before = LF_FenceSerials.ExportAll();
		LF_FenceSerials.ApplyAll(null);

		// The armory: a senior officer draws the rifle, and the serial names the armory.
		LF_PlayerAccount account = LF_TestPoliceKit.Account(SENIOR_UID, true);
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		account.SetPoliceRank(LF_PoliceRanks.SENIOR);
		LF_PoliceRanks.BindOfficer(officer, SENIOR_UID);

		string missing;
		bool issued = LF_PoliceRanks.IssueItem(SENIOR_UID, officer, "rifle_m16a2", missing);
		ctx.True(issued, "a senior officer draws the rifle");
		string issuedSerial = SerialOf(officer, ISSUED_RIFLE);
		ctx.False(issuedSerial.IsEmpty(), "the issued rifle is stamped");
		ctx.EqualStr(LF_PoliceRanks.ARMORY_OUTLET, LF_OutletSerial.Outlet(issuedSerial), "the issued rifle's serial names the armory");

		// The gun counter: a weapon the counter sells is registered to its buyer with the counter in the serial.
		string outlet = COUNTER;
		if (RedProof())
			outlet = "";

		LF_InventoryUtils.AddAmount(buyer, SERIALLED_PISTOL, 1);
		ctx.Equal(1, LF_FenceSerials.OnWeaponSale(buyer, BUYER_UID, SERIALLED_PISTOL, 1, outlet), "the sold pistol is stamped");
		string soldSerial = SerialOf(buyer, SERIALLED_PISTOL);
		ctx.False(soldSerial.IsEmpty(), "the sold pistol carries a serial");
		ctx.EqualStr(COUNTER, LF_OutletSerial.Outlet(soldSerial), "the sold pistol's serial names the gun counter");
		LF_FenceSerialRow row = LF_FenceSerials.Find(soldSerial);
		ctx.NotNull(row, "the sold pistol's serial is in the book");
		if (row)
			ctx.EqualStr(BUYER_UID, row.m_sBuyerUid, "the serial names the buyer");

		// A weapon prefab with no serial component cannot be stamped: the sale stands, nothing is booked.
		int booked = LF_FenceSerials.ExportAll().Count();
		LF_InventoryUtils.AddAmount(buyer, "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et", 1);
		ctx.Equal(0, LF_FenceSerials.OnWeaponSale(buyer, BUYER_UID, "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et", 1, COUNTER), "a plain weapon with no serial component is not stamped");
		ctx.Equal(booked, LF_FenceSerials.ExportAll().Count(), "nothing is booked for it");

		LF_FenceSerials.ApplyAll(before);
		LF_TestPoliceKit.Release(SENIOR_UID);
		LF_TestCharacterFixture.Delete(officer);
		LF_TestCharacterFixture.Delete(buyer);
	}
}
