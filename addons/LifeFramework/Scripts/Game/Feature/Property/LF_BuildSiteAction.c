//------------------------------------------------------------------------------------------------
enum LF_EBuildSiteMode
{
	DEPOSIT,
	WORK
}

//------------------------------------------------------------------------------------------------
//! The two things an owner does at their build site (#2650): DEPOSIT hands over what they carry of
//! the stage's missing items ("Need N more <item>" until met), WORK is a held action that adds one
//! unit of work once the items are in. The labels read the site's replicated mirror; the server
//! re-derives every number in LF_BuildSiteComponent.
//------------------------------------------------------------------------------------------------
class LF_BuildSiteAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "What this action does at the site", "", ParamEnumArray.FromEnum(LF_EBuildSiteMode))]
	protected LF_EBuildSiteMode m_eMode;

	protected LF_BuildSiteComponent m_Site;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Site = LF_Component<LF_BuildSiteComponent>.Find(pOwnerEntity);
		if (!m_Site)
			LF_Debug.Error(LF_BuildSiteComponent.FEATURE, "build site action on an entity with no LF_BuildSiteComponent; action hidden");

		SetCannotPerformReason("#LF-Property_BuildNoItems");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Site || !m_Site.IsOwned() || m_Site.IsComplete())
			return false;

		bool itemsMissing = !m_Site.GetMissing().IsEmpty();
		if (m_eMode == LF_EBuildSiteMode.DEPOSIT)
			return itemsMissing;

		return !itemsMissing && m_Site.GetWorkDone() < m_Site.GetWorkNeeded();
	}

	//------------------------------------------------------------------------------------------------
	//! A client check for the offer: the caller carries some of what is missing. The server checks again.
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_Site || m_eMode != LF_EBuildSiteMode.DEPOSIT)
			return true;

		array<string> prefabs = {};
		array<int> counts = {};
		LF_BuildStageRules.Parse(m_Site.GetMissing(), prefabs, counts);
		foreach (string prefab : prefabs)
		{
			if (LF_InventoryUtils.GetAmount(user, prefab) > 0)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!m_Site)
			return false;

		if (m_eMode == LF_EBuildSiteMode.WORK)
		{
			outName = WidgetManager.Translate("#LF-Property_BuildWork", m_Site.GetWorkDone(), m_Site.GetWorkNeeded());
			return true;
		}

		array<string> prefabs = {};
		array<int> counts = {};
		LF_BuildStageRules.Parse(m_Site.GetMissing(), prefabs, counts);
		if (prefabs.IsEmpty())
			return false;

		string item = WidgetManager.Translate(LF_ShopManagerComponent.ResolveDisplayName(prefabs[0]));
		outName = WidgetManager.Translate("#LF-Property_BuildNeed", counts[0], item);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!m_Site || !pUserEntity)
		{
			LF_Debug.Error(LF_BuildSiteComponent.FEATURE, "build site action performed without a site or a user");
			return;
		}

		if (!Replication.IsServer())
			return;

		string actorKey = LF_Utils.GetPlayerUID(pUserEntity);
		LF_EBuildSite result;
		if (m_eMode == LF_EBuildSiteMode.DEPOSIT)
		{
			result = m_Site.ServerDeposit(pUserEntity, actorKey);
		}
		else
		{
			float now = GetGame().GetWorld().GetWorldTime() / 1000; // lf-float: world time in seconds for the work interval
			result = m_Site.ServerWork(pUserEntity, actorKey, now);
		}

		string key = RefusalKey(result);
		if (!key.IsEmpty())
			LF_Utils.NotifyEntity(pUserEntity, key, "#LF-Property_Title", 4.0);
	}

	//------------------------------------------------------------------------------------------------
	//! The toast for a refused call; empty for OK and for refusals the player cannot act on.
	static string RefusalKey(LF_EBuildSite result)
	{
		if (result == LF_EBuildSite.REFUSED_NOT_ALLOWED)
			return "#LF-Property_BuildNotYours";

		if (result == LF_EBuildSite.REFUSED_NO_ITEMS)
			return "#LF-Property_BuildNoItems";

		if (result == LF_EBuildSite.REFUSED_ITEMS_FIRST)
			return "#LF-Property_BuildItemsFirst";

		return "";
	}
}
