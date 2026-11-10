//------------------------------------------------------------------------------------------------
//! Medic events on the server bus (docs/architecture/events.md). Fields are plain values: uids and
//! ints, never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! A player went down but is not dead (LF_Downed). m_sDownerUid is empty when no player did it.
//! The murder and the hospital wake come later, with the real death (LF_OnPlayerKilled).
class LF_OnPlayerDowned : LF_Event
{
	string m_sPlayerUid;
	string m_sDownerUid;
	//! Where the player went down.
	vector m_vPosition;

	static LF_OnPlayerDowned Create(string playerUid, string downerUid, vector position)
	{
		LF_OnPlayerDowned e = new LF_OnPlayerDowned();
		e.m_sPlayerUid = playerUid;
		e.m_sDownerUid = downerUid;
		e.m_vPosition = position;
		return e;
	}
}

//! A downed player was dragged, carried, dropped or loaded into a vehicle (LF_Carry, #261).
//! m_sKind is "drag", "carry", "drop" or "load".
class LF_OnPlayerCarried : LF_Event
{
	string m_sPlayerUid;
	string m_sCarrierUid;
	string m_sKind;
	vector m_vPosition;

	static LF_OnPlayerCarried Create(string playerUid, string carrierUid, string kind, vector position)
	{
		LF_OnPlayerCarried e = new LF_OnPlayerCarried();
		e.m_sPlayerUid = playerUid;
		e.m_sCarrierUid = carrierUid;
		e.m_sKind = kind;
		e.m_vPosition = position;
		return e;
	}
}

//! A medic on duty brought a downed player back (LF_Revive, #259). The downed state is gone and the
//! bleed-out clock stopped by the time this is published.
class LF_OnPlayerRevived : LF_Event
{
	string m_sPlayerUid;
	string m_sMedicUid;
	//! Where the player was revived.
	vector m_vPosition;

	static LF_OnPlayerRevived Create(string playerUid, string medicUid, vector position)
	{
		LF_OnPlayerRevived e = new LF_OnPlayerRevived();
		e.m_sPlayerUid = playerUid;
		e.m_sMedicUid = medicUid;
		e.m_vPosition = position;
		return e;
	}
}

//! A medic put a downed patient in a hospital bed (LF_BedDelivery, #262). The patient is already
//! revived (LF_OnPlayerRevived is published first) and the reduced revive bill is written.
class LF_OnPatientDelivered : LF_Event
{
	string m_sPlayerUid;
	string m_sMedicUid;
	//! Bill number written for the patient, 0 when none (an NPC).
	int m_iBillNumber;
	vector m_vPosition;

	static LF_OnPatientDelivered Create(string playerUid, string medicUid, int billNumber, vector position)
	{
		LF_OnPatientDelivered e = new LF_OnPatientDelivered();
		e.m_sPlayerUid = playerUid;
		e.m_sMedicUid = medicUid;
		e.m_iBillNumber = billNumber;
		e.m_vPosition = position;
		return e;
	}
}

//! First aid bought a downed player more time (LF_FirstAid, #257). m_sHelperUid is empty when
//! the helper is not a player.
class LF_OnFirstAid : LF_Event
{
	string m_sPlayerUid;
	string m_sHelperUid;
	//! "bandage", "tourniquet" or "saline".
	string m_sKind;
	//! Bleed-out seconds actually added (the cap may have trimmed it).
	float m_fSeconds;
	vector m_vPosition;

	static LF_OnFirstAid Create(string playerUid, string helperUid, string kind, float seconds, vector position)
	{
		LF_OnFirstAid e = new LF_OnFirstAid();
		e.m_sPlayerUid = playerUid;
		e.m_sHelperUid = helperUid;
		e.m_sKind = kind;
		e.m_fSeconds = seconds;
		e.m_vPosition = position;
		return e;
	}
}

//! A player woke in hospital after a real death and got the New Life Rule wristband (#264).
//! Published by LF_NewLifeRule. A revive never publishes it. m_vScene is where they died.
class LF_OnNewLifeWristband : LF_Event
{
	string m_sPlayerUid;
	int m_iMinutes;
	vector m_vScene;

	static LF_OnNewLifeWristband Create(string playerUid, int minutes, vector scene)
	{
		LF_OnNewLifeWristband e = new LF_OnNewLifeWristband();
		e.m_sPlayerUid = playerUid;
		e.m_iMinutes = minutes;
		e.m_vScene = scene;
		return e;
	}
}

//! A wristband wearer came back near their death scene inside the window (#264). Published once
//! per wristband by LF_NewLifeRule; the incident log records it (kind "nlr"). Nothing blocks them.
class LF_OnNewLifeReturn : LF_Event
{
	string m_sPlayerUid;
	//! Where the player is now.
	vector m_vPosition;
	vector m_vScene;
	int m_iSecondsSinceWake;

	static LF_OnNewLifeReturn Create(string playerUid, vector position, vector scene, int secondsSinceWake)
	{
		LF_OnNewLifeReturn e = new LF_OnNewLifeReturn();
		e.m_sPlayerUid = playerUid;
		e.m_vPosition = position;
		e.m_vScene = scene;
		e.m_iSecondsSinceWake = secondsSinceWake;
		return e;
	}
}
