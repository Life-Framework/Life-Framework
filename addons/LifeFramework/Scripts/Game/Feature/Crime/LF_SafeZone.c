//------------------------------------------------------------------------------------------------
//! Spawn safe zone (M1-27, docs/decisions.md "the spawn is a safe zone").
//! Players cannot hurt each other inside a zone. The server refuses the damage in the vanilla
//! hook SCR_CharacterDamageManagerComponent.HijackDamageHandling: returning true there tells the
//! engine the hit was handled, so no health, bleeding, or ragdoll is applied. Weapons still
//! raise and fire (nothing in vanilla fights us), the bullet just does nothing to a person.
//!
//! A hit is refused when the attacker is a player, the victim is a different player, and either
//! of them stands inside a zone: you cannot be shot on the square, and you cannot shoot out of it.
//! NPC, fall, fire, and self damage pass through. On-duty police get no exemption: arrest and
//! escort are interact actions (LF_JailArrestAction), so no officer needs a gun on the square.
//!
//! A zone is a world entity with LF_SafeZoneComponent; its radius is the component attribute, so
//! a server owner sets it where the zone is placed. The flagship zone is LF_SafeZone_Square in
//! Gamemode.layer. The in-world cue is the "safe_zone" sign card at the square's edges.
//------------------------------------------------------------------------------------------------
class LF_SafeZoneArea : Managed
{
	string m_sName;
	vector m_vCenter;
	float m_fRadius;
}

//------------------------------------------------------------------------------------------------
class LF_SafeZone
{
	static const string FEATURE = "SafeZone";
	//! Seconds between two "your shot did nothing" notes to the same shooter.
	static const float NOTICE_INTERVAL = 8;

	protected static ref array<ref LF_SafeZoneArea> s_aZones;
	protected static ref map<int, float> s_mLastNotice;
	protected static int s_iRefused;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_SafeZoneArea> Zones()
	{
		if (!s_aZones)
			s_aZones = new array<ref LF_SafeZoneArea>();

		return s_aZones;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds or moves a zone. A radius of 0 or less is refused and logged.
	static bool Register(string name, vector center, float radius)
	{
		if (name.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "zone refused: empty name");
			return false;
		}

		if (radius <= 0)
		{
			LF_Debug.Error(FEATURE, string.Format("zone %1 refused: radius %2", name, radius));
			return false;
		}

		LF_SafeZoneArea area = Find(name);
		if (!area)
		{
			area = new LF_SafeZoneArea();
			area.m_sName = name;
			Zones().Insert(area);
		}

		area.m_vCenter = center;
		area.m_fRadius = radius;
		LF_Debug.Info(FEATURE, string.Format("zone %1 at %2 radius %3 m", name, center, radius));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void Unregister(string name)
	{
		for (int i = Zones().Count() - 1; i >= 0; i--)
		{
			if (Zones().Get(i).m_sName == name)
				Zones().Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	static LF_SafeZoneArea Find(string name)
	{
		foreach (LF_SafeZoneArea area : Zones())
		{
			if (area.m_sName == name)
				return area;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Flat distance: the zone is a cylinder, so a player on a roof or a balcony is still inside.
	static bool IsInside(vector position)
	{
		foreach (LF_SafeZoneArea area : Zones())
		{
			if (vector.DistanceSqXZ(position, area.m_vCenter) <= area.m_fRadius * area.m_fRadius)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure rule: is this hit refused?
	//! \param attackerPlayerId Player id behind the damage, 0 or less for NPCs and the world.
	//! \param victimPlayerId Player id of the hit body, 0 or less for an NPC.
	static bool ShouldRefuse(int attackerPlayerId, int victimPlayerId, bool victimInside, bool attackerInside)
	{
		if (attackerPlayerId <= 0 || victimPlayerId <= 0)
			return false;

		if (attackerPlayerId == victimPlayerId)
			return false;

		return victimInside || attackerInside;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from the character damage manager. True means the hit is swallowed.
	static bool RefuseHit(IEntity victim, notnull BaseDamageContext damageContext)
	{
		if (!victim || Zones().IsEmpty())
			return false;

		Instigator instigator = damageContext.instigator;
		if (!instigator)
			return false;

		int attackerId = instigator.GetInstigatorPlayerID();
		if (attackerId <= 0)
			return false;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return false;

		int victimId = playerManager.GetPlayerIdFromControlledEntity(victim);
		bool victimInside = IsInside(victim.GetOrigin());
		bool attackerInside;
		IEntity attacker = instigator.GetInstigatorEntity();
		if (attacker)
			attackerInside = IsInside(attacker.GetOrigin());

		if (!ShouldRefuse(attackerId, victimId, victimInside, attackerInside))
			return false;

		if (Replication.IsServer())
			NoteRefusal(attackerId, victimId, damageContext.damageValue, victimInside, attacker);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void NoteRefusal(int attackerId, int victimId, float damage, bool victimInside, IEntity attacker)
	{
		s_iRefused++;
		LF_Debug.Info(FEATURE, string.Format("refused %1 damage from player %2 to player %3 (victim inside=%4)", damage, attackerId, victimId, victimInside));

		if (!s_mLastNotice)
			s_mLastNotice = new map<int, float>();

		float now;
		if (GetGame() && GetGame().GetWorld())
			now = GetGame().GetWorld().GetWorldTime() / 1000;

		float last;
		if (s_mLastNotice.Find(attackerId, last) && now - last < NOTICE_INTERVAL)
			return;

		s_mLastNotice.Set(attackerId, now);
		if (attacker)
			LF_Utils.NotifyEntity(attacker, "#LF-SafeZone_ShotRefused", "#LF-SafeZone_Title", 4.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Hits refused since the server started. Tests and the harness read it.
	static int GetRefusedCount()
	{
		return s_iRefused;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Crime", description: "Spawn safe zone: players inside cannot hurt each other or be hurt by players")]
class LF_SafeZoneComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_SafeZoneComponent : ScriptComponent
{
	[Attribute("30", UIWidgets.Auto, "Radius in metres, measured flat from this entity")]
	protected float m_fRadius;

	protected string m_sZoneName;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_sZoneName = owner.GetName();
		if (m_sZoneName.IsEmpty())
			m_sZoneName = string.Format("zone@%1", owner.GetOrigin());

		LF_SafeZone.Register(m_sZoneName, owner.GetOrigin(), m_fRadius);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (!m_sZoneName.IsEmpty())
			LF_SafeZone.Unregister(m_sZoneName);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	float GetRadius()
	{
		return m_fRadius;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_CharacterDamageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
	{
		if (LF_SafeZone.RefuseHit(GetOwner(), damageContext))
			return true;

		return super.HijackDamageHandling(damageContext);
	}
}
