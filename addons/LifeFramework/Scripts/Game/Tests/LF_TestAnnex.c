//------------------------------------------------------------------------------------------------
//! Names of the neutral fixtures the tests use. They live in the DebugWorld annex layers
//! (Annex.layer), a fenced yard away from the town, so the village can move to the pack
//! without taking a test along (#323 batch 0, docs/architecture/core-vs-pack.md). A test never
//! quotes a village entity (`Roadmap_*`, `Sign_*`, `Street_*`); `validate-annex-fixtures`
//! rejects it. Each fixture is the same prefab and components as its town counterpart, with its
//! own breach, vault, hideout and shop ids so both can stand in the world at once.
//! Place a new fixture with `tools\cli place`, never typed coordinates (docs/placement-tool.md).
class LF_TestAnnex
{
	static const string BANK_VAULT = "Annex_Bank_Vault";
	static const string BANK_ALARM_BOX = "Annex_Bank_AlarmBox";
	static const string BANK_BAG = "Annex_Bank_Bag";
	static const string BANK_ROB_KIT = "Annex_Bank_RobKit";
	static const string CASH_VAN = "Annex_Cash_Van";
	static const string CASH_VAN_BOX = "Annex_Cash_Van_Box";
	static const string BREACH_DOOR = "Annex_Breach_Door";
	static const string BREACH_CHARGE = "Annex_Breach_Charge";
	static const string LOCKPICK_DOOR = "Annex_Lockpick_Door";
	static const string LOCKPICK_CAR = "Annex_Lockpick_Car";
	static const string CHOP_SHOP = "Annex_ChopShop";
	static const string CHOP_SHOP_ID = "annex-chop";
	static const string VEHICLE_S105 = "Annex_Vehicle_S105";
	static const string POLICE_DESK = "Annex_Police_Desk";
	static const string POLICE_ARMORY = "Annex_Police_Armory";
	static const string POLICE_WANTED_BOARD = "Annex_Police_WantedBoard";
	static const string POLICE_EVIDENCE_LOCKER = "Annex_Police_EvidenceLocker";
	static const string ESCORT_CAR = "Annex_Escort_Car";
	static const string JAIL_DOOR = "Annex_Jail_Door";
	//! A bare marker: the jail has one real cell, so the fixture carries no LF_JailCellComponent.
	static const string JAIL_CELL = "Annex_Jail_Cell";
	static const string HIDEOUT_ID = "Annex_Hideout";
	static const string HIDEOUT_DOOR = "Annex_Hideout_Door";
	static const string HIDEOUT_MACHINE = "Annex_Hideout_Machine";
	static const string HIDEOUT_STASH = "Annex_Hideout_Stash";

	//------------------------------------------------------------------------------------------------
	//! \return The annex fixture entity, or null (with an Error) when the layer does not place it.
	static IEntity Find(string name)
	{
		if (!GetGame() || !GetGame().GetWorld())
		{
			LF_Debug.Error("TestAnnex", "no world to look up " + name);
			return null;
		}

		IEntity entity = GetGame().GetWorld().FindEntityByName(name);
		if (!entity)
			LF_Debug.Error("TestAnnex", "annex fixture is not placed: " + name);

		return entity;
	}
}
