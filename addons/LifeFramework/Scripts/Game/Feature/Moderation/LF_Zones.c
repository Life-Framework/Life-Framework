//------------------------------------------------------------------------------------------------
//! The zone service (#242, epic #145 Moderation; grew out of the M1 spawn safe zone, #125).
//! Places on the map carry a rule set: SAFE (players cannot hurt each other), LAWLESS (the server
//! rules say so; wanted stays on unless config says otherwise) or NORMAL (everywhere else).
//! Every system asks LF_Zones.RuleAt(pos) instead of testing a hard-coded position.
//!
//! A zone is a world entity with LF_ZoneComponent: a shape (circle, box, polyline corridor) and a
//! rule set. Overlaps resolve strictest first: SAFE, then LAWLESS, then NORMAL. Shapes are flat
//! (XZ), so a roof or a balcony counts. What each rule set does is in Configs/Moderation/LF_Zones.conf.
//!
//! SAFE refuses player-on-player damage in the vanilla hook
//! SCR_CharacterDamageManagerComponent.HijackDamageHandling: returning true there tells the engine
//! the hit was handled, so no health, bleeding or ragdoll is applied. A hit is refused when the
//! attacker is a player, the victim is a different player, and either stands inside a SAFE zone.
//! NPC, fall, fire and self damage pass through. On-duty police get no exemption: arrest and
//! escort are interact actions (LF_JailArrestAction). The in-world cue is the "safe_zone" sign.
//!
//! Publishes LF_OnZoneChanged when a zone is added, changed or removed. The incident log stamps
//! RuleName(RuleAt(pos)) on each record.
//------------------------------------------------------------------------------------------------
enum LF_ZoneRule
{
	NORMAL,
	SAFE,
	LAWLESS
}

enum LF_ZoneShape
{
	CIRCLE,
	BOX,
	POLYLINE
}

//------------------------------------------------------------------------------------------------
//! One rule's behaviour (#755). A zone carries a rule id; this table says what the rule means.
//! The defaults of the new fields are "nothing changes": a rule set that only sets the two old
//! flags behaves as before. Only blocksPlayerDamage and suppressesWanted have consumers today;
//! the rest are read by the crime features as they adopt them (RuleSetAt is the one answer).
[BaseContainerProps()]
class LF_ZoneRuleSet
{
	[Attribute("0", UIWidgets.ComboBox, "Rule this entry configures. Values are stable ids", "", ParamEnumArray.FromEnum(LF_ZoneRule))]
	LF_ZoneRule m_eRule;

	[Attribute("0", UIWidgets.CheckBox, "Player-on-player damage is refused when either side stands inside")]
	bool m_bBlocksPlayerDamage;

	[Attribute("0", UIWidgets.CheckBox, "A murder inside raises no wanted level")]
	bool m_bSuppressesWanted;

	[Attribute("1", UIWidgets.CheckBox, "Robbery is allowed inside")]
	bool m_bAllowsRobbery = true;

	[Attribute("1", UIWidgets.CheckBox, "Vehicle theft is allowed inside")]
	bool m_bAllowsVehicleTheft = true;

	[Attribute("1", UIWidgets.CheckBox, "Drawing a weapon is allowed inside")]
	bool m_bAllowsWeaponDraw = true;

	[Attribute("1", UIWidgets.EditBox, "Multiplier on fines for crimes committed inside")]
	float m_fFineMultiplier = 1;

