//------------------------------------------------------------------------------------------------
//! The bleed-out timer of the downed state (#256). Server-owned: every downed character carries
//! the seconds it has left; one shared one-second tick counts them down and, at zero, calls
//! LF_Downed.Finish, the real death (murder hook, hospital wake). The downed player can also give
//! up once the configured delay has passed. The client reads the time left through a replicated,
//! throttled copy (SCR_ChimeraCharacter.LF_GetBleedSecondsLeft); nothing replicates per frame.
//! A character that disconnects while downed dies the same way (config).
//------------------------------------------------------------------------------------------------
class LF_BleedOut
{
	static const string FEATURE = "Medic";
	static const int TICK_MS = 1000;

	//! The replicated copy moves when it differs from the real value by at least this many seconds.
	static const int SYNC_STEP_SECONDS = 5;

	protected static ref array<EntityID> s_aDowned = {};
	protected static bool s_bTicking;
	protected static float s_fLastTickMs;
	protected static int s_iExpiredCount;
	protected static int s_iGaveUpCount;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bTicking)
			GetGame().GetCallqueue().Remove(Tick);

		s_aDowned = {};
		s_bTicking = false;
		s_fLastTickMs = 0;
		s_iExpiredCount = 0;
		s_iGaveUpCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds left after `dt` seconds of bleeding. Never below zero.
	static float Elapse(float left, float dt)
	{
		return Math.Max(0, left - dt);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsExpired(float left)
	{
		return left <= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Giving up is allowed once the player has been downed for `delay` seconds.
	static bool CanGiveUp(float downedFor, float delay)
	{
		return downedFor >= delay;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the replicated copy (`synced`, whole seconds) is stale enough to be bumped.
	//! Always true at zero so the client sees the end.
	static bool NeedsSync(int synced, float left)
	{
		int whole = Math.Ceil(left);
		if (whole <= 0)
			return synced != 0;

		return Math.AbsInt(synced - whole) >= SYNC_STEP_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: start the countdown for a character that has just gone down.
	static void Begin(notnull SCR_ChimeraCharacter character)
	{
		if (!Replication.IsServer())
			return;

		character.LF_StartBleeding(LF_MedicConfig.Get().m_fBleedOutSeconds);
		EntityID id = character.GetID();
		if (s_aDowned.Find(id) < 0)
			s_aDowned.Insert(id);

		LF_Utils.NotifyEntity(character, "#LF-Medic_Downed", "", 8.0, Math.Ceil(LF_MedicConfig.Get().m_fGiveUpDelaySeconds).ToString());

		if (s_bTicking)
			return;

		s_bTicking = true;
		s_fLastTickMs = GetGame().GetWorld().GetWorldTime();
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the character is no longer downed (revive or death). Stops its countdown.
	static void End(notnull SCR_ChimeraCharacter character)
	{
		character.LF_StopBleeding();
		int index = s_aDowned.Find(character.GetID());
		if (index >= 0)
			s_aDowned.Remove(index);

		if (s_aDowned.IsEmpty())
			StopTicking();
	}

	//------------------------------------------------------------------------------------------------
	protected static void StopTicking()
	{
		if (!s_bTicking)
			return;

		GetGame().GetCallqueue().Remove(Tick);
		s_bTicking = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The shared one-second tick: count every downed character down, kill the ones that ran out.
	protected static void Tick()
	{
		float now = GetGame().GetWorld().GetWorldTime();
		float dt = (now - s_fLastTickMs) * 0.001;
		s_fLastTickMs = now;
		Advance(dt);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `dt` seconds of bleeding for every downed character. Tests call it to skip the wait.
	static void Advance(float dt)
	{
		int perfToken = LF_Perf.Begin();
		array<SCR_ChimeraCharacter> expired = {};
		for (int i = s_aDowned.Count() - 1; i >= 0; i--)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().GetWorld().FindEntityByID(s_aDowned[i]));
			if (!character || !character.LF_IsDowned())
			{
				s_aDowned.Remove(i);
				continue;
			}

			// A patient lying on a stretcher (a seat of an ambulance) does not bleed out (#262).
			if (LF_BedDelivery.IsLoaded(character))
				continue;

			// The playtest town's practice patient waits for a medic as long as it takes.
			if (LF_DownedPatient.IsPatient(character))
				continue;

			if (character.LF_BleedStep(dt))
				expired.Insert(character);
		}

		foreach (SCR_ChimeraCharacter dead : expired)
		{
			s_iExpiredCount++;
			LF_Debug.Info(FEATURE, string.Format("player %1 bled out", LF_Utils.GetPlayerUID(dead)));
			Finish(dead);
		}

		LF_Perf.Gauge("Medic.downed", s_aDowned.Count());
		LF_Perf.End("Medic.bleedAdvance", perfToken);

		if (s_aDowned.IsEmpty())
			StopTicking();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the downed player asks to give up. Refused unless they are downed and the delay
	//! has passed. \return True when the character was killed.
	static bool GiveUp(IEntity entity)
	{
		if (!Replication.IsServer())
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character || !character.LF_IsDowned())
		{
			LF_Debug.Warn(FEATURE, "give up refused: the player is not downed");
			return false;
		}

		if (character.LF_IsStunned())
		{
			LF_Debug.Info(FEATURE, string.Format("give up refused for %1: stunned, not dying", LF_Utils.GetPlayerUID(character)));
			return false;
		}

		float delay = LF_MedicConfig.Get().m_fGiveUpDelaySeconds;
		if (!CanGiveUp(character.LF_GetDownedForSeconds(), delay))
		{
			LF_Debug.Info(FEATURE, string.Format("give up refused for %1: downed %2s, needs %3s", LF_Utils.GetPlayerUID(character), character.LF_GetDownedForSeconds(), delay));
			LF_Utils.NotifyEntity(character, "#LF-Medic_GiveUpTooEarly", "", 3.0, Math.Ceil(delay).ToString());
			return false;
		}

		s_iGaveUpCount++;
		LF_Debug.Info(FEATURE, string.Format("player %1 gave up", LF_Utils.GetPlayerUID(character)));
		return Finish(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player disconnects. While downed that counts as death (config), so the murder
	//! and the hospital wake do not depend on the body the base game deletes next.
	static void OnPlayerDisconnected(int playerId)
	{
		if (!Replication.IsServer() || !LF_MedicConfig.Get().m_bDisconnectWhileDownedKills)
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		IEntity controlled = playerManager.GetPlayerControlledEntity(playerId);
		if (!LF_Downed.IsDowned(controlled))
			return;

		// A stun is not a death in waiting: it never kills, whatever the player does.
		if (LF_Stun.IsStunned(controlled))
			return;

		LF_Debug.Info(FEATURE, string.Format("player %1 disconnected while downed: death", LF_Utils.GetPlayerUID(playerId)));
		Finish(SCR_ChimeraCharacter.Cast(controlled));
	}

	//------------------------------------------------------------------------------------------------
	//! The real death of a downed character. The murder lands on whoever downed them when that
	//! player is still connected; otherwise the character dies by its own hand (no murder).
	protected static bool Finish(SCR_ChimeraCharacter character)
	{
		if (!character)
			return false;

		return LF_Downed.Finish(character, InstigatorFor(character));
	}

	//------------------------------------------------------------------------------------------------
	protected static Instigator InstigatorFor(notnull SCR_ChimeraCharacter character)
	{
		int downerId = PlayerIdOfUid(character.LF_GetDownerUid());
		if (downerId <= 0)
			return Instigator.CreateInstigator(character);

		Instigator instigator = Instigator.CreateInstigator(null);
		instigator.SetInstigatorByPlayerID(downerId);
		return instigator;
	}

	//------------------------------------------------------------------------------------------------
	protected static int PlayerIdOfUid(string uid)
	{
		if (uid.IsEmpty())
			return 0;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return 0;

		array<int> players = {};
		playerManager.GetPlayers(players);
		foreach (int playerId : players)
		{
			if (LF_Utils.GetPlayerUID(playerId) == uid)
				return playerId;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	static int GetExpiredCount()
	{
		return s_iExpiredCount;
	}

	//------------------------------------------------------------------------------------------------
	static int GetGaveUpCount()
	{
		return s_iGaveUpCount;
	}
}

//------------------------------------------------------------------------------------------------
//! The bleed-out clock on the character. The seconds are server state; the whole-second copy is
//! an RplProp moved at most every LF_BleedOut.SYNC_STEP_SECONDS (and at the start and the end).
modded class SCR_ChimeraCharacter
{
	[RplProp()]
	protected int m_iLF_BleedSecondsLeft = -1;

	protected float m_fLF_BleedLeft;
	protected float m_fLF_DownedFor;

	//------------------------------------------------------------------------------------------------
	void LF_StartBleeding(float seconds)
	{
		m_fLF_BleedLeft = seconds;
		m_fLF_DownedFor = 0;
		m_iLF_BleedSecondsLeft = Math.Ceil(seconds);
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	void LF_StopBleeding()
	{
		m_fLF_BleedLeft = 0;
		m_fLF_DownedFor = 0;
		if (m_iLF_BleedSecondsLeft == -1)
			return;

		m_iLF_BleedSecondsLeft = -1;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `dt` seconds passed. \return True when the time has run out.
	bool LF_BleedStep(float dt)
	{
		m_fLF_DownedFor += dt;
		m_fLF_BleedLeft = LF_BleedOut.Elapse(m_fLF_BleedLeft, dt);
		if (LF_BleedOut.NeedsSync(m_iLF_BleedSecondsLeft, m_fLF_BleedLeft))
		{
			m_iLF_BleedSecondsLeft = Math.Ceil(m_fLF_BleedLeft);
			Replication.BumpMe();
		}

		return LF_BleedOut.IsExpired(m_fLF_BleedLeft);
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds the client believes are left; -1 when not downed. Up to SYNC_STEP_SECONDS stale.
	int LF_GetBleedSecondsLeft()
	{
		return m_iLF_BleedSecondsLeft;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the exact seconds left (tests and the harness read it).
	float LF_GetBleedLeft()
	{
		return m_fLF_BleedLeft;
	}

	//------------------------------------------------------------------------------------------------
	float LF_GetDownedForSeconds()
	{
		return m_fLF_DownedFor;
	}
}
