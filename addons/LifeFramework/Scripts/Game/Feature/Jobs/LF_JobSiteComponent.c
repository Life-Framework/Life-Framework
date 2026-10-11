//------------------------------------------------------------------------------------------------
//! Workplace, drop, taxi rank, or fuel can on DebugWorld. The entity name is
//! the contract the world test finds.
//------------------------------------------------------------------------------------------------
enum LF_EJobSite
{
	DEPOT = 0,
	DROP = 1,
	TAXI = 2,
	FUEL = 3
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Jobs", description: "A civilian job workplace or drop.")]
class LF_JobSiteComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_JobSiteComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.ComboBox, "Site kind", "", ParamEnumArray.FromEnum(LF_EJobSite))]
	protected LF_EJobSite m_eSite;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		if (!owner || owner.GetName().IsEmpty())
		{
			EL_Debug.Error("Jobs", "job site has no entity name");
			return;
		}

		EL_Debug.Log("Jobs", string.Format("site %1 kind %2", owner.GetName(), m_eSite));

		if (m_eSite == LF_EJobSite.FUEL)
		{
			Resource fuel = Resource.Load(LF_CivilianJob.FUEL_PREFAB);
			if (!fuel.IsValid())
				EL_Debug.Error("Jobs", "fuel can prefab missing: " + LF_CivilianJob.FUEL_PREFAB);
		}

		if (m_eSite == LF_EJobSite.TAXI)
		{
			Resource taxi = Resource.Load(LF_CivilianJob.TAXI_PREFAB);
			if (!taxi.IsValid())
				EL_Debug.Error("Jobs", "taxi prefab missing: " + LF_CivilianJob.TAXI_PREFAB);
		}

		if (m_eSite == LF_EJobSite.DEPOT)
		{
			Resource depot = Resource.Load(LF_CivilianJob.DEPOT_PREFAB);
			if (!depot.IsValid())
				EL_Debug.Error("Jobs", "depot prefab missing: " + LF_CivilianJob.DEPOT_PREFAB);
		}
	}

	//------------------------------------------------------------------------------------------------
	LF_EJobSite GetSite()
	{
		return m_eSite;
	}
}