	[Attribute("1", UIWidgets.CheckBox, "Police may enter and act inside")]
	bool m_bCopsEnter = true;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_ZoneConfig
{
	static const ResourceName CONFIG_PATH = "{7C41D2A95E0B38F6}Configs/Moderation/LF_Zones.conf";

	protected static ref LF_ZoneConfig s_Config;
	protected static bool s_bLoadAttempted;

	//! Per-rule table. A rule with no entry falls back to the two legacy flags below.
	[Attribute("", UIWidgets.Object, "One entry per rule: what that rule does")]
	ref array<ref LF_ZoneRuleSet> m_aRuleSets;

	//! Legacy flag (before #755). Used only for SAFE when m_aRuleSets has no SAFE entry.
	[Attribute("1", UIWidgets.CheckBox, "Legacy: SAFE zones refuse player-on-player damage. Ignored once the rule table has a SAFE entry")]
	bool m_bSafeBlocksPlayerDamage = true;

	//! Legacy flag (before #755). Used only for LAWLESS when m_aRuleSets has no LAWLESS entry.
	[Attribute("0", UIWidgets.CheckBox, "Legacy: a murder inside a LAWLESS zone raises no wanted level. Ignored once the rule table has a LAWLESS entry")]
	bool m_bLawlessSuppressesWanted;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Null when the file is bad; callers then use the defaults (safe blocks, lawless keeps wanted).
	static LF_ZoneConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_Zones.FEATURE, string.Format("zone config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_ZoneConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(LF_Zones.FEATURE, string.Format("zone config has an unexpected root class, using defaults: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the rule set for a rule from one config object (null means the file is bad). A table
	//! entry wins; without one, the legacy flags build the answer. The result is a fresh object.
	static LF_ZoneRuleSet Resolve(LF_ZoneConfig config, LF_ZoneRule rule)
	{
		if (config && config.m_aRuleSets)
		{
			foreach (LF_ZoneRuleSet entry : config.m_aRuleSets)
			{
				if (entry && entry.m_eRule == rule)
					return entry;
			}
		}

		LF_ZoneRuleSet legacy = new LF_ZoneRuleSet();
		legacy.m_eRule = rule;
		if (rule == LF_ZoneRule.SAFE)
		{
			legacy.m_bBlocksPlayerDamage = true;
			if (config)
				legacy.m_bBlocksPlayerDamage = config.m_bSafeBlocksPlayerDamage;
		}
		else if (rule == LF_ZoneRule.LAWLESS && config)
		{
			legacy.m_bSuppressesWanted = config.m_bLawlessSuppressesWanted;
		}

		return legacy;
	}

	//------------------------------------------------------------------------------------------------
	static LF_ZoneRuleSet RuleSetFor(LF_ZoneRule rule)
	{
		return Resolve(Get(), rule);
	}
}

//------------------------------------------------------------------------------------------------
//! One zone's data. Polyline points are world positions; m_fRadius is the circle radius or the
//! corridor half-width. A box is m_vHalfExtents (x, z) turned by m_fYaw degrees around m_vCenter.
class LF_ZoneArea : Managed
{
	string m_sName;
	LF_ZoneRule m_eRule;
	LF_ZoneShape m_eShape;
	vector m_vCenter;
	float m_fRadius;
	float m_fYaw;
	vector m_vHalfExtents;
	ref array<vector> m_aPoints = {};

	//------------------------------------------------------------------------------------------------
	//! Pure, flat (XZ) containment.
	bool Contains(vector position)
	{
		if (m_eShape == LF_ZoneShape.CIRCLE)
			return vector.DistanceSqXZ(position, m_vCenter) <= m_fRadius * m_fRadius;

		if (m_eShape == LF_ZoneShape.BOX)
		{
			float dx = position[0] - m_vCenter[0];
			float dz = position[2] - m_vCenter[2];
			float rad = -m_fYaw * Math.DEG2RAD;
			float c = Math.Cos(rad);
			float s = Math.Sin(rad);
			float lx = dx * c - dz * s;
			float lz = dx * s + dz * c;
			return Math.AbsFloat(lx) <= m_vHalfExtents[0] && Math.AbsFloat(lz) <= m_vHalfExtents[2];
		}

		// Polyline corridor: within m_fRadius of any segment (one point is a circle).
		if (m_aPoints.Count() == 1)
			return vector.DistanceSqXZ(position, m_aPoints[0]) <= m_fRadius * m_fRadius;

		for (int i = 0; i < m_aPoints.Count() - 1; i++)
		{
			if (DistanceSqToSegment(position, m_aPoints[i], m_aPoints[i + 1]) <= m_fRadius * m_fRadius)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static float DistanceSqToSegment(vector p, vector a, vector b)
	{
		float abx = b[0] - a[0];
		float abz = b[2] - a[2];
		float lenSq = abx * abx + abz * abz;
		float t;
		if (lenSq > 0)
			t = Math.Clamp(((p[0] - a[0]) * abx + (p[2] - a[2]) * abz) / lenSq, 0, 1);

		float cx = a[0] + abx * t - p[0];
		float cz = a[2] + abz * t - p[2];
		return cx * cx + cz * cz;
	}
}

//------------------------------------------------------------------------------------------------
class LF_Zones
{
	static const string FEATURE = "Zones";

	protected static ref array<ref LF_ZoneArea> s_aZones;
	protected static ref map<int, float> s_mLastNotice;
	protected static int s_iRefused;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_ZoneArea> Zones()
	{
		if (!s_aZones)
			s_aZones = new array<ref LF_ZoneArea>();

		return s_aZones;
	}

	//------------------------------------------------------------------------------------------------
	static string RuleName(LF_ZoneRule rule)
	{
		if (rule == LF_ZoneRule.SAFE)
			return "safe";

		if (rule == LF_ZoneRule.LAWLESS)
			return "lawless";

		return "normal";
	}

	//------------------------------------------------------------------------------------------------
	//! Adds or replaces a zone by name. An empty name, a non-positive radius (circle, corridor), a
	//! non-positive box extent, or a polyline with no points is refused and logged.
	static bool Register(LF_ZoneArea area)
	{
		if (!area || area.m_sName.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "zone refused: empty name");
			return false;
		}

		string problem;
		if (area.m_eShape == LF_ZoneShape.BOX)
		{
			if (area.m_vHalfExtents[0] <= 0 || area.m_vHalfExtents[2] <= 0)
				problem = "box extents must be positive";
		}
		else if (area.m_fRadius <= 0)
		{
			problem = string.Format("radius %1", area.m_fRadius);
		}
		else if (area.m_eShape == LF_ZoneShape.POLYLINE && area.m_aPoints.IsEmpty())
		{
			problem = "polyline has no points";
		}

		if (!problem.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("zone %1 refused: %2", area.m_sName, problem));
			return false;
		}

		LF_ZoneArea old = Find(area.m_sName);
		if (old)
			Zones().RemoveItem(old);

		Zones().Insert(area);
		LF_Debug.Info(FEATURE, string.Format("zone %1 is %2: shape %3 at %4 radius %5 m", area.m_sName, RuleName(area.m_eRule), typename.EnumToString(LF_ZoneShape, area.m_eShape), area.m_vCenter, area.m_fRadius));
		Publish(area.m_sName, area.m_eRule, false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Circle shortcut for code and tests.
	static bool RegisterCircle(string name, LF_ZoneRule rule, vector center, float radius)
	{
		LF_ZoneArea area = new LF_ZoneArea();
		area.m_sName = name;
		area.m_eRule = rule;
		area.m_eShape = LF_ZoneShape.CIRCLE;
		area.m_vCenter = center;
		area.m_fRadius = radius;
		return Register(area);
	}

	//------------------------------------------------------------------------------------------------
	static void Unregister(string name)
	{
		LF_ZoneArea area = Find(name);
		if (!area)
			return;

		Zones().RemoveItem(area);
		LF_Debug.Info(FEATURE, string.Format("zone %1 removed (was %2)", name, RuleName(area.m_eRule)));
		Publish(name, area.m_eRule, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Publish(string name, LF_ZoneRule rule, bool removed)
	{
		if (!Replication.IsServer())
			return;

		LF_EventBus.Publish(LF_OnZoneChanged.Create(name, RuleName(rule), removed));
	}

	//------------------------------------------------------------------------------------------------
	static LF_ZoneArea Find(string name)
	{
		foreach (LF_ZoneArea area : Zones())
		{
			if (area.m_sName == name)
				return area;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The first registered zone with this rule (the first one the world placed), or null.
	static LF_ZoneArea FindFirstByRule(LF_ZoneRule rule)
	{
		foreach (LF_ZoneArea area : Zones())
		{
			if (area.m_eRule == rule)
				return area;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The rule set at a position. Overlaps resolve strictest first: SAFE, LAWLESS, NORMAL.
	static LF_ZoneRule RuleAt(vector position)
	{
		LF_ZoneRule result = LF_ZoneRule.NORMAL;
		foreach (LF_ZoneArea area : Zones())
		{
			if (area.m_eRule == LF_ZoneRule.NORMAL || !area.Contains(position))
				continue;

			if (area.m_eRule == LF_ZoneRule.SAFE)
				return LF_ZoneRule.SAFE;

			result = area.m_eRule;
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsSafe(vector position)
	{
		return RuleAt(position) == LF_ZoneRule.SAFE;
	}

	//------------------------------------------------------------------------------------------------
	//! True when a crime at this position raises no wanted level: LAWLESS and config says so.
	static bool WantedSuppressedAt(vector position)
	{
		return RuleSetAt(position).m_bSuppressesWanted;
	}

	//------------------------------------------------------------------------------------------------
	//! The rule set in force at a position (the table entry for RuleAt). Never null.
	static LF_ZoneRuleSet RuleSetAt(vector position)
	{
		return LF_ZoneConfig.RuleSetFor(RuleAt(position));
	}

	//------------------------------------------------------------------------------------------------
	static bool IsLawless(vector position)
	{
		return RuleAt(position) == LF_ZoneRule.LAWLESS;
	}

	//------------------------------------------------------------------------------------------------
	//! True when player-on-player damage is refused for someone standing here.
	static bool BlocksPlayerDamageAt(vector position)
	{
		return RuleSetAt(position).m_bBlocksPlayerDamage;
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
		bool victimInside = BlocksPlayerDamageAt(victim.GetOrigin());
		bool attackerInside;
		IEntity attacker = instigator.GetInstigatorEntity();
		if (attacker)
			attackerInside = BlocksPlayerDamageAt(attacker.GetOrigin());

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
		if (s_mLastNotice.Find(attackerId, last) && now - last < LF_IncidentConfig.ZoneNoticeSeconds())
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
[ComponentEditorProps(category: "LifeFramework/Moderation", description: "A zone with a rule set (safe, lawless, normal) and a circle, box or polyline shape")]
class LF_ZoneComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ZoneComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.ComboBox, "Rule set inside the zone", "", ParamEnumArray.FromEnum(LF_ZoneRule))]
	protected LF_ZoneRule m_eRule;

	[Attribute("0", UIWidgets.ComboBox, "Shape: circle, box (turned by this entity's yaw), polyline (a corridor)", "", ParamEnumArray.FromEnum(LF_ZoneShape))]
	protected LF_ZoneShape m_eShape;

	[Attribute("30", UIWidgets.Auto, "Circle radius, or polyline corridor half-width, in metres, measured flat")]
	protected float m_fRadius;

	[Attribute("20 0 20", UIWidgets.Auto, "Box half extents in metres (x and z), along and across this entity's yaw")]
	protected vector m_vHalfExtents;

	[Attribute("", UIWidgets.Auto, "Polyline points, local to this entity (x and z are used)")]
	protected ref array<vector> m_aPoints;

	protected string m_sZoneName;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_sZoneName = owner.GetName();
		if (m_sZoneName.IsEmpty())
			m_sZoneName = string.Format("zone@%1", owner.GetOrigin());

		LF_ZoneArea area = new LF_ZoneArea();
		area.m_sName = m_sZoneName;
		area.m_eRule = m_eRule;
		area.m_eShape = m_eShape;
		area.m_vCenter = owner.GetOrigin();
		area.m_fRadius = m_fRadius;
		area.m_fYaw = owner.GetAngles()[1];
		area.m_vHalfExtents = m_vHalfExtents;
		if (m_aPoints)
		{
			foreach (vector point : m_aPoints)
				area.m_aPoints.Insert(owner.CoordToParent(point));
		}

		LF_Zones.Register(area);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (!m_sZoneName.IsEmpty())
			LF_Zones.Unregister(m_sZoneName);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	float GetRadius()
	{
		return m_fRadius;
	}

	//------------------------------------------------------------------------------------------------
	LF_ZoneRule GetRule()
	{
		return m_eRule;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_CharacterDamageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
	{
		if (LF_Zones.RefuseHit(GetOwner(), damageContext))
			return true;

		return super.HijackDamageHandling(damageContext);
	}
}
