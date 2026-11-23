//------------------------------------------------------------------------------------------------
//! What one heist stage is made of (#3414). A stage names a kind; the kind says which world
//! component the stage binds to and which LF_HeistStages gate it waits for: "door" is a
//! LF_BreachTargetComponent (gate TARGET_OPENED), "vault" a LF_BankVaultComponent (TARGET_OPENED),
//! "getaway" the radius around the LF_HeistRunComponent site (CREW_IN_AREA).
class LF_HeistKinds
{
	static const string DOOR = "door";
	static const string VAULT = "vault";
	static const string GETAWAY = "getaway";

	//------------------------------------------------------------------------------------------------
	static bool Known(string kind)
	{
		return kind == DOOR || kind == VAULT || kind == GETAWAY;
	}

	//------------------------------------------------------------------------------------------------
	//! The gate a stage of this kind waits for. An unknown kind reads as TARGET_OPENED; check Known first.
	static LF_EHeistGate Gate(string kind)
	{
		if (kind == GETAWAY)
			return LF_EHeistGate.CREW_IN_AREA;

		return LF_EHeistGate.TARGET_OPENED;
	}

	//------------------------------------------------------------------------------------------------
	//! The component class the stage binds to, or null for an unknown kind.
	static typename Component(string kind)
	{
		if (kind == DOOR)
			return LF_BreachTargetComponent;

		if (kind == VAULT)
			return LF_BankVaultComponent;

		if (kind == GETAWAY)
			return LF_HeistRunComponent;

		typename none;
		return none;
	}
}

//------------------------------------------------------------------------------------------------
//! One stage of a configured heist: its kind and the world id it binds to.
[BaseContainerProps()]
class LF_HeistStageRow
{
	[Attribute("", UIWidgets.EditBox, "Stage kind: door, vault or getaway")]
	string m_sKind;

	[Attribute("", UIWidgets.EditBox, "Door: the LF_BreachTargetComponent target id. Vault: the LF_BankVaultComponent vault id. Getaway: empty")]
	string m_sTargetId;

	[Attribute("0", UIWidgets.Auto, "Getaway only: metres from the site every crew member must reach. Zero keeps the site component's own radius", params: "0 5000")]
	float m_fRadius;

	[Attribute("0", UIWidgets.CheckBox, "Door only: the clerk inside may choose hands up instead of running")]
	bool m_bClerkHandsUp;
}

