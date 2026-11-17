//------------------------------------------------------------------------------------------------
//! A trial that runs out ends an officer's police powers (#2416). LF_PoliceApplications.ExpireTrials
//! only drops the roster row, and nothing else reads the roster while an officer is on duty, so an
//! officer whose trial expired mid-shift kept the badge. The server sweeps every
//! LF_PoliceConfig.TrialSweepMs(): it expires trials, then clocks out each connected on-duty officer
//! whose trial ended unpromoted, through the duty desk's own clock-out (shift pay, gear swap).
//! An officer who is offline is clocked out by the first sweep after they return.
class LF_PoliceTrialSweep
{
	static const string FEATURE = "Police";

	//------------------------------------------------------------------------------------------------
	//! Server, once: starts the periodic sweep.
	static void Start()
	{
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		Stop();
		GetGame().GetCallqueue().CallLater(Sweep, LF_PoliceConfig.TrialSweepMs(), true);
	}

	//------------------------------------------------------------------------------------------------
	static void Sweep()
	{
		SweepAt(System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the periodic sweep (the world-end reset).
	static void Stop()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Sweep);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: expires the trials that ended by now and clocks out the connected officers they ended.
	//! eturn How many officers were clocked out.
	static int SweepAt(int now)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!playerManager || !accounts)
			return 0;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		array<LF_PlayerAccount> online = {};
		foreach (int playerId : playerIds)
		{
			online.Insert(accounts.GetAccount(LF_Utils.GetPlayerUID(playerId)));
		}

		ExpireAt(now, online);

		int ended = 0;
		for (int i = 0; i < playerIds.Count(); i++)
		{
			if (EndIfExpired(playerIds[i], online[i], playerManager.GetPlayerControlledEntity(playerIds[i])))
				ended++;
		}

		return ended;
	}

	//------------------------------------------------------------------------------------------------
	//! Settles the running duty clock of each connected officer, then expires the trials that ended
	//! by now. The order matters: seconds an officer already earned may be the promotion that ends
	//! their trial (LF_PoliceRanks.AccrueDuty), and an expiry first would take the roster row before
	//! the promotion lands.
	//! eturn How many officers left the roster.
	static int ExpireAt(int now, notnull array<LF_PlayerAccount> online)
	{
		foreach (LF_PlayerAccount account : online)
		{
			LF_PoliceRanks.SettleNow(account);
		}

		return LF_PoliceApplications.ExpireTrials(now);
	}

	//------------------------------------------------------------------------------------------------
	//! Rule: an on-duty police account whose trial ended unpromoted and who may no longer clock in
	//! (the roster row is gone; an admin who put them back on the roster keeps them on duty).
	static bool ShouldEnd(LF_PlayerAccount account)
	{
		if (!account || account.GetFaction() != LF_Faction.POLICE || !account.IsOnDuty())
			return false;

		string uid = account.GetPersistentId();
		if (!LF_PoliceApplications.TrialExpiredFor(uid))
			return false;

		return !LF_WhitelistManager.MayJoinFaction(uid, LF_EFactionType.POLICE);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: ends the shift of an officer whose trial ran out, through the duty desk's own
	//! clock-out (shift pay, gear swap). With no living body (the spawn screen) or a refused
	//! clock-out nothing is changed: the gear swap needs the body and the pay needs a pocket, so the
	//! next sweep tries again. Death already ends a shift (LF_DutyLocker.OnDied).
	//! eturn True when the officer was taken off duty.
	static bool EndIfExpired(int playerId, LF_PlayerAccount account, IEntity body)
	{
		if (!ShouldEnd(account))
			return false;

		string uid = account.GetPersistentId();
		if (!body)
		{
			LF_Debug.Log(FEATURE, string.Format("trial of %1 ended on duty: no body to clock out yet", uid));
			return false;
		}

		string refusal = LF_DutyLocker.ClockOut(account, body, LF_Faction.POLICE);
		if (!refusal.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("trial of %1 ended on duty: desk clock-out refused (%2), tried again at the next sweep", uid, refusal));
			return false;
		}

		LF_Debug.Info(FEATURE, string.Format("trial of %1 ended on duty: clocked out", uid));
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", "#LF-Duty_TrialExpired", 6.0, LF_ENotificationType.WARNING);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PoliceTrialSweep's timer (registered at script load; Core/LF_StaticReset.c).
class LF_PoliceTrialSweep_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_PoliceTrialSweep_StaticResetHook());

	override string Owner()
	{
		return "LF_PoliceTrialSweep";
	}

	override void Apply()
	{
		LF_PoliceTrialSweep.Stop();
	}
}
