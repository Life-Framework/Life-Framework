//------------------------------------------------------------------------------------------------
//! What the "dispatch and seize" scene operation did in one run (#3785), plain data for the log and
//! the tests.
enum LF_EDispatchSeizeOutcome
{
	NOT_SERVER,		//!< Not the server: nothing happens on a client.
	ALREADY_RAN,	//!< This phase run already dispatched; the operation acts once per phase run.
	NO_SITE,		//!< The scene has no placed event_target, so there is nowhere to send the call or look.
	NO_DISPATCH,	//!< The police call was refused (logged by LF_Dispatch); tried again at the next cadence.
	SEIZED,			//!< The police were called and the item was found at its place and logged as seized.
	MOVED			//!< The police were called but the item was no longer at its place: nothing seized.
}

//------------------------------------------------------------------------------------------------
//! A "dispatch and seize" module (#3785): a pack .conf with this root class becomes an operation a
//! phase names (LF_EventModuleLoader). When the phase runs it files a police call at the scene's
//! `event_target` marker (LF_Dispatch.Report), then looks for the named item lying at that place.
//! Found, it is logged in the evidence locker (LF_EvidenceLocker.LogSeizure) and taken out of the
//! world; moved (the smugglers got it out first), nothing is seized and the log says so. Data only:
//! the scene id, the prefab and the text are the pack's, the core knows no place or item name.
[BaseContainerProps()]
class LF_EventDispatchSeize
{
	[Attribute("", UIWidgets.EditBox, "Scene id whose placed event_target marker is the place: the police call goes there and the item is looked for around it")]
	string m_sSceneId;

	[Attribute("", UIWidgets.ResourceNamePicker, "The item the police look for at the place (matched by prefab GUID)", "et")]
	ResourceName m_Item;

	[Attribute("smuggling", UIWidgets.EditBox, "Offence on the police call")]
	string m_sOffence;

	[Attribute("event", UIWidgets.EditBox, "Account id the call is filed under; the event is the caller, not a player")]
	string m_sReporterUid;

	[Attribute("event", UIWidgets.EditBox, "Officer id the evidence locker row carries; no player officer made the seizure")]
	string m_sOfficerUid;

	[Attribute("", UIWidgets.EditBox, "Place name the evidence locker row carries (a localisation key or plain text from the pack)")]
	string m_sPlace;

	[Attribute("6", UIWidgets.Auto, "Metres around the marker the item still counts as at its place; moved further, it is gone", params: "0.5 100")]
	float m_fRadius;

	[Attribute("1", UIWidgets.CheckBox, "Take the seized item out of the world")]
	bool m_bRemoveItem;

