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
	static const string JOB_DEPOT = "Annex_Job_Depot";
	static const string JOB_DROP = "Annex_Job_Drop";
	static const string JOB_TAXI_PAD = "Annex_Job_TaxiPad";
	static const string JOB_FUEL_CAN = "Annex_Job_FuelCan";
	static const string ORG_DESK = "Annex_Org_Desk";
	static const string FURNACE = "Annex_Furnace";
	static const string ATM = "Annex_ATM";
	static const string VEHICLE_LOT = "Annex_Vehicle_Lot";
	static const string LOT_ID = "annex-lot";
	static const string GARAGE = "Annex_Garage";
	static const string GARAGE_PAD_ID = "annex-garage";
	static const string IMPOUND_DESK = "Annex_Impound_Desk";
	static const string IMPOUND_PAD_ID = "annex-impound";
	static const string CHAINS_BARREL_1 = "Annex_Chains_Barrel_1";
	static const string CHAINS_BARREL_2 = "Annex_Chains_Barrel_2";
	static const string CHAINS_BARREL_3 = "Annex_Chains_Barrel_3";
	static const string CHAINS_SITE = "Annex_Chains_Site";
	static const string APPLE_TRADER = "Annex_AppleTrader";
	static const string APPLE_GATHER_TREE = "Annex_AppleGatherTree";
	static const string APPLE_TRADE_CONTAINER = "Annex_AppleTradeContainer";
	static const string APPLE_TRADE_CONTAINER3 = "Annex_AppleTradeContainer3";
	static const string HOSPITAL_BED_1 = "Annex_Hospital_Bed_1";
	static const string HOSPITAL_AMBULANCE = "Annex_Hospital_Ambulance";
	static const string HOSPITAL_DUTY_DESK = "Annex_Hospital_Duty_Desk";
	static const string DISPATCH_CONSOLE = "Annex_Dispatch_Console";
	static const string DISPATCH_POLICE_BOARD = "Annex_Dispatch_PoliceBoard";
	static const string DISPATCH_MEDIC_BOARD = "Annex_Dispatch_MedicBoard";
	static const string TEST_TILL = "Annex_Test_Till";
	static const string FUEL_TILL = "Annex_Fuel_Till";
	static const string SIGN_SAFE_ZONE_SPAWN = "Annex_Sign_SafeZone_Spawn";
	static const string SIGN_SAFE_ZONE_EAST = "Annex_Sign_SafeZone_East";
	static const string SIGN_LAWLESS_ENTRY = "Annex_Sign_Lawless_Entry";
	static const string SIGN_LAWLESS_PIT = "Annex_Sign_Lawless_Pit";

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
