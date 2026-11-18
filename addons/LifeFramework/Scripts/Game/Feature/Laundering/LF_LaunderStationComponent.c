//------------------------------------------------------------------------------------------------
//! Why the laundering station refused a character. OK is the only result that changed anything.
enum LF_ELaunderStation
{
	OK,
	NO_CONFIG,
	BUSY,
	EMPTY,
	TOO_FAR,
	NOT_ALIVE,
	TOO_FAST,
	NOTHING,
	DAILY_CAP,
	NOT_YOURS,
	NOT_READY,
	FAILED,
	RESTRAINED
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Laundering", description: "The laundering station (#2853): washes marked notes into cash minus the cut. Actions: LF_LaunderStationAction.")]
class LF_LaunderStationComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The laundering station (#2853, beat 3 of #1025). A character loads marked notes, the batch washes
//! for the config's batch time, and the same character takes out clean cash minus the cut
//! (LF_LaunderRules). Server only: the station takes the marked notes first, then pays; a failed
//! take puts back what it took, a failed pay keeps the unpaid rest in the station. The batch belongs
//! to the character who loaded it: walking away mid-wash leaves it waiting for them, nobody else.
//!
//! Heat: every batch warms the station's chain site (LF_ChainSites) by m_iHeatPerBatch work units of
//! the site's chain heat settings, the same heat a still gets, so the smell trace (LF_ChainTrace)
//! leads the police to it.
//!
//! Saved (#2894): the batch is the "launder" save section (LF_LaunderSave.c), keyed by the station's
//! chain site id; wash and ready time are kept as seconds, since world time restarts at 0. A batch
//! washed but uncollected for m_iForfeitSeconds is forfeited: logged, nobody paid, the station frees.
//! The day totals stay session-only (LF_LaunderRules).
class LF_LaunderStationComponent : ScriptComponent
{
	static const string FEATURE = "Laundering";
	static const string REASON_IN = "launder-in";
	static const string REASON_OUT = "launder-out";
	static const string REASON_REFUND = "launder-refund";

	static const int STATE_EMPTY = 0;
	static const int STATE_WASHING = 1;
	static const int STATE_READY = 2;

	[Attribute("", UIWidgets.EditBox, "Chain site this station warms. Stations with the same site id share heat (LF_ChainSites)")]
	protected string m_sSiteId;

	[Attribute("cider", UIWidgets.EditBox, "Chain id in ResourceChains.conf whose heat settings the site uses")]
	protected string m_sChainId;

	[Attribute("10", UIWidgets.Auto, "Heat per batch, in work units of the chain's heat settings (m_fPerWork each)")]
	protected int m_iHeatPerBatch;

	[Attribute("1000", UIWidgets.Auto, "Fastest one character's actions at the station repeat, in milliseconds; sooner is refused")]
	protected int m_iMinActionMs;

	[Attribute("3.5", UIWidgets.Auto, "Metres from the station a character must stand")]
	protected float m_fReach;

	[Attribute("7200", UIWidgets.Auto, "Seconds a washed batch waits for its owner; then it is forfeited (nobody is paid) and the station frees. 0 never forfeits")]
	protected int m_iForfeitSeconds;

	//! STATE_EMPTY, STATE_WASHING or STATE_READY. Server writes, clients read for the offer.
	[RplProp()]
	protected int m_iState;

	//! Server: the loaded batch. Owner is the worker key of the character who loaded it.
	protected string m_sOwnerKey;
	protected int m_iMarked;
	protected int m_iClean;
	protected float m_fReadyAt;

	protected ref map<string, float> m_mLastAction = new map<string, float>();

	//! Server: live stations by save key (LF_LaunderStations). Lives here so the component's own
	//! OnDelete removes it.
	protected static ref map<string, LF_LaunderStationComponent> s_mLive;

	//! Tests only: worker keys for bodies no player controls (entity id -> key).
	protected static ref map<string, string> s_mTestKeys;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer() || m_sSiteId.IsEmpty())
			return;

		LF_ChainSites.Register(m_sSiteId, m_sChainId, owner.GetOrigin());
		LF_LaunderStations.Register(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Test and setup seam: a station spawned in script uses its prefab's site; a test sets its own.
	//! The site id is also the save key, so the station moves to its new key in LF_LaunderStations.
	void Configure(string siteId, string chainId, int heatPerBatch)
	{
		if (Replication.IsServer())
			LF_LaunderStations.Unregister(this);

		m_sSiteId = siteId;
		m_sChainId = chainId;
		m_iHeatPerBatch = heatPerBatch;
		if (Replication.IsServer() && !m_sSiteId.IsEmpty())
			LF_LaunderStations.Register(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Test and setup seam: seconds a washed batch waits for its owner (0 never forfeits).
	void SetForfeitSeconds(int seconds)
	{
		m_iForfeitSeconds = LF_Utils.MaxInt(seconds, 0);
	}

	//------------------------------------------------------------------------------------------------
	string GetSiteId()
	{
		return m_sSiteId;
	}

	//------------------------------------------------------------------------------------------------
	//! The station's save key: its chain site id.
	string GetStationKey()
	{
		return m_sSiteId;
	}

	//------------------------------------------------------------------------------------------------
	//! STATE_EMPTY, STATE_WASHING or STATE_READY. On the server it is computed at nowSeconds, after
	//! forfeiting a washed batch left past its window.
	int GetState(float nowSeconds)
	{
		if (!Replication.IsServer())
			return m_iState;

		Expire(nowSeconds);
		return StateAt(nowSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner of the loaded batch (empty when the station is empty).
	string GetOwnerKey()
	{
		return m_sOwnerKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Clean cash owed for the loaded batch (a short pay leaves the unpaid rest here).
	int GetOwedClean()
	{
		return m_iClean;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a washed batch left uncollected past m_iForfeitSeconds is forfeited. It is logged with
	//! owner and amounts, nobody is paid, and the station frees.
	//! \return True when a batch was forfeited.
	bool Expire(float nowSeconds)
	{
		if (m_iForfeitSeconds <= 0 || StateAt(nowSeconds) != STATE_READY)
			return false;

		float waited = nowSeconds - m_fReadyAt;
		if (waited < m_iForfeitSeconds)
			return false;

		LF_Debug.Info(FEATURE, string.Format("station %1: %2's washed batch forfeited, uncollected for %3 s (limit %4 s): %5 marked, %6 clean owed; nobody paid, station free", m_sSiteId, m_sOwnerKey, (int)waited, m_iForfeitSeconds, m_iMarked, m_iClean));
		ClearBatch(nowSeconds);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearBatch(float nowSeconds)
	{
		m_sOwnerKey = "";
		m_iMarked = 0;
		m_iClean = 0;
		m_fReadyAt = 0;
		Sync(nowSeconds);
		ScheduleTimer(nowSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the loaded batch as a row, or null when the station is empty. Times are
	//! seconds relative to nowSeconds (world time restarts at 0 on a boot). Forfeits first.
	LF_LaunderStationRow Export(float nowSeconds)
	{
		Expire(nowSeconds);
		if (m_sOwnerKey.IsEmpty())
			return null;

		int washLeft;
		int readyFor;
		if (nowSeconds < m_fReadyAt)
			washLeft = (int)Math.Ceil(m_fReadyAt - nowSeconds); // lf-float: sub-second wash time, rounded up
		else
			readyFor = (int)Math.Floor(nowSeconds - m_fReadyAt); // lf-float: sub-second ready time, rounded down

		return LF_LaunderStationRow.Create(m_sSiteId, m_sOwnerKey, m_iMarked, m_iClean, washLeft, readyFor);
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: puts a saved batch back at nowSeconds, or empties the station for a null
	//! row. Idempotent: the same row gives the same batch. A broken row is logged and dropped.
	void Import(LF_LaunderStationRow row, float nowSeconds)
	{
		if (!row)
		{
			if (!m_sOwnerKey.IsEmpty())
				LF_Debug.Info(FEATURE, string.Format("station %1: the save holds no batch; %2's live batch is replaced by an empty station", m_sSiteId, m_sOwnerKey));

			ClearBatch(nowSeconds);
			return;
		}

		if (row.m_sOwnerKey.IsEmpty() || row.m_iMarked <= 0 || row.m_iClean < 0 || row.m_iWashLeft < 0 || row.m_iReadySeconds < 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("station %1: saved batch dropped, it is broken (owner '%2', %3 marked, %4 clean, wash %5 s, ready %6 s)", m_sSiteId, row.m_sOwnerKey, row.m_iMarked, row.m_iClean, row.m_iWashLeft, row.m_iReadySeconds));
			ClearBatch(nowSeconds);
			return;
		}

		m_sOwnerKey = row.m_sOwnerKey;
		m_iMarked = row.m_iMarked;
		m_iClean = row.m_iClean;
		m_fReadyAt = nowSeconds + row.m_iWashLeft - row.m_iReadySeconds;
		LF_Debug.Info(FEATURE, string.Format("station %1 restored: %2's batch of %3 marked for %4 clean, wash %5 s left, ready for %6 s", m_sSiteId, m_sOwnerKey, m_iMarked, m_iClean, row.m_iWashLeft, row.m_iReadySeconds));
		if (!Expire(nowSeconds))
		{
			Sync(nowSeconds);
			ScheduleTimer(nowSeconds);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Replicated state, for a client's action offer.
	int GetReplicatedState()
	{
		return m_iState;
	}

	//------------------------------------------------------------------------------------------------
	//! Marked notes held in the station for the loaded batch (0 when empty).
	int GetHeldMarked()
	{
		return m_iMarked;
	}

	//------------------------------------------------------------------------------------------------
	protected int StateAt(float nowSeconds)
	{
		if (m_sOwnerKey.IsEmpty())
			return STATE_EMPTY;

		if (nowSeconds < m_fReadyAt)
			return STATE_WASHING;

		return STATE_READY;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: pushes the state to clients when it changed. Washing turns ready on the next action
	//! or tick (the action's offer reads it).
	void Sync(float nowSeconds)
	{
		int state = StateAt(nowSeconds);
		if (state == m_iState)
			return;

		m_iState = state;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! The stable key of a character: the player UID, or a test key for a body no player controls.
	static string WorkerKey(IEntity user)
	{
		if (!user)
			return "";

		string uid = LF_Utils.GetPlayerUID(user);
		if (!uid.IsEmpty())
			return uid;

		if (s_mTestKeys)
		{
			string key;
			if (s_mTestKeys.Find(user.GetID().ToString(), key))
				return key;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: give `body` (no player) a worker key; an empty key forgets it.
	static void SetKeyForTests(IEntity body, string key)
	{
		if (!body)
			return;

		if (!s_mTestKeys)
			s_mTestKeys = new map<string, string>();

		if (key.IsEmpty())
			s_mTestKeys.Remove(body.GetID().ToString());
		else
			s_mTestKeys.Set(body.GetID().ToString(), key);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mTestKeys = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Living, standing, in reach and not repeating faster than a hold. Records the action time.
	protected LF_ELaunderStation CheckUser(IEntity user, string key, float nowSeconds)
	{
		if (!user || key.IsEmpty())
			return LF_ELaunderStation.FAILED;

		if (!LF_Utils.IsAliveState(user) || LF_Downed.IsDowned(user))
			return LF_ELaunderStation.NOT_ALIVE;

		// Cuffed: refused like robbery and lockpicking (LF_CuffEnforcement tells the player why).
		if (LF_CuffEnforcement.Refuse(user, "laundering"))
			return LF_ELaunderStation.RESTRAINED;

		if (vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) > m_fReach)
			return LF_ELaunderStation.TOO_FAR;

		// Find leaves `last` at 0 on a miss, so a first action early in the world must not read as a repeat.
		float last;
		if (m_mLastAction.Find(key, last) && (nowSeconds - last) * 1000 < m_iMinActionMs)
		{
			LF_Debug.Warn(FEATURE, string.Format("station %1: %2 asked again faster than a hold, refused", m_sSiteId, key));
			return LF_ELaunderStation.TOO_FAST;
		}

		m_mLastAction.Set(key, nowSeconds);
		return LF_ELaunderStation.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `user` loads their marked notes, up to the batch and day limits. The notes leave the
	//! inventory before anything else changes; a short take is put back and refused.
	//! \param nowSeconds World seconds (batch timing and heat); the day window reads unix time.
	LF_ELaunderStation ServerLoad(IEntity user, float nowSeconds)
	{
		if (!Replication.IsServer())
			return LF_ELaunderStation.FAILED;

		string key = WorkerKey(user);
		LF_ELaunderStation access = CheckUser(user, key, nowSeconds);
		if (access != LF_ELaunderStation.OK)
			return access;

		Expire(nowSeconds);
		if (StateAt(nowSeconds) != STATE_EMPTY)
			return LF_ELaunderStation.BUSY;

		LF_LaunderConfig config = LF_LaunderConfig.GetRules();
		if (!config)
			return LF_ELaunderStation.NO_CONFIG;

		int held = LF_MarkedCash.Count(user);
		int unixNow = System.GetUnixTime();
		LF_LaunderQuote quote = LF_LaunderRules.Quote(key, held, unixNow, config.m_iCutPct, config.m_iBatchMax, config.m_iDailyCap);
		if (quote.m_sReason == LF_LaunderRules.REASON_DAILY_CAP)
		{
			LF_Debug.Info(FEATURE, string.Format("station %1: %2 refused, at the daily cap", m_sSiteId, key));
			return LF_ELaunderStation.DAILY_CAP;
		}

		if (quote.m_iAccepted <= 0)
			return LF_ELaunderStation.NOTHING;

		// Take first. A short take is put back exactly once and nothing else changes.
		int taken = LF_MarkedCash.Take(user, quote.m_iAccepted, REASON_IN);
		if (taken != quote.m_iAccepted)
		{
			int putBack;
			if (taken > 0)
				putBack = LF_MarkedCash.Add(user, taken, REASON_REFUND, true);

			LF_Debug.Error(FEATURE, string.Format("station %1: load by %2 failed, took %3 of %4; put back %5", m_sSiteId, key, taken, quote.m_iAccepted, putBack));
			return LF_ELaunderStation.FAILED;
		}

		LF_LaunderRules.Record(key, taken, unixNow);
		m_sOwnerKey = key;
		m_iMarked = taken;
		m_iClean = quote.m_iClean;
		m_fReadyAt = nowSeconds + LF_LaunderRules.SecondsFor(taken, config.m_iBatchMax, config.m_iBatchSeconds);
		Sync(nowSeconds);
		ScheduleTimer(nowSeconds);

		LF_EventBus.Publish(LF_OnMoneyMoved.CreateFull(key, LF_Ledger.ACCOUNT_MARKED, taken, false, REASON_IN, LF_MarkedCash.Count(user), ""));
		Warm(key, nowSeconds);
		LF_Debug.Info(FEATURE, string.Format("station %1 loaded by %2: %3 marked for %4 clean, ready at %5", m_sSiteId, key, taken, m_iClean, m_fReadyAt));
		return LF_ELaunderStation.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one timer for the next edge of the batch: the wash ends (clients see it ready), then
	//! the forfeit window ends (the station frees without anyone touching it).
	protected void ScheduleTimer(float nowSeconds)
	{
		if (!GetGame() || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().Remove(OnTimer);
		if (m_sOwnerKey.IsEmpty())
			return;

		float next = m_fReadyAt;
		if (nowSeconds >= m_fReadyAt)
		{
			if (m_iForfeitSeconds <= 0)
				return;

			next = m_fReadyAt + m_iForfeitSeconds;
		}

		int delayMs = LF_Utils.MaxInt((int)((next - nowSeconds) * 1000), 0) + 100;
		GetGame().GetCallqueue().CallLater(OnTimer, delayMs, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTimer()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime() / 1000;
		if (!Expire(now))
		{
			Sync(now);
			ScheduleTimer(now);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(OnTimer);

		// A station that leaves keeps its batch as a pending save row (LF_LaunderStations).
		if (Replication.IsServer())
			LF_LaunderStations.Unregister(this);

		if (s_mLive && s_mLive.Get(m_sSiteId) == this)
			s_mLive.Remove(m_sSiteId);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	static map<string, LF_LaunderStationComponent> Live()
	{
		if (!s_mLive)
			s_mLive = new map<string, LF_LaunderStationComponent>();

		return s_mLive;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetLive()
	{
		s_mLive = null;
	}

	//------------------------------------------------------------------------------------------------
	//! One batch of heat on the chain site, and a work event so the trace sweep wakes.
	protected void Warm(string key, float nowSeconds)
	{
		if (m_sSiteId.IsEmpty() || m_iHeatPerBatch <= 0)
			return;

		float heat = LF_ChainSites.AddWork(m_sSiteId, m_iHeatPerBatch, nowSeconds);
		float rangeM = LF_ChainSites.GetRange(m_sSiteId, nowSeconds);
		LF_EventBus.Publish(LF_OnChainWork.Create(key, m_sSiteId, m_sChainId, "launder:" + m_sSiteId, 0, heat, rangeM, GetOwner().GetOrigin()));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the character who loaded the batch takes out the clean cash once it is washed. A
	//! short pay keeps the unpaid rest in the station for the same character.
	LF_ELaunderStation ServerCollect(IEntity user, float nowSeconds)
	{
		if (!Replication.IsServer())
			return LF_ELaunderStation.FAILED;

		string key = WorkerKey(user);
		LF_ELaunderStation access = CheckUser(user, key, nowSeconds);
		if (access != LF_ELaunderStation.OK)
			return access;

		Expire(nowSeconds);
		int state = StateAt(nowSeconds);
		if (state == STATE_EMPTY)
			return LF_ELaunderStation.EMPTY;

		if (key != m_sOwnerKey)
		{
			LF_Debug.Info(FEATURE, string.Format("station %1: %2 tried to take %3's batch, refused", m_sSiteId, key, m_sOwnerKey));
			return LF_ELaunderStation.NOT_YOURS;
		}

		if (state != STATE_READY)
			return LF_ELaunderStation.NOT_READY;

		int owed = m_iClean;
		int paid = LF_MoneyUtils.AddCash(user, owed, REASON_OUT, "", true);
		if (paid < owed)
		{
			m_iClean = owed - LF_Utils.MaxInt(paid, 0);
			LF_Debug.Error(FEATURE, string.Format("station %1: paid %2 of %3 to %4; the rest stays in the station", m_sSiteId, paid, owed, key));
			return LF_ELaunderStation.FAILED;
		}

		LF_Debug.Info(FEATURE, string.Format("station %1: %2 took %3 clean for %4 marked", m_sSiteId, key, owed, m_iMarked));
		ClearBatch(nowSeconds);
		return LF_ELaunderStation.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key telling the character why, or empty for a result that needs no words.
	static string RefusalKey(LF_ELaunderStation result)
	{
		switch (result)
		{
			case LF_ELaunderStation.NO_CONFIG: return "#LF-Launder_Closed";
			case LF_ELaunderStation.BUSY: return "#LF-Launder_Busy";
			case LF_ELaunderStation.EMPTY: return "#LF-Launder_Empty";
			case LF_ELaunderStation.TOO_FAR: return "#LF-Chain_TooFar";
			case LF_ELaunderStation.NOTHING: return "#LF-Launder_NoNotes";
			case LF_ELaunderStation.DAILY_CAP: return "#LF-Launder_DailyCap";
			case LF_ELaunderStation.NOT_YOURS: return "#LF-Launder_NotYours";
			case LF_ELaunderStation.NOT_READY: return "#LF-Launder_NotReady";
			case LF_ELaunderStation.FAILED: return "#LF-Chain_Failed";
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LaunderStationComponent's statics (LF_StaticReset finds it by reflection).
class LF_LaunderStationComponent_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LaunderStationComponent";
	}

	override void Apply()
	{
		LF_LaunderStationComponent.ResetStatics();
	}
}