	//------------------------------------------------------------------------------------------------
	//! \return an empty string when usable, else why not.
	string Validate()
	{
		if (m_sSceneId.IsEmpty())
			return "no scene id";

		if (m_Item.IsEmpty())
			return "no item prefab";

		if (m_sOffence.IsEmpty())
			return "no offence";

		if (m_sReporterUid.IsEmpty())
			return "no reporter id";

		if (m_fRadius <= 0)
			return "radius must be above 0";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The operation an LF_EventDispatchSeize config registers as. Acts once per phase run, so a
//! cadence of 5 s does not file a police call every tick; the phase's own cooldown paces repeats.
//! It is never Busy: the call and the seizure happen inside the one Run. Server only.
class LF_EventDispatchSeizeOperation : LF_EventOperation
{
	static const string FEATURE = "Events";

	string m_sModuleId;
	ref LF_EventDispatchSeize m_Config;
	//! Tests: the place to use instead of the scene's marker.
	bool m_bSiteOverride;
	vector m_vSiteOverride;
	//! The last run's outcome, for the tests.
	LF_EDispatchSeizeOutcome m_eLast;

	protected bool m_bRan;
	protected float m_fRanPhaseAt;

	//! Query scratch: what the sphere scan is looking for.
	protected ref array<IEntity> m_aMatches;
	protected vector m_vScanOrigin;

	//------------------------------------------------------------------------------------------------
	void LF_EventDispatchSeizeOperation(string moduleId, LF_EventDispatchSeize config)
	{
		m_sModuleId = moduleId;
		m_Config = config;
		m_eLast = LF_EDispatchSeizeOutcome.NOT_SERVER;
	}

	//------------------------------------------------------------------------------------------------
	override bool Run(string eventId, string phaseName, LF_EventContext ctx)
	{
		LF_EDispatchSeizeOutcome outcome = Execute(eventId, phaseName, ctx);
		m_eLast = outcome;
		return outcome == LF_EDispatchSeizeOutcome.SEIZED || outcome == LF_EDispatchSeizeOutcome.MOVED;
	}

	//------------------------------------------------------------------------------------------------
	//! The whole job of one run. Order matters: the call is filed first (the police are told whether
	//! or not the crate is still there), then the place is checked.
	LF_EDispatchSeizeOutcome Execute(string eventId, string phaseName, LF_EventContext ctx)
	{
		if (!Replication.IsServer() || !m_Config)
			return LF_EDispatchSeizeOutcome.NOT_SERVER;

		if (ctx && m_bRan && ctx.m_fPhaseStartedAt == m_fRanPhaseAt)
			return LF_EDispatchSeizeOutcome.ALREADY_RAN;

		vector site;
		if (!ResolveSite(site))
		{
			LF_Debug.Warn(FEATURE, string.Format("event %1 phase %2: module %3 has no placed event_target for scene '%4'; no call, no seizure", eventId, phaseName, m_sModuleId, m_Config.m_sSceneId));
			return LF_EDispatchSeizeOutcome.NO_SITE;
		}

		LF_DispatchRecord call = LF_Dispatch.Report(m_Config.m_sReporterUid, null, m_Config.m_sOffence, site);
		if (!call)
		{
			LF_Debug.Warn(FEATURE, string.Format("event %1 phase %2: module %3 police call refused; tried again at the next run", eventId, phaseName, m_sModuleId));
			return LF_EDispatchSeizeOutcome.NO_DISPATCH;
		}

		// The call is on the log: this phase run is spent, whatever the crate did.
		m_bRan = ctx != null;
		if (ctx)
			m_fRanPhaseAt = ctx.m_fPhaseStartedAt;

		IEntity item = FindItem(site);
		if (!item)
		{
			LF_Debug.Info(FEATURE, string.Format("event %1 phase %2: module %3 police called to scene '%4', %5 is no longer within %6 m: moved, nothing seized", eventId, phaseName, m_sModuleId, m_Config.m_sSceneId, m_Config.m_Item, m_Config.m_fRadius));
			return LF_EDispatchSeizeOutcome.MOVED;
		}

		LF_EvidenceLockerRow row = LF_EvidenceLocker.LogSeizure(m_Config.m_Item, m_Config.m_sPlace, m_Config.m_sOfficerUid);
		if (!row)
		{
			// Logging only fails off the server, which Execute has ruled out; keep the item and say so.
			LF_Debug.Error(FEATURE, string.Format("event %1: %2 found but the seizure was not logged; item left in place", eventId, m_Config.m_Item));
			return LF_EDispatchSeizeOutcome.MOVED;
		}

		if (m_Config.m_bRemoveItem)
			SCR_EntityHelper.DeleteEntityAndChildren(item);

		LF_Debug.Info(FEATURE, string.Format("event %1 phase %2: module %3 seized %4 at scene '%5' (locker row #%6)", eventId, phaseName, m_sModuleId, m_Config.m_Item, m_Config.m_sSceneId, row.m_iNumber));
		return LF_EDispatchSeizeOutcome.SEIZED;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ResolveSite(out vector site)
	{
		if (m_bSiteOverride)
		{
			site = m_vSiteOverride;
			return true;
		}

		IEntity marker = LF_EventWaypointComponent.FindTarget(m_Config.m_sSceneId);
		if (!marker)
			return false;

		site = marker.GetOrigin();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest loose item of the configured prefab within the radius of `site`. One lying in a
	//! storage (a carried crate, one in a vehicle) has a parent and does not count as at its place.
	//! \return The item, or null when it was moved.
	IEntity FindItem(vector site)
	{
		if (!GetGame() || !GetGame().GetWorld())
		{
			LF_Debug.Error(FEATURE, "seizure lookup skipped: no world");
			return null;
		}

		m_aMatches = {};
		m_vScanOrigin = site;
		GetGame().GetWorld().QueryEntitiesBySphere(site, m_Config.m_fRadius, CollectItem, null, EQueryEntitiesFlags.ALL);

		IEntity best;
		float bestDistance = m_Config.m_fRadius + 1;
		foreach (IEntity candidate : m_aMatches)
		{
			float distance = vector.Distance(candidate.GetOrigin(), site);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = candidate;
			}
		}

		m_aMatches = null;
		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectItem(IEntity entity)
	{
		if (!entity || entity.GetParent())
			return true;

		if (vector.Distance(entity.GetOrigin(), m_vScanOrigin) > m_Config.m_fRadius)
			return true;

		if (LF_Utils.SamePrefab(LF_Utils.GetPrefabName(entity), m_Config.m_Item))
			m_aMatches.Insert(entity);

		return true;
	}
}
