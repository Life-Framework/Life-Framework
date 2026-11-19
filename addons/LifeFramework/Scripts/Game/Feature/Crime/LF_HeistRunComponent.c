//------------------------------------------------------------------------------------------------
//! A heist site (#3411): names its breach door (an LF_BreachTargetComponent's target id) and its
//! vault (an LF_BankVaultComponent's vault id) and keeps one LF_HeistRun listening for them. The
//! getaway is the radius around this entity. Ticks once a second while a run is going; a finished
//! run is replaced by a fresh one so the site can be hit again after its door and vault are reset.
[ComponentEditorProps(category: "LifeFramework/Feature/Crime", description: "Heist site: door, vault drill, then get out of the area.")]
class LF_HeistRunComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_HeistRunComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Site id. Empty uses the entity name.")]
	protected string m_sSiteId;

	[Attribute("", UIWidgets.EditBox, desc: "Heist job id in LF_HeistConfig (e.g. bank). Set: the job row's door, vault, getaway radius, clock and police minimum replace the four fields below.")]
	protected string m_sHeistId;

	[Attribute("", UIWidgets.EditBox, desc: "Stage one: target id of the breach door (LF_BreachTargetComponent).")]
	protected string m_sDoorTargetId;

	[Attribute("", UIWidgets.EditBox, desc: "Stage two: vault id the drill opens (LF_BankVaultComponent).")]
	protected string m_sVaultId;

	[Attribute("40", UIWidgets.EditBox, desc: "Getaway: metres from this entity every crew member must reach.")]
	protected float m_fGetawayRadius;

	[Attribute("600", UIWidgets.EditBox, desc: "Seconds from the door opening to get away. Zero: no clock.")]
	protected float m_fClockSeconds;

	protected static const int TICK_MS = 1000; // lf-fixed: check cadence, not tuning

	protected ref LF_HeistRun m_Run;
	protected bool m_bTicking;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer() || !GetGame() || !GetGame().InPlayMode())
			return;

		NewRun();
	}

	//------------------------------------------------------------------------------------------------
	string GetSiteId()
	{
		if (!m_sSiteId.IsEmpty())
			return m_sSiteId;

		if (GetOwner())
			return GetOwner().GetName();

		return "";
	}

	//------------------------------------------------------------------------------------------------
	LF_HeistRun GetRun()
	{
		return m_Run;
	}

	//------------------------------------------------------------------------------------------------
	//! The run a site builds (#3748): with a heist id, from that LF_HeistConfig row (the radius
	//! attribute is the fallback when the row's getaway names none); without one, from the attributes.
	//! \return null (logged) when the id names no row or the row cannot run.
	static LF_HeistRun BuildRun(string siteId, string heistId, string doorId, string vaultId, vector centre, float radius, float clockSeconds)
	{
		if (heistId.IsEmpty())
			return LF_HeistRun.Create(siteId, doorId, vaultId, centre, radius, clockSeconds * 1000);

		LF_HeistSiteRow row;
		LF_HeistConfig config = LF_HeistConfig.Get();
		if (config)
			row = config.Find(heistId);

		if (!row)
		{
			LF_Debug.Error(LF_HeistRun.FEATURE, string.Format("heist site %1: no heist job '%2' in the heist config", siteId, heistId));
			return null;
		}

		return LF_HeistRun.FromRow(siteId, row, centre, radius);
	}

	//------------------------------------------------------------------------------------------------
	protected void NewRun()
	{
		if (m_Run)
			m_Run.Stop();

		m_Run = BuildRun(GetSiteId(), m_sHeistId, m_sDoorTargetId, m_sVaultId, GetOwner().GetOrigin(), m_fGetawayRadius, m_fClockSeconds);
		if (!m_Run || !m_Run.Listen())
			return;

		if (!m_bTicking)
		{
			m_bTicking = true;
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (!m_Run || m_Run.GetState() != LF_EHeistState.RUNNING)
			return;

		array<vector> positions = {};
		foreach (string uid : m_Run.GetCrew())
		{
			LF_PlayerAccount account;
			IEntity body = LF_PoliceUtils.FindPlayerEntityByUid(uid, account);
			if (body && LF_Utils.IsAliveState(body))
				positions.Insert(body.GetOrigin());
		}

		if (m_Run.Tick(LF_HeistRun.Now(), positions))
			NewRun();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_bTicking && GetGame())
			GetGame().GetCallqueue().Remove(Tick);

		if (m_Run)
			m_Run.Stop();

		super.OnDelete(owner);
	}
}
