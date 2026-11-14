//------------------------------------------------------------------------------------------------
//! World cleanup (#690): a server sweep that removes abandoned dropped items, vehicles and bodies.
//! Rules per kind come from the config service (Configs/World/LF_CleanupConfig.conf, then the
//! owner section "cleanup"): age since a player was last near, distance to the nearest player,
//! damage state and an exempt list. An owned vehicle (a saved vehicle record), a named mission
//! entity and anything registered with Exempt() is never touched. Each sweep logs what it removed.
//------------------------------------------------------------------------------------------------
enum LF_ECleanupKind
{
	NONE,
	ITEM,
	VEHICLE,
	BODY
}

//------------------------------------------------------------------------------------------------
//! One kind's rule.
[BaseContainerProps()]
class LF_CleanupRule
{
	[Attribute("", UIWidgets.EditBox, "Kind this rule is for: item, vehicle or body")]
	string m_sKind;

	[Attribute("1", UIWidgets.CheckBox, "Sweep this kind at all")]
	bool m_bEnabled;

	[Attribute("0", UIWidgets.Auto, "Seconds an entity must have gone without a player near it before it goes", params: "0 604800")]
	int m_iMaxAgeSeconds;

	[Attribute("0", UIWidgets.Auto, "Metres to the nearest player; a closer entity is kept and its age starts over", params: "0 100000")]
	int m_iMinPlayerDistance;

	[Attribute("100", UIWidgets.Auto, "Health percent at or below which an entity may go. 100 sweeps any damage state", params: "0 100")]
	int m_iMaxHealthPercent;

	[Attribute("", UIWidgets.Auto, "Prefab name parts that are never swept")]
	ref array<string> m_aExemptPrefabs;

	//------------------------------------------------------------------------------------------------
	static LF_CleanupRule Make(string kind, int maxAge, int minDistance, int maxHealth)
	{
		LF_CleanupRule rule = new LF_CleanupRule();
		rule.m_sKind = kind;
		rule.m_bEnabled = true;
		rule.m_iMaxAgeSeconds = maxAge;
		rule.m_iMinPlayerDistance = minDistance;
		rule.m_iMaxHealthPercent = maxHealth;
		return rule;
	}

