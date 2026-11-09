//------------------------------------------------------------------------------------------------
//! Police duty pay. The duty clock (LF_PoliceRanks) settles on-duty seconds onto the account;
//! going off duty at the station desk pays those seconds times the configured rate in cash,
//! once. There is no timer paycheck: civilians earn from completed runs and trader sales.
class LF_DutyPay
{
	//------------------------------------------------------------------------------------------------
	//! Configured cash per on-duty second. A missing config pays nothing and logs once.
	static float PoliceRatePerSecond()
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config)
			return 0;

		return config.m_fPoliceDutyPayPerSecond;
	}

	//------------------------------------------------------------------------------------------------
	//! Configured cash per on-duty second for a duty role.
	static float RateFor(LF_Faction role)
	{
		if (role != LF_Faction.MEDIC)
			return PoliceRatePerSecond();

		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config)
			return 0;

		return config.m_fMedicDutyPayPerSecond;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: cash owed for seconds at rate, rounded down. Non-positive input owes nothing.
	static int Owed(int seconds, float rate)
	{
		if (seconds <= 0 || rate <= 0)
			return 0;

		return Math.Floor(seconds * rate);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the account's unpaid duty seconds and returns the cash owed for them. The seconds
	//! are zeroed, so a second call returns 0. A non-police account is owed nothing.
	static int TakeShiftPay(LF_PlayerAccount account, float rate)
	{
		if (!account)
		{
			LF_Debug.Error("DutyPay", "shift pay refused: no account");
			return 0;
		}

		int seconds = account.TakeUnpaidDutySeconds();
		if (account.GetFaction() == LF_Faction.CIVILIAN)
		{
			LF_Debug.Warn("DutyPay", string.Format("shift pay refused: %1 is not on a duty role", account.GetPersistentId()));
			return 0;
		}

		int owed = Owed(seconds, rate);
		if (owed <= 0)
			LF_Debug.Info("DutyPay", string.Format("no shift pay for %1 (%2 s unpaid)", account.GetPersistentId(), seconds));

		return owed;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: hands the officer the shift's duty pay as cash, with the pick-up sound first and
	//! a toast second. Call after the duty clock settled the clock-out.
	//! \return Cash placed on the officer.
	static int PayAtClockOut(LF_PlayerAccount account, IEntity officer)
	{
		if (!Replication.IsServer() && Replication.IsRunning())
			return 0;

		int owed = TakeShiftPay(account, RateFor(account.GetFaction()));
		if (owed <= 0)
			return 0;

		if (!officer)
		{
			LF_Debug.Error("DutyPay", string.Format("shift pay of %1 lost: no officer entity for %2", owed, account.GetPersistentId()));
			return 0;
		}

		int given = LF_MoneyUtils.GiveCash(officer, owed, "duty-pay");
		if (given != owed)
			LF_Debug.Warn("DutyPay", string.Format("shift pay short: paid %1 of %2 (player %3)", given, owed, officer));
		else
			LF_Debug.Info("DutyPay", string.Format("shift pay +%1 cash to %2", given, account.GetPersistentId()));

		if (given <= 0)
			return 0;

		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(officer);
		if (inventory)
			inventory.RpcAsk_PlaySound(LF_NetworkUtils.GetRplId(officer), "SOUND_PICK_UP");

		LF_Utils.NotifyEntity(officer, "#EL-Duty_Paid", "#EL-Duty_Status", 4.0, given.ToString());
		return given;
	}
}