//------------------------------------------------------------------------------------------------
//! One heist job a pack ships: ordered stages, one clock and the police it needs on duty.
[BaseContainerProps()]
class LF_HeistSiteRow : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Site id, e.g. bank")]
	string m_sId;

	[Attribute("600", UIWidgets.Auto, "Seconds from the first stage opening to get away. Zero: no clock", params: "0 86400")]
	int m_iClockSeconds;

	[Attribute("0", UIWidgets.Auto, "Minimum police on duty before the job can start. Same gate as LF_RobAction", params: "0 20")]
	int m_iMinPoliceOnDuty;

	[Attribute("10800", UIWidgets.Auto, "Big-job cooldown in seconds after the job ends (LF_BigJobLock). Zero: the bank's plan default, 3 h", params: "0 86400")]
	int m_iCooldownSeconds;

	[Attribute("", UIWidgets.Object, "The stages in order")]
	ref array<ref LF_HeistStageRow> m_aStages;

	//------------------------------------------------------------------------------------------------
	static LF_HeistSiteRow Make(string id)
	{
		LF_HeistSiteRow row = new LF_HeistSiteRow();
		row.m_sId = id;
		row.m_aStages = new array<ref LF_HeistStageRow>();
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! The first stage of this kind, or null.
	LF_HeistStageRow FindStage(string kind)
	{
		if (!m_aStages)
			return null;

		foreach (LF_HeistStageRow stage : m_aStages)
		{
			if (stage && stage.m_sKind == kind)
				return stage;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Why the row cannot run (no stages, an unknown kind, a door or vault with no id), or empty.
	string Problem()
	{
		if (!m_aStages || m_aStages.IsEmpty())
			return string.Format("heist site '%1' has no stages", m_sId);

		foreach (int i, LF_HeistStageRow stage : m_aStages)
		{
			if (!stage || !LF_HeistKinds.Known(stage.m_sKind))
				return string.Format("heist site '%1' stage %2 has an unknown kind", m_sId, i);

			if (stage.m_sKind != LF_HeistKinds.GETAWAY && stage.m_sTargetId.IsEmpty())
				return string.Format("heist site '%1' stage %2 (%3) names no target id", m_sId, i, stage.m_sKind);
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The stage list as an LF_HeistStages, one gate per row in order. Null (logged) when Problem() says so.
	LF_HeistStages BuildStages()
	{
		string problem = Problem();
		if (!problem.IsEmpty())
		{
			LF_Debug.Error(LF_HeistConfig.FEATURE, problem);
			return null;
		}

		LF_HeistStages stages = new LF_HeistStages();
		foreach (LF_HeistStageRow stage : m_aStages)
		{
			stages.AddStage(LF_HeistKinds.Gate(stage.m_sKind), 0);
		}

		return stages;
	}
}

//------------------------------------------------------------------------------------------------
//! The configured heist jobs: Configs/Crime/LF_Heist.conf (core ships none) with each pack overlay
//! layered over it (LF_ConfigOverlay). A pack adds a job by site id. Core knows how a heist runs; the
//! pack says where and for how long.
[BaseContainerProps(configRoot: true)]
class LF_HeistConfig
{
	static const string FEATURE = "Heist";
	static const ResourceName CONFIG_PATH = "{5C2B8E4D91A7F360}Configs/Crime/LF_Heist.conf";

	[Attribute("", UIWidgets.Object, "One row per heist job")]
	ref array<ref LF_HeistSiteRow> m_aSites;

	protected static ref LF_HeistConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered table, or null (logged once) when the core file does not resolve.
	static LF_HeistConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("heist config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_HeistConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(FEATURE, string.Format("heist config has an unexpected root class: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached table so the next Get() loads and layers it again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of this class, in load order, over this instance.
	void ApplyLayers()
	{
		if (!m_aSites)
			m_aSites = new array<ref LF_HeistSiteRow>();

		LF_HeistSiteList list = new LF_HeistSiteList(this);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_HeistConfig, "heist", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aSites", names[i], claims);
			LF_Debug.Info(FEATURE, string.Format("pack overlay %1 changed %2 heist sites", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The site with this id, or null.
	LF_HeistSiteRow Find(string id)
	{
		if (!m_aSites || id.IsEmpty())
			return null;

		foreach (LF_HeistSiteRow row : m_aSites)
		{
			if (row && row.m_sId == id)
				return row;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The heist config's `m_aSites` list for the shared merge (LF_ConfigOverlay), keyed by site id.
class LF_HeistSiteList : LF_ConfigList
{
	protected LF_HeistConfig m_Config;

	void LF_HeistSiteList(LF_HeistConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "heist.sites";
	}

	override int Count()
	{
		return m_Config.m_aSites.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aSites[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_HeistSiteRow.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a heist site id such as bank";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_HeistSiteRow.Make(id);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_HeistSiteRow row = LF_HeistSiteRow.Cast(record);
		if (index < m_Config.m_aSites.Count())
			m_Config.m_aSites.Set(index, row);
		else
			m_Config.m_aSites.Insert(row);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aSites.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_HeistSiteRow row = LF_HeistSiteRow.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_iClockSeconds") && container.Get("m_iClockSeconds", row.m_iClockSeconds))
			changed++;
		if (container.IsVariableSetDirectly("m_iMinPoliceOnDuty") && container.Get("m_iMinPoliceOnDuty", row.m_iMinPoliceOnDuty))
			changed++;

		if (container.IsVariableSetDirectly("m_aStages"))
		{
			row.m_aStages = new array<ref LF_HeistStageRow>();
			BaseContainerList stages = container.GetObjectArray("m_aStages");
			if (stages)
			{
				for (int i = 0; i < stages.Count(); i++)
				{
					BaseContainer entry = stages.Get(i);
					if (!entry)
						continue;

					LF_HeistStageRow stage = LF_HeistStageRow.Cast(BaseContainerTools.CreateInstanceFromContainer(entry));
					if (stage)
						row.m_aStages.Insert(stage);
				}
			}

			changed++;
		}

		return changed;
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		return LF_HeistSiteRow.Cast(record).Problem();
	}
}
