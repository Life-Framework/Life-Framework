//------------------------------------------------------------------------------------------------
//! What the idle sampler last saw of one on-duty player (#3120). Runtime only: a restart or a
//! rejoin starts a fresh watch, and the spawn restarts the duty clock (LF_PoliceRanks.ResumeDutyClock).
class LF_DutyIdleWatch : Managed
{
	bool m_bHasSample;
	vector m_vPosition;
	vector m_vAim;
	int m_iLastActiveMs;
	//! True while this sampler holds the clock paused.
	bool m_bPaused;

	//------------------------------------------------------------------------------------------------
	void Record(int nowMs, vector position, vector aim)
	{
		m_bHasSample = true;
		m_vPosition = position;
		m_vAim = aim;
		m_iLastActiveMs = nowMs;
	}
}

//------------------------------------------------------------------------------------------------
//! Duty pay pauses while the officer or medic stands idle (#3120). Every SAMPLE_MS the server looks
//! at each connected player on duty. Moving more than 2 m (on foot or in any vehicle), or turning
//! the view, counts as active: a player at a desk, on patrol, in a parked car looking out, or
//! driving to a call is playing the role. One who does none of that for `dutyIdleSeconds`
//! (LF_JobsConfig, 0 = off) has the duty clock settled at the moment the window ran out and
//! stopped; the idle seconds after it earn no pay and no rank time. The next sample that sees
//! activity starts the clock again. The rank rates (LF_DutyPay) are untouched.
class LF_DutyIdle
{
	static const string FEATURE = "DutyIdle";
	static const int SAMPLE_MS = 30000;	// lf-fixed: sampler cadence, the pause lands at most this late
	static const float MOVE_METRES = 2;	// lf-fixed: the card's activity distance
	static const float AIM_COS = 0.985;	// lf-fixed: about 10 degrees of view turn counts as active

	protected static ref map<string, ref LF_DutyIdleWatch> s_mWatches = new map<string, ref LF_DutyIdleWatch>();

	//------------------------------------------------------------------------------------------------
	//! Configured idle window in seconds; 0 (or a missing config) turns the pause off.
	static int IdleSeconds()
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config)
			return 0;

		return LF_Utils.MaxInt(0, config.m_iDutyIdleSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a player moved MOVE_METRES or turned the view between two samples. A zero aim
	//! (no aiming component) never counts as a turn.
	static bool IsActive(vector prevPosition, vector position, vector prevAim, vector aim)
	{
		if (vector.DistanceSq(prevPosition, position) > MOVE_METRES * MOVE_METRES)
			return true;

		if (prevAim == vector.Zero || aim == vector.Zero)
			return false;

		return vector.Dot(prevAim.Normalized(), aim.Normalized()) < AIM_COS;
	}

	//------------------------------------------------------------------------------------------------
	//! One sample of one account at nowMs. Pauses (settles and stops) a running clock once the
	//! player has been idle for idleSeconds, and restarts a clock this sampler paused on activity.
	//! \return True when the sample paused or resumed the clock.
	static bool SampleAt(LF_PlayerAccount account, notnull LF_DutyIdleWatch watch, int nowMs, vector position, vector aim, int idleSeconds)
	{
		if (!account || !account.IsOnDuty() || account.GetFaction() == LF_Faction.CIVILIAN)
		{
			watch.m_bHasSample = false;
			watch.m_bPaused = false;
			return false;
		}

		bool active = !watch.m_bHasSample || idleSeconds <= 0 || IsActive(watch.m_vPosition, position, watch.m_vAim, aim);
		if (active)
		{
			watch.Record(nowMs, position, aim);
			if (!watch.m_bPaused)
				return false;

			watch.m_bPaused = false;
			if (account.IsDutyClockRunning())
				return false;

			account.StartDutyClock(nowMs);
			LF_Debug.Info(FEATURE, string.Format("%1: active again, duty clock resumed", account.GetPersistentId()));
			return true;
		}

		// Idle: keep the old pose, so slow drift still adds up to MOVE_METRES.
		if (!account.IsDutyClockRunning())
			return false;

		// Someone else restarted the clock (a spawn) while this sampler held it paused: the idle
		// window counts again from that start.
		int since = LF_Utils.MaxInt(watch.m_iLastActiveMs, account.GetDutyStartedAtMs());
		watch.m_bPaused = false;
		int pauseAt = since + idleSeconds * 1000;
		if (nowMs < pauseAt)
			return false;

		LF_PoliceRanks.SettleDutyAt(account, false, pauseAt);
		watch.m_bPaused = true;
		LF_Debug.Info(FEATURE, string.Format("%1: idle %2 s on duty, duty clock paused until they move", account.GetPersistentId(), idleSeconds));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a character is and where it looks. In a vehicle the vehicle's position is used, so a
	//! patrol car that moves counts as active for every seat.
	static void ReadPose(IEntity character, out vector position, out vector aim)
	{
		position = vector.Zero;
		aim = vector.Zero;
		if (!character)
			return;

		position = character.GetOrigin();
		IEntity vehicle = SCR_CompartmentAccessComponent.GetVehicleIn(character);
		if (vehicle)
			position = vehicle.GetOrigin();

		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return;

		CharacterControllerComponent controller = chimera.GetCharacterController();
		if (controller && controller.GetAimingComponent())
			aim = controller.GetAimingComponent().GetAimingDirectionWorld();
	}

	//------------------------------------------------------------------------------------------------
	//! Server, once: starts the periodic sampler.
	static void Start()
	{
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		Stop();
		GetGame().GetCallqueue().CallLater(Sweep, SAMPLE_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the sampler and forgets every watch (the world-end reset).
	static void Stop()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Sweep);

		s_mWatches.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Samples every connected player once. Watches of players who left are dropped; their clock
	//! already stopped (LF_PoliceRanks.OnPlayerLeft) and restarts on their next spawn.
	static void Sweep()
	{
		int idleSeconds = IdleSeconds();
		if (!GetGame() || !GetGame().GetWorld() || !GetGame().GetPlayerManager())
			return;

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return;

		int nowMs = Math.Round(GetGame().GetWorld().GetWorldTime());
		PlayerManager playerManager = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);

		map<string, ref LF_DutyIdleWatch> kept = new map<string, ref LF_DutyIdleWatch>();
		foreach (int playerId : playerIds)
		{
			string uid = LF_Utils.GetPlayerUID(playerId);
			if (uid.IsEmpty())
				continue;

			LF_DutyIdleWatch watch = s_mWatches.Get(uid);
			if (!watch)
				watch = new LF_DutyIdleWatch();
			kept.Set(uid, watch);

			// No body (dead, in the spawn menu): nothing to sample this round.
			IEntity character = playerManager.GetPlayerControlledEntity(playerId);
			if (!character)
				continue;

			vector position;
			vector aim;
			ReadPose(character, position, aim);
			SampleAt(accounts.GetAccount(uid), watch, nowMs, position, aim, idleSeconds);
		}

		s_mWatches = kept;
	}
}

//------------------------------------------------------------------------------------------------
//! Self-start: the server samples from the world's start.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_DutyIdle.Start();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_DutyIdle's sampler and watches (LF_StaticReset finds it by reflection).
class LF_DutyIdle_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_DutyIdle";
	}

	override void Apply()
	{
		LF_DutyIdle.Stop();
	}
}
