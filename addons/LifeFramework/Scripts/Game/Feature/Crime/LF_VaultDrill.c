//------------------------------------------------------------------------------------------------
enum LF_EVaultDrillStep
{
	KEEP,
	DONE,
	STOP_GONE,
	STOP_DOWN,
	STOP_RANGE,
	STOP_UNEQUIPPED
}

//------------------------------------------------------------------------------------------------
//! One robber drilling one vault. Server only; entities are held by id.
class LF_VaultDrillSession : Managed
{
	string m_sVaultId;
	string m_sPlayerUid;
	EntityID m_Robber;
	EntityID m_Drill;
	vector m_vAnchor;
	float m_fStartedAt;
	float m_fSeconds;
}

//------------------------------------------------------------------------------------------------
//! The timed vault drill. The robber holds the drill (LF_HandTool) at the door; the server times the
//! work on its own clock and pays (LF_Bank.FinishDrill) only when the drill ran its full time with
//! the robber alive, in reach and the drill still in the hands. Cancelled, interrupted, unequipped,
//! walked off or broken pays nothing; a session is removed before it pays, so it pays once.
class LF_VaultDrill
{
	static const string FEATURE = "Bank";
	static const string TOOL_ID = "drill";
	static const float TICK_SECONDS = 0.5; // lf-fixed: check cadence, the same as the lockpick tick, not tuning
	//! The robber must stay this close to the vault door (metres).
	static const float MAX_DISTANCE = 3.5; // lf-fixed: anti-cheat reach at the door, not a gameplay knob
	static const float DEFAULT_SECONDS = 30; // lf-fixed: fallback only; the tunable is LF_BankVaultComponent.m_fDrillSeconds

	//! Test seam: a value >= 0 is the server clock in seconds.
	static float s_fNowOverride = -1;

