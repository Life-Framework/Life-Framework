//------------------------------------------------------------------------------------------------
//! Crime events on the server bus (docs/architecture/events.md). Fields are plain values: uids and
//! ints, never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! A crime was reported (the wanted report is filed). m_sOffence is an OFFENCE_ localization key.
class LF_OnCrimeCommitted : LF_Event
{
	string m_sPlayerUid;
	string m_sOffence;

	static LF_OnCrimeCommitted Create(string playerUid, string offence)
	{
		LF_OnCrimeCommitted e = new LF_OnCrimeCommitted();
		e.m_sPlayerUid = playerUid;
		e.m_sOffence = offence;
		return e;
	}
}

//! A player character died. m_sKillerUid is empty when no player killed them.
class LF_OnPlayerKilled : LF_Event
{
	string m_sVictimUid;
	string m_sKillerUid;
	//! Where the victim died. Zero when the publisher did not know.
	vector m_vPosition;
	//! Prefab of the killer's current weapon; empty when unarmed or unknown.
	string m_sWeapon;
	//! LF_Murder's rule counts this death as murder (set before the wanted decision).
	bool m_bMurder;

	static LF_OnPlayerKilled Create(string victimUid, string killerUid)
	{
		LF_OnPlayerKilled e = new LF_OnPlayerKilled();
		e.m_sVictimUid = victimUid;
		e.m_sKillerUid = killerUid;
		return e;
	}
}

//! An owner reported their stolen vehicle (LF_StolenVehicles.Report); the plate is on the wanted board.
class LF_OnVehicleReportedStolen : LF_Event
{
	string m_sOwnerUid;
	string m_sVehicleId;
	string m_sPlate;

	static LF_OnVehicleReportedStolen Create(string ownerUid, string vehicleId, string plate)
	{
		LF_OnVehicleReportedStolen e = new LF_OnVehicleReportedStolen();
		e.m_sOwnerUid = ownerUid;
		e.m_sVehicleId = vehicleId;
		e.m_sPlate = plate;
		return e;
	}
}

//! A chop shop paid out for a vehicle and it is gone (LF_ChopShopComponent.ServerCollect). m_sOwnerUid is
//! empty for a car nobody owned.
class LF_OnVehicleChopped : LF_Event
{
	string m_sChopperUid;
	string m_sShopId;
	string m_sVehicleId;
	string m_sPlate;
	string m_sOwnerUid;
	int m_iPayout;
	int m_iParts;

	static LF_OnVehicleChopped Create(string chopperUid, string shopId, string vehicleId, string plate, string ownerUid, int payout, int parts)
	{
		LF_OnVehicleChopped e = new LF_OnVehicleChopped();
		e.m_sChopperUid = chopperUid;
		e.m_sShopId = shopId;
		e.m_sVehicleId = vehicleId;
		e.m_sPlate = plate;
		e.m_sOwnerUid = ownerUid;
		e.m_iPayout = payout;
		e.m_iParts = parts;
		return e;
	}
}

//! An officer recovered a reported vehicle; it is back in the owner's garage.
class LF_OnVehicleRecovered : LF_Event
{
	string m_sOfficerUid;
	string m_sOwnerUid;
	string m_sVehicleId;
	string m_sPlate;

	static LF_OnVehicleRecovered Create(string officerUid, string ownerUid, string vehicleId, string plate)
	{
		LF_OnVehicleRecovered e = new LF_OnVehicleRecovered();
		e.m_sOfficerUid = officerUid;
		e.m_sOwnerUid = ownerUid;
		e.m_sVehicleId = vehicleId;
		e.m_sPlate = plate;
		return e;
	}
}

//! A robber tried a vault with a fence tool (#273). Published by LF_Bank.Rob. m_sOutcome is
//! "opened", "jammed", or "refused" (no tool in hand). m_sSerial is the tool's fence serial, empty
//! for a refusal; the evidence locker reads the same serial off the bagged drill.
class LF_OnVaultBreach : LF_Event
{
	string m_sPlayerUid;
	string m_sVaultId;
	string m_sTool;
	string m_sSerial;
	string m_sOutcome;

	static LF_OnVaultBreach Create(string playerUid, string vaultId, string tool, string serial, string outcome)
	{
		LF_OnVaultBreach e = new LF_OnVaultBreach();
		e.m_sPlayerUid = playerUid;
		e.m_sVaultId = vaultId;
		e.m_sTool = tool;
		e.m_sSerial = serial;
		e.m_sOutcome = outcome;
		return e;
	}
}

//! A forced-entry heist paid out (#274 cash van, #275 police armory). Published once by LF_Bank
//! when the target opens. m_sKind is "bags" (cash left lying as carried bags) or "weapons".
//! m_iAmount is the cash across the bags, or the number of weapons. m_sSerials lists the book
//! serials stamped on stolen weapons, comma separated; empty for cash.
class LF_OnHeistLoot : LF_Event
{
	string m_sPlayerUid;
	string m_sTargetId;
	string m_sKind;
	int m_iAmount;
	string m_sSerials;

	static LF_OnHeistLoot Create(string playerUid, string targetId, string kind, int amount, string serials)
	{
		LF_OnHeistLoot e = new LF_OnHeistLoot();
		e.m_sPlayerUid = playerUid;
		e.m_sTargetId = targetId;
		e.m_sKind = kind;
		e.m_iAmount = amount;
		e.m_sSerials = serials;
		return e;
	}
}
