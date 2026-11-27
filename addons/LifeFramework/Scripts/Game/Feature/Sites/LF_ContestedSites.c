//------------------------------------------------------------------------------------------------
//! LF_ContestedSites.c: the server sweep of contested sites (#2680, gang war #1022). Every
//! SWEEP_SECONDS it takes each site registered under LF_Places kind `contested-site`, finds the
//! living, conscious player characters inside its radius, groups them by organisation, keeps the
//! active ones (moved at least ACTIVE_MOVE_M or acted within the site's activeWindowSeconds; the
//! sweep keeps its own position samples), counts the inner ring double and runs LF_SiteHold.Step
//! with LF_SitesConfig.For(id).ToHoldRules(). The holder is the hideout's lessee and its held
//! ground (LF_Hideout, the "heldground" payload): nothing new is saved. A capture hands the hideout
//! over (LF_Hideout.HandOver: release, assign, held ground at zero) and publishes LF_OnSiteCaptured;
//! a contest start publishes LF_OnSiteContested.

//! One character the sweep saw on a site: the world gather fills it, a test feeds it directly.
class LF_SiteSighting : Managed
{
	string m_sCharacterId;
	string m_sOrgId;
	vector m_vPosition;
	//! Metres to the site centre (decides the rings).
	float m_fDistance;

