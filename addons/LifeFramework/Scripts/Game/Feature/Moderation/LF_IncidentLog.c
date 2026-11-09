//------------------------------------------------------------------------------------------------
//! One incident: who, whom, where, when, how. Positions are plain floats so the record saves.
class LF_IncidentRecord : Managed
{
	int m_iNumber;
	//! LF_IncidentConfig.KIND_*.
	string m_sKind;
	//! Real clock, seconds since 1970 (System.GetUnixTime).
	int m_iTime;
	//! The player it happened to (victim, or the one whose cash moved).
	string m_sSubjectUid;
	//! The other player (killer, attacker); empty when there is none.
	string m_sOtherUid;
	float m_fX;
	float m_fY;
	float m_fZ;
	//! Cash moved, or damage dealt (rounded).
	int m_iAmount;
	//! The weapon prefab for a death or damage; the money source for cash.
	string m_sHow;
	//! Death: LF_Murder counted it as murder.
	bool m_bMurder;

	//------------------------------------------------------------------------------------------------
	vector GetPosition()
	{
		return Vector(m_fX, m_fY, m_fZ);
	}

	//------------------------------------------------------------------------------------------------
	LF_IncidentRecord Copy()
	{
		LF_IncidentRecord copy = new LF_IncidentRecord();
		copy.m_iNumber = m_iNumber;
		copy.m_sKind = m_sKind;
		copy.m_iTime = m_iTime;
		copy.m_sSubjectUid = m_sSubjectUid;
		copy.m_sOtherUid = m_sOtherUid;
		copy.m_fX = m_fX;
		copy.m_fY = m_fY;
		copy.m_fZ = m_fZ;
		copy.m_iAmount = m_iAmount;
		copy.m_sHow = m_sHow;
		copy.m_bMurder = m_bMurder;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! The incident log (#239, epic #145 Moderation). A server-only, append-only record of deaths,
//! sampled player-on-player damage, and cash moves, so an admin rules from facts.
//!
//! It is a subscriber on LF_EventBus: LF_OnPlayerKilled, LF_OnPlayerDamaged, LF_OnMoneyMoved,
//! LF_OnPlayerDowned (kind "downed", #255: the downer is the other party). No
//! feature calls it. Never replicated: nothing here is sent to a client.
//! BOUNDED: config m_iMaxRecords (oldest go first) and m_iRetentionHours (older than that are
//! dropped on every append and load).
//! PERSISTENCE: the "incidents" payload (save version 13). Save and load go through ExportAll /
//! ApplyAll; ApplyAll replaces the list, so a second load converges.
//------------------------------------------------------------------------------------------------
class LF_IncidentLog
{
	static const string FEATURE = "Incident";

	protected static ref array<ref LF_IncidentRecord> s_aRecords;
	protected static int s_iLastNumber;
	protected static bool s_bInstalled;
	protected static ref LF_IncidentKilledSubscriber s_Killed;
	protected static ref LF_IncidentDamagedSubscriber s_Damaged;
	protected static ref LF_IncidentMoneySubscriber s_Money;
	protected static ref LF_IncidentDownedSubscriber s_Downed;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_IncidentRecord> Records()
	{
		if (!s_aRecords)
			s_aRecords = new array<ref LF_IncidentRecord>();

		return s_aRecords;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Killed)
			LF_EventBus.Unsubscribe(LF_OnPlayerKilled, s_Killed);
		if (s_Damaged)
			LF_EventBus.Unsubscribe(LF_OnPlayerDamaged, s_Damaged);
		if (s_Money)
			LF_EventBus.Unsubscribe(LF_OnMoneyMoved, s_Money);
		if (s_Downed)
			LF_EventBus.Unsubscribe(LF_OnPlayerDowned, s_Downed);

		s_aRecords = null;
		s_iLastNumber = 0;
		s_bInstalled = false;
		s_Killed = null;
		s_Damaged = null;
		s_Money = null;
		s_Downed = null;
		LF_IncidentConfig.ResetStatics();
		LF_IncidentDamage.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes the log to the bus. Safe to call twice. The bus table is cleared at world
	//! end, so the game mode calls this again each world.
	static void Install()
	{
		if (!Replication.IsServer() || s_bInstalled)
			return;

		s_Killed = new LF_IncidentKilledSubscriber();
		s_Damaged = new LF_IncidentDamagedSubscriber();
		s_Money = new LF_IncidentMoneySubscriber();
		s_Downed = new LF_IncidentDownedSubscriber();
		LF_EventBus.Subscribe(LF_OnPlayerKilled, s_Killed);
		LF_EventBus.Subscribe(LF_OnPlayerDamaged, s_Damaged);
		LF_EventBus.Subscribe(LF_OnMoneyMoved, s_Money);
		LF_EventBus.Subscribe(LF_OnPlayerDowned, s_Downed);
		s_bInstalled = true;
		LF_Debug.Info(FEATURE, string.Format("incident log installed (%1 record(s) loaded)", Records().Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: records to keep. Drops anything older than retentionSeconds before now, then the
	//! oldest beyond maxRecords. Records are in append order, oldest first.
	//! \return How many were dropped.
	static int Trim(notnull array<ref LF_IncidentRecord> records, int now, int retentionSeconds, int maxRecords)
	{
		int dropped = 0;
		while (records.Count() > 0 && (records[0].m_iTime < now - retentionSeconds || records.Count() > maxRecords))
		{
			records.RemoveOrdered(0);
			dropped++;
		}

		return dropped;
	}

	//------------------------------------------------------------------------------------------------
	//! Appends one record when its kind is enabled, then trims. Time is stamped by the caller so a
	//! test controls it.
	//! \return The stored record, or null when the kind is switched off.
	static LF_IncidentRecord Append(string kind, int time, string subjectUid, string otherUid, vector position, int amount, string how, bool murder)
	{
		if (!LF_IncidentConfig.IsEnabled(kind))
			return null;

		LF_IncidentRecord record = new LF_IncidentRecord();
		s_iLastNumber++;
		record.m_iNumber = s_iLastNumber;
		record.m_sKind = kind;
		record.m_iTime = time;
		record.m_sSubjectUid = subjectUid;
		record.m_sOtherUid = otherUid;
		record.m_fX = position[0];
		record.m_fY = position[1];
		record.m_fZ = position[2];
		record.m_iAmount = amount;
		record.m_sHow = how;
		record.m_bMurder = murder;
		Records().Insert(record);

		int dropped = Trim(Records(), time, LF_IncidentConfig.RetentionSeconds(), LF_IncidentConfig.MaxRecords());
		LF_Debug.Info(FEATURE, string.Format("#%1 %2: %3 / %4 amount %5 how %6 at %7 murder %8", record.m_iNumber, kind, subjectUid, otherUid, amount, how, position, murder));
		if (dropped > 0)
			LF_Debug.Info(FEATURE, string.Format("trimmed %1 old record(s), %2 kept", dropped, Records().Count()));

		return record;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Records().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: does the record fall inside the radius (flat, metres) of center and the time window?
	//! A radius below zero ignores position; toTime below zero ignores the end of the window.
	static bool Matches(notnull LF_IncidentRecord record, vector center, float radius, int fromTime, int toTime, string kind)
	{
		if (!kind.IsEmpty() && record.m_sKind != kind)
			return false;

		if (record.m_iTime < fromTime)
			return false;

		if (toTime >= 0 && record.m_iTime > toTime)
			return false;

		if (radius >= 0)
		{
			float dx = record.m_fX - center[0];
			float dz = record.m_fZ - center[2];
			if (dx * dx + dz * dz > radius * radius)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Copies of the matching records, oldest first. Pass radius -1 for anywhere, kind "" for all.
	static array<ref LF_IncidentRecord> Query(vector center, float radius, int fromTime, int toTime, string kind)
	{
		array<ref LF_IncidentRecord> result = new array<ref LF_IncidentRecord>();
		foreach (LF_IncidentRecord record : Records())
		{
			if (Matches(record, center, radius, fromTime, toTime, kind))
				result.Insert(record.Copy());
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: copies of every record.
	static array<ref LF_IncidentRecord> ExportAll()
	{
		array<ref LF_IncidentRecord> result = new array<ref LF_IncidentRecord>();
		foreach (LF_IncidentRecord record : Records())
			result.Insert(record.Copy());

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: replaces the log with the saved records, then trims against the clock.
	//! Replacement makes a second apply end in the same state.
	static void ApplyAll(array<ref LF_IncidentRecord> records)
	{
		s_aRecords = new array<ref LF_IncidentRecord>();
		s_iLastNumber = 0;
		if (!records)
			return;

		foreach (LF_IncidentRecord record : records)
		{
			if (!record)
				continue;

			s_aRecords.Insert(record.Copy());
			if (record.m_iNumber > s_iLastNumber)
				s_iLastNumber = record.m_iNumber;
		}

		Trim(s_aRecords, System.GetUnixTime(), LF_IncidentConfig.RetentionSeconds(), LF_IncidentConfig.MaxRecords());
	}

	//------------------------------------------------------------------------------------------------
	//! Prefab name of the weapon the character holds; empty for null, unarmed, or not a character.
	static string WeaponOf(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return "";

		CharacterControllerComponent controller = chimera.GetCharacterController();
		if (!controller)
			return "";

		BaseWeaponManagerComponent weapons = controller.GetWeaponManagerComponent();
		if (!weapons)
			return "";

		BaseWeaponComponent weapon = weapons.GetCurrentWeapon();
		if (!weapon || !weapon.GetOwner())
			return "";

		EntityPrefabData data = weapon.GetOwner().GetPrefabData();
		if (!data)
			return "";

		return data.GetPrefabName();
	}

	//------------------------------------------------------------------------------------------------
	//! The connected player's body position by uid; zero when they are not online.
	static vector PositionOfUid(string uid)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players || uid.IsEmpty())
			return vector.Zero;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int id : ids)
		{
			if (LF_Utils.GetPlayerUID(id) != uid)
				continue;

			IEntity body = players.GetPlayerControlledEntity(id);
			if (body)
				return body.GetOrigin();
		}

		return vector.Zero;
	}
}

//------------------------------------------------------------------------------------------------
class LF_IncidentKilledSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerKilled killed = LF_OnPlayerKilled.Cast(busEvent);
		if (!killed)
			return false;

		LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, System.GetUnixTime(), killed.m_sVictimUid, killed.m_sKillerUid, killed.m_vPosition, 0, killed.m_sWeapon, killed.m_bMurder);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_IncidentDamagedSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerDamaged damaged = LF_OnPlayerDamaged.Cast(busEvent);
		if (!damaged)
			return false;

		LF_IncidentLog.Append(LF_IncidentConfig.KIND_DAMAGE, System.GetUnixTime(), damaged.m_sVictimUid, damaged.m_sAttackerUid, damaged.m_vPosition, Math.Round(damaged.m_fDamage), damaged.m_sWeapon, false);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Cash and bank moves. The source names the path ("atm-deposit", ...); the position is the
//! player's body now, or zero when they have just left.
class LF_IncidentMoneySubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnMoneyMoved moved = LF_OnMoneyMoved.Cast(busEvent);
		if (!moved)
			return false;

		LF_IncidentLog.Append(LF_IncidentConfig.KIND_CASH, System.GetUnixTime(), moved.m_sPlayerUid, "", LF_IncidentLog.PositionOfUid(moved.m_sPlayerUid), moved.m_iAmount, moved.m_sSource, false);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A player went down (LF_Downed): the victim, and the player who downed them when there is one.
class LF_IncidentDownedSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerDowned downed = LF_OnPlayerDowned.Cast(busEvent);
		if (!downed)
			return false;

		LF_IncidentLog.Append(LF_IncidentConfig.KIND_DOWNED, System.GetUnixTime(), downed.m_sPlayerUid, downed.m_sDownerUid, downed.m_vPosition, 0, "", false);
		return true;
	}
}