	//------------------------------------------------------------------------------------------------
	//! The rule's verdict from measured facts. Empty string: the entity goes. Otherwise the reason
	//! it stays. Pure: no world.
	string Verdict(int ageSeconds, float playerDistance, int healthPercent, string prefab)
	{
		if (!m_bEnabled)
			return "kind off";

		if (m_aExemptPrefabs)
		{
			foreach (string part : m_aExemptPrefabs)
			{
				if (!part.IsEmpty() && prefab.Contains(part))
					return "exempt prefab";
			}
		}

		if (playerDistance < m_iMinPlayerDistance)
			return "player near";

		if (ageSeconds < m_iMaxAgeSeconds)
			return "too young";

		if (healthPercent > m_iMaxHealthPercent)
			return "damage state";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! The cleanup config: Configs/World/LF_CleanupConfig.conf with the owner's cleanup section over it.
[BaseContainerProps(configRoot: true)]
class LF_CleanupConfig
{
	static const ResourceName CONFIG_PATH = "{4C8E2B7A91D35F60}Configs/World/LF_CleanupConfig.conf";
	static const string SECTION = "cleanup";

	static const int INTERVAL_MAX = 86400;	// lf-fixed: the schema limit, one day
	static const int AGE_MAX = 604800;	// lf-fixed: the schema limit, one week
	static const int DISTANCE_MAX = 100000;	// lf-fixed: the schema limit, metres
	static const int PERCENT_MAX = 100;	// lf-fixed: the schema limit

	[Attribute("300", UIWidgets.Auto, "Seconds between sweeps. 0 turns the sweep off", params: "0 86400")]
	int m_iSweepIntervalSeconds;

	[Attribute("", UIWidgets.Object, "One rule per kind")]
	ref array<ref LF_CleanupRule> m_aRules;

	protected static ref LF_CleanupConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static LF_CleanupConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_Cleanup.FEATURE, string.Format("cleanup config does not resolve, nothing is swept: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_CleanupConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(LF_Cleanup.FEATURE, string.Format("cleanup config has an unexpected root class: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	LF_CleanupRule Find(string kind)
	{
		if (!m_aRules)
			return null;

		foreach (LF_CleanupRule rule : m_aRules)
		{
			if (rule && rule.m_sKind == kind)
				return rule;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	void ApplyLayers()
	{
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("cleanup: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! rules is an object keyed by kind (item, vehicle, body).
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"sweepIntervalSeconds", "rules"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadInt("sweepIntervalSeconds", m_iSweepIntervalSeconds, 0, INTERVAL_MAX);

		if (!m_aRules)
			m_aRules = new array<ref LF_CleanupRule>();

		int count = reader.BeginRecords("rules");
		for (int i = 0; i < count; i++)
		{
			string key = reader.GetRecordKey(i);
			if (!reader.BeginRecord(key))
				continue;

			if (key != "item" && key != "vehicle" && key != "body")
			{
				reader.RejectRecord("the key is not a kind the sweep knows: item, vehicle or body");
				reader.EndRecord();
				continue;
			}

			LF_CleanupRule rule = Find(key);
			if (!rule)
			{
				rule = LF_CleanupRule.Make(key, 0, 0, PERCENT_MAX);
				m_aRules.Insert(rule);
			}

			array<string> recordKeys = {"enabled", "maxAgeSeconds", "minPlayerDistance", "maxHealthPercent", "exemptPrefabs"};
			reader.WarnUnknownKeys(recordKeys);
			reader.ReadBool("enabled", rule.m_bEnabled);
			reader.ReadInt("maxAgeSeconds", rule.m_iMaxAgeSeconds, 0, AGE_MAX);
			reader.ReadInt("minPlayerDistance", rule.m_iMinPlayerDistance, 0, DISTANCE_MAX);
			reader.ReadInt("maxHealthPercent", rule.m_iMaxHealthPercent, 0, PERCENT_MAX);
			if (!rule.m_aExemptPrefabs)
				rule.m_aExemptPrefabs = new array<string>();
			reader.ReadStrings("exemptPrefabs", rule.m_aExemptPrefabs);
			reader.EndRecord();
		}

		if (count >= 0)
			reader.EndRecords();
	}
}

//------------------------------------------------------------------------------------------------
class LF_Cleanup
{
	static const string FEATURE = "Cleanup";

	protected static ref map<string, int> s_mLastNearMs;
	protected static ref set<string> s_mExempt;
	protected static bool s_bRunning;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mLastNearMs = null;
		s_mExempt = null;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(SweepWorld);
		s_bRunning = false;
		LF_CleanupConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	//! Server. Starts the periodic sweep from the game mode. Does nothing when the system is off or
	//! the interval is 0.
	static void Start()
	{
		if (!Replication.IsServer() || s_bRunning)
			return;

		if (!LF_Systems.IsEnabled("World"))
			return;

		LF_CleanupConfig config = LF_CleanupConfig.Get();
		if (!config || config.m_iSweepIntervalSeconds <= 0)
		{
			LF_Debug.Info(FEATURE, "sweep not started: no config or interval 0");
			return;
		}

		s_bRunning = true;
		GetGame().GetCallqueue().CallLater(SweepWorld, config.m_iSweepIntervalSeconds * 1000, true);
		LF_Debug.Info(FEATURE, string.Format("sweep started, every %1 s", config.m_iSweepIntervalSeconds));
	}

	//------------------------------------------------------------------------------------------------
	//! Mission code calls this for an entity the sweep must never remove.
	static void Exempt(IEntity entity)
	{
		if (!entity)
			return;

		if (!s_mExempt)
			s_mExempt = new set<string>();

		s_mExempt.Insert(entity.GetID().ToString());
	}

	//------------------------------------------------------------------------------------------------
	protected static void SweepWorld()
	{
		if (!LF_Systems.IsEnabled("World"))
			return;

		array<vector> players = {};
		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
		{
			array<int> ids = {};
			pm.GetPlayers(ids);
			foreach (int id : ids)
			{
				IEntity controlled = pm.GetPlayerControlledEntity(id);
				if (controlled)
					players.Insert(controlled.GetOrigin());
			}
		}

		Sweep(LF_WorldResetScan.All(), players, System.GetTickCount());
	}

	//------------------------------------------------------------------------------------------------
	//! One sweep over the given entities. players: player positions. nowMs: tick count.
	//! \return How many entities were removed. Logs each removal and one summary line.
	static int Sweep(notnull array<IEntity> entities, notnull array<vector> players, int nowMs)
	{
		LF_CleanupConfig config = LF_CleanupConfig.Get();
		if (!config)
			return 0;

		if (!s_mLastNearMs)
			s_mLastNearMs = new map<string, int>();

		array<IEntity> doomed = {};
		int kept;
		foreach (IEntity entity : entities)
		{
			LF_ECleanupKind kind = KindOf(entity);
			if (kind == LF_ECleanupKind.NONE)
				continue;

			LF_CleanupRule rule = config.Find(KindName(kind));
			if (!rule)
				continue;

			string stay = ProtectedReason(entity);
			if (stay.IsEmpty())
			{
				string key = entity.GetID().ToString();
				float distance = NearestPlayer(entity.GetOrigin(), players);
				if (!s_mLastNearMs.Contains(key) || distance < rule.m_iMinPlayerDistance)
					s_mLastNearMs.Set(key, nowMs);

				int ageSeconds = (nowMs - s_mLastNearMs.Get(key)) / 1000;
				stay = rule.Verdict(ageSeconds, distance, HealthPercent(entity), entity.GetPrefabData().GetPrefabName());
			}

			if (stay.IsEmpty())
				doomed.Insert(entity);
			else
				kept++;
		}

		int removed;
		foreach (IEntity gone : doomed)
		{
			LF_Debug.Info(FEATURE, string.Format("removed %1 %2 at %3", KindName(KindOf(gone)), gone.GetPrefabData().GetPrefabName(), gone.GetOrigin()));
			s_mLastNearMs.Remove(gone.GetID().ToString());
			SCR_EntityHelper.DeleteEntityAndChildren(gone);
			removed++;
		}

		if (removed > 0)
			LF_Debug.Info(FEATURE, string.Format("sweep removed %1, kept %2", removed, kept));

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! Why an entity is never swept whatever the rule says, or an empty string.
	static string ProtectedReason(IEntity entity)
	{
		if (s_mExempt && s_mExempt.Contains(entity.GetID().ToString()))
			return "registered exempt";

		if (!entity.GetName().IsEmpty())
			return "named mission entity";

		if (entity.IsInherited(Vehicle) && LF_VehicleOwnership.IsOwnedEntity(entity))
			return "owned vehicle";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	static LF_ECleanupKind KindOf(IEntity entity)
	{
		if (!entity || !entity.GetPrefabData())
			return LF_ECleanupKind.NONE;

		if (entity.IsInherited(Vehicle))
			return LF_ECleanupKind.VEHICLE;

		if (entity.IsInherited(ChimeraCharacter))
		{
			if (!LF_StorageSnapshot.IsAlive(entity) && !IsPlayerBody(entity))
				return LF_ECleanupKind.BODY;

			return LF_ECleanupKind.NONE;
		}

		InventoryItemComponent item = InventoryItemComponent.Cast(entity.FindComponent(InventoryItemComponent));
		if (item && !item.GetParentSlot() && !entity.GetParent())
			return LF_ECleanupKind.ITEM;

		return LF_ECleanupKind.NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsPlayerBody(IEntity character)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		return pm && pm.GetPlayerIdFromControlledEntity(character) > 0;
	}

	//------------------------------------------------------------------------------------------------
	static string KindName(LF_ECleanupKind kind)
	{
		if (kind == LF_ECleanupKind.ITEM)
			return "item";
		if (kind == LF_ECleanupKind.VEHICLE)
			return "vehicle";
		if (kind == LF_ECleanupKind.BODY)
			return "body";

		return "none";
	}

	//------------------------------------------------------------------------------------------------
	static float NearestPlayer(vector origin, notnull array<vector> players)
	{
		float best = float.MAX;
		foreach (vector p : players)
			best = Math.Min(best, vector.Distance(origin, p));

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Health 0..100; full when the entity has no damage manager (nothing to compare).
	static int HealthPercent(IEntity entity)
	{
		DamageManagerComponent damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
		if (!damage)
			return LF_CleanupConfig.PERCENT_MAX;

		return Math.Round(damage.GetHealthScaled() * LF_CleanupConfig.PERCENT_MAX);
	}
}