	//------------------------------------------------------------------------------------------------
	static LF_SiteSighting Create(string characterId, string orgId, vector position, float distance)
	{
		LF_SiteSighting row = new LF_SiteSighting();
		row.m_sCharacterId = characterId;
		row.m_sOrgId = orgId;
		row.m_vPosition = position;
		row.m_fDistance = distance;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The sweep's last sample of one character: where it stood and when it last moved or acted.
class LF_SiteActivity : Managed
{
	vector m_vPosition;
	int m_iLastActive;
}

//------------------------------------------------------------------------------------------------
class LF_ContestedSites
{
	//! LF_Places kind of a site centre (LF_ContestedSiteComponent); the id is the hideout id.
	static const string PLACE_KIND = "contested-site";
	static const int SWEEP_MS = 5000;	// lf-fixed: sweep period, not a balance knob
	static const int SWEEP_SECONDS = 5;	// lf-fixed: SWEEP_MS in seconds, the step each sweep scores
	static const float ACTIVE_MOVE_M = 1;	// lf-fixed: movement that counts as activity

	protected static ref map<string, ref LF_SiteHoldState> s_mStates;
	protected static ref map<string, ref LF_SiteActivity> s_mActivity;
	//! Per site, organisation id to counted members of the last sweep (tests and debug read it).
	protected static ref map<string, ref map<string, int>> s_mCounted;
	protected static ref array<string> s_aContested;
	protected static bool s_bScheduled;
	protected static int s_iCaptures;
	protected static int s_iContestStarts;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Stop();
		s_mStates = null;
		s_mActivity = null;
		s_mCounted = null;
		s_aContested = null;
		s_iCaptures = 0;
		s_iContestStarts = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the sweep on the server (idempotent); the site component calls it.
	static void Start()
	{
		LF_SiteAlerts.Start();	// #2686: alert the holders (server only, idempotent)
		if (s_bScheduled || !Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bScheduled = true;
		GetGame().GetCallqueue().CallLater(Tick, SWEEP_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stop()
	{
		if (s_bScheduled && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bScheduled = false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		s_bScheduled = false;
		SweepAll(System.GetUnixTime(), SWEEP_SECONDS);
		Start();
	}

	//------------------------------------------------------------------------------------------------
	//! One pass over every registered site (server).
	static void SweepAll(int nowUnix, float elapsedSeconds)
	{
		array<IEntity> centres = {};
		LF_Places.FindAll(PLACE_KIND, centres);
		foreach (IEntity centre : centres)
		{
			LF_ContestedSiteComponent site = LF_ContestedSiteComponent.Cast(centre.FindComponent(LF_ContestedSiteComponent));
			if (!site)
				continue;

			string hideoutId = site.GetHideoutId();
			LF_SiteSettings settings = LF_SitesConfig.For(hideoutId);
			array<ref LF_SiteSighting> sightings = {};
			Gather(centre.GetOrigin(), settings.m_iRadius, sightings);
			SweepSite(hideoutId, sightings, nowUnix, elapsedSeconds);
			LF_SiteCaptureFeed.Push(hideoutId, centre.GetOrigin(), settings.m_iRadius, StateOf(hideoutId), settings.m_iCaptureSeconds);	// #2686: the capture bar
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The site's scoring state, null before its first sweep (the capture bar reads it).
	static LF_SiteHoldState StateOf(string hideoutId)
	{
		if (!s_mStates)
			return null;

		return s_mStates.Get(hideoutId);
	}

	//------------------------------------------------------------------------------------------------
	//! Living, conscious player characters within `radius` of `centre`, with their organisation.
	protected static void Gather(vector centre, float radius, notnull array<ref LF_SiteSighting> into)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(players.GetPlayerControlledEntity(playerId));
			if (!character)
				continue;

			float distance = vector.Distance(character.GetOrigin(), centre);
			if (distance > radius)
				continue;

			CharacterControllerComponent controller = character.GetCharacterController();
			if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE)
				continue;

			string characterId = LF_Hideout.CharacterIdOf(character);
			if (characterId.IsEmpty())
				continue;

			LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
			if (!org)
				continue;

			into.Insert(LF_SiteSighting.Create(characterId, org.GetId(), character.GetOrigin(), distance));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True when the character moved ACTIVE_MOVE_M since its last sample or acted within the window.
	//! A character seen for the first time counts as active.
	protected static bool IsActive(notnull LF_SiteSighting row, int nowUnix, int windowSeconds)
	{
		if (!s_mActivity)
			s_mActivity = new map<string, ref LF_SiteActivity>();

		LF_SiteActivity activity = s_mActivity.Get(row.m_sCharacterId);
		if (!activity)
		{
			activity = new LF_SiteActivity();
			activity.m_vPosition = row.m_vPosition;
			activity.m_iLastActive = nowUnix;
			s_mActivity.Set(row.m_sCharacterId, activity);
			return true;
		}

		if (vector.Distance(activity.m_vPosition, row.m_vPosition) >= ACTIVE_MOVE_M)
		{
			activity.m_vPosition = row.m_vPosition;
			activity.m_iLastActive = LF_Utils.MaxInt(activity.m_iLastActive, nowUnix);
		}

		return nowUnix - activity.m_iLastActive <= windowSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Scores one site from these sightings (server). The world sweep and tests both come here.
	static LF_ESiteHoldOutcome SweepSite(string hideoutId, notnull array<ref LF_SiteSighting> sightings, int nowUnix, float elapsedSeconds)
	{
		if (!Replication.IsServer() || hideoutId.IsEmpty())
			return LF_ESiteHoldOutcome.NONE;

		LF_SiteSettings settings = LF_SitesConfig.For(hideoutId);
		LF_SiteRules rules = settings.ToHoldRules();
		LF_SiteHoldState state = SyncState(hideoutId);

		// One presence row per active sighting; Step merges them per organisation and counts the
		// inner ring double.
		array<ref LF_SitePresence> presence = {};
		map<string, int> counted = new map<string, int>();
		foreach (LF_SiteSighting row : sightings)
		{
			if (!row || row.m_sOrgId.IsEmpty() || row.m_fDistance > settings.m_iRadius)
				continue;

			if (!IsActive(row, nowUnix, settings.m_iActiveWindowSeconds))
				continue;

			int inner;
			if (row.m_fDistance <= settings.m_iInnerRadius)
				inner = 1;

			presence.Insert(LF_SitePresence.Create(row.m_sOrgId, 1, inner));
			counted.Set(row.m_sOrgId, counted.Get(row.m_sOrgId) + 1 + inner);
		}

		if (!s_mCounted)
			s_mCounted = new map<string, ref map<string, int>>();
		s_mCounted.Set(hideoutId, counted);

		string holderBefore = state.m_sHolderOrgId;
		LF_ESiteHoldOutcome outcome = LF_SiteHold.Step(state, presence, elapsedSeconds, rules);
		NoteContest(hideoutId, holderBefore, outcome, counted.Count());
		LF_SiteRent.Accrue(hideoutId, holderBefore, outcome, elapsedSeconds, nowUnix);

		if (outcome == LF_ESiteHoldOutcome.CAPTURED)
		{
			HandOver(hideoutId, holderBefore, state);
			return outcome;
		}

		if (!state.m_sHolderOrgId.IsEmpty())
			LF_Hideout.SetHeldGround(hideoutId, state.m_sHolderOrgId, state.HoldSeconds(), state.HoldPoints());

		return outcome;
	}

	//------------------------------------------------------------------------------------------------
	//! The site's scoring state, its holder kept equal to the hideout's lessee: a lease that changed
	//! outside the sweep (released idle, emptied organisation, the desk) resets the hold to the lease
	//! and takes the hold seconds and points from the hideout's held ground.
	protected static LF_SiteHoldState SyncState(string hideoutId)
	{
		if (!s_mStates)
			s_mStates = new map<string, ref LF_SiteHoldState>();

		LF_SiteHoldState state = s_mStates.Get(hideoutId);
		bool fresh;
		if (!state)
		{
			state = new LF_SiteHoldState();
			s_mStates.Set(hideoutId, state);
			fresh = true;
		}

		string lessee = LF_Hideout.GetOwner(hideoutId);
		if (!fresh && state.m_sHolderOrgId == lessee)
			return state;

		state.m_sHolderOrgId = lessee;
		state.m_fHoldSeconds = 0;
		state.m_fHoldPoints = 0;
		LF_HeldGroundRow held = LF_Hideout.HeldGroundOf(hideoutId);
		if (held && !lessee.IsEmpty() && held.m_sHolderOrgId == lessee)
		{
			state.m_fHoldSeconds = held.m_iHoldSeconds;
			state.m_fHoldPoints = held.m_iHoldPoints;
		}

		if (state.m_sCapturerOrgId == lessee)
		{
			state.m_sCapturerOrgId = "";
			state.m_fCaptureProgress = 0;
		}

		return state;
	}

	//------------------------------------------------------------------------------------------------
	protected static void NoteContest(string hideoutId, string holderOrgId, LF_ESiteHoldOutcome outcome, int organisations)
	{
		if (!s_aContested)
			s_aContested = {};

		int at = s_aContested.Find(hideoutId);
		if (outcome != LF_ESiteHoldOutcome.CONTESTED)
		{
			if (at >= 0)
				s_aContested.Remove(at);
			return;
		}

		if (at >= 0)
			return;

		s_aContested.Insert(hideoutId);
		s_iContestStarts++;
		LF_Debug.Info("Sites", string.Format("site contested hideout=%1 holder=%2 organisations=%3", hideoutId, holderOrgId, organisations));
		LF_EventBus.Publish(LF_OnSiteContested.Create(hideoutId, holderOrgId, organisations));
	}

	//------------------------------------------------------------------------------------------------
	//! Step flipped the site: move the lease. A refused hand-over (the capturer leases another site)
	//! puts the hold back on the lessee so the site never claims a holder the lease disagrees with.
	protected static void HandOver(string hideoutId, string fromOrgId, notnull LF_SiteHoldState state)
	{
		string toOrgId = state.m_sHolderOrgId;
		if (!LF_Hideout.HandOver(hideoutId, toOrgId, "captured"))
		{
			LF_Debug.Warn("Sites", string.Format("capture of %1 by %2 refused: %3", hideoutId, toOrgId, LF_Hideout.GetLastRefusal()));
			s_mStates.Remove(hideoutId);
			SyncState(hideoutId);
			return;
		}

		s_iCaptures++;
		LF_Debug.Info("Sites", string.Format("site captured hideout=%1 from=%2 to=%3", hideoutId, fromOrgId, toOrgId));
		LF_EventBus.Publish(LF_OnSiteCaptured.Create(hideoutId, fromOrgId, toOrgId));
	}

	//------------------------------------------------------------------------------------------------
	//! Capture progress of the site's current challenger, 0 when none (tests read it).
	static float CaptureProgressOf(string hideoutId)
	{
		if (!s_mStates || !s_mStates.Get(hideoutId))
			return 0;

		return s_mStates.Get(hideoutId).m_fCaptureProgress;
	}

	//------------------------------------------------------------------------------------------------
	//! Counted presence (inner ring double) of one organisation at the site's last sweep.
	static int CountedOf(string hideoutId, string orgId)
	{
		if (!s_mCounted || !s_mCounted.Get(hideoutId))
			return 0;

		return s_mCounted.Get(hideoutId).Get(orgId);
	}

	//------------------------------------------------------------------------------------------------
	//! Captures published since the last reset.
	static int Captures()
	{
		return s_iCaptures;
	}

	//------------------------------------------------------------------------------------------------
	//! Contest starts published since the last reset.
	static int ContestStarts()
	{
		return s_iContestStarts;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ContestedSites's statics (LF_StaticReset finds it by reflection).
class LF_ContestedSites_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ContestedSites";
	}

	override void Apply()
	{
		LF_ContestedSites.ResetStatics();
	}
}