	protected static ref array<ref LF_VaultDrillSession> s_aSessions = {};
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aSessions.Clear();
		s_fNowOverride = -1;
		s_bTicking = false;
	}

	//------------------------------------------------------------------------------------------------
	static float Now()
	{
		if (s_fNowOverride >= 0)
			return s_fNowOverride;

		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: what one tick decides. Elapsed is server seconds since the drilling began.
	static LF_EVaultDrillStep Step(bool robberThere, bool alive, float distance, bool drillHeld, float elapsedSeconds, float seconds)
	{
		if (!robberThere)
			return LF_EVaultDrillStep.STOP_GONE;

		if (!alive)
			return LF_EVaultDrillStep.STOP_DOWN;

		if (distance > MAX_DISTANCE)
			return LF_EVaultDrillStep.STOP_RANGE;

		if (!drillHeld)
			return LF_EVaultDrillStep.STOP_UNEQUIPPED;

		if (elapsedSeconds >= seconds)
			return LF_EVaultDrillStep.DONE;

		return LF_EVaultDrillStep.KEEP;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_VaultDrillSession Find(string vaultId)
	{
		foreach (LF_VaultDrillSession session : s_aSessions)
		{
			if (session.m_sVaultId == vaultId)
				return session;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsDrilling(string vaultId)
	{
		return Find(vaultId) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! True when this character is the one drilling the vault (the action then reads "Stop drilling").
	static bool IsDrillingBy(string vaultId, IEntity robber)
	{
		LF_VaultDrillSession session = Find(vaultId);
		return session && robber && session.m_Robber == robber.GetID();
	}

	//------------------------------------------------------------------------------------------------
	static int GetActiveCount()
	{
		return s_aSessions.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the robber starts drilling with the drill in their hands.
	static bool Begin(string vaultId, string playerUid, notnull IEntity robber, notnull IEntity drill, vector anchor, float seconds)
	{
		if (!Replication.IsServer())
			return false;

		if (Find(vaultId))
		{
			LF_Debug.Info(FEATURE, string.Format("drill refused: %1 is already being drilled", vaultId));
			return false;
		}

		LF_VaultDrillSession session = new LF_VaultDrillSession();
		session.m_sVaultId = vaultId;
		session.m_sPlayerUid = playerUid;
		session.m_Robber = robber.GetID();
		session.m_Drill = drill.GetID();
		session.m_vAnchor = anchor;
		session.m_fStartedAt = Now();
		session.m_fSeconds = seconds;
		s_aSessions.Insert(session);
		Spin(drill, true);
		LF_Debug.Info(FEATURE, string.Format("%1 starts drilling %2 for %3 s", playerUid, vaultId, seconds));
		LF_Utils.NotifyEntity(robber, "#LF-Bank_DrillStarted", "#EL-Bank_Rob", 4.0);

		if (!s_bTicking && GetGame())
		{
			s_bTicking = true;
			GetGame().GetCallqueue().CallLater(Tick, TICK_SECONDS * 1000, true);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the robber stopped (the action again) or the drill was taken away. Pays nothing.
	static bool Cancel(string vaultId, string reason)
	{
		LF_VaultDrillSession session = Find(vaultId);
		if (!session)
			return false;

		Drop(session, reason);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One step over every session. Also the test seam (with s_fNowOverride).
	static void Tick()
	{
		for (int i = s_aSessions.Count() - 1; i >= 0; i--)
		{
			if (!s_aSessions.IsIndexValid(i))
				continue;

			LF_VaultDrillSession session = s_aSessions[i];
			IEntity robber = FindById(session.m_Robber);
			bool alive = false;
			float distance = 0;
			bool held = false;
			if (robber)
			{
				CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(robber);
				alive = !controller || controller.GetLifeState() == ECharacterLifeState.ALIVE;
				distance = vector.Distance(robber.GetOrigin(), session.m_vAnchor);
				IEntity drill = LF_HandTool.HeldTool(robber, TOOL_ID);
				held = drill && drill.GetID() == session.m_Drill;
			}

			LF_EVaultDrillStep step = Step(robber != null, alive, distance, held, Now() - session.m_fStartedAt, session.m_fSeconds);
			if (step == LF_EVaultDrillStep.KEEP)
				continue;

			if (step == LF_EVaultDrillStep.DONE)
			{
				Finish(session, robber);
				continue;
			}

			Drop(session, typename.EnumToString(LF_EVaultDrillStep, step));
		}

		if (s_aSessions.IsEmpty() && s_bTicking && GetGame())
		{
			s_bTicking = false;
			GetGame().GetCallqueue().Remove(Tick);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Finish(LF_VaultDrillSession session, IEntity robber)
	{
		// Removed before paying: a second tick, a second action or a repeat call cannot pay again.
		s_aSessions.RemoveItem(session);
		IEntity drill = FindById(session.m_Drill);
		Spin(drill, false);
		LF_Debug.Info(FEATURE, string.Format("%1 finished drilling %2 after %3 s", session.m_sPlayerUid, session.m_sVaultId, Now() - session.m_fStartedAt));
		LF_Bank.FinishDrill(session.m_sVaultId, session.m_sPlayerUid, robber, drill);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Drop(LF_VaultDrillSession session, string reason)
	{
		s_aSessions.RemoveItem(session);
		Spin(FindById(session.m_Drill), false);
		LF_Debug.Info(FEATURE, string.Format("drilling %1 by %2 stopped (%3): nothing paid", session.m_sVaultId, session.m_sPlayerUid, reason));
		IEntity robber = FindById(session.m_Robber);
		if (robber)
			LF_Utils.NotifyEntity(robber, "#LF-Bank_DrillStopped", "#EL-Bank_Rob", 4.0);
	}

	//------------------------------------------------------------------------------------------------
	//! The drill's chuck, sparks and motor while it works (LF_AssetAnimComponent replicates it).
	protected static void Spin(IEntity drill, bool on)
	{
		LF_AssetAnimComponent anim = LF_AssetAnimComponent.Of(drill);
		if (anim)
			anim.SetActive(on);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindById(EntityID id)
	{
		if (id == EntityID.INVALID || !GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(id);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_VaultDrill's statics (registered at script load; Core/LF_StaticReset.c).
class LF_VaultDrill_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_VaultDrill_StaticResetHook());

	override string Owner()
	{
		return "LF_VaultDrill";
	}

	override void Apply()
	{
		LF_VaultDrill.ResetStatics();
	}
}
