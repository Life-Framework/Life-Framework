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
	//! Rate for this account. An officer's rank row may set its own rate; a negative rate defers to RateFor.
	static float RateForAccount(LF_PlayerAccount account)
	{
		if (!account)
			return 0;

		if (account.GetFaction() == LF_Faction.POLICE)
		{
			float rankRate = LF_PoliceRanks.PayPerSecond(account.GetPoliceRank());
			if (rankRate >= 0)
				return rankRate;
		}

		float medicRate = LF_MedicRanks.PayPerSecondFor(account);
		if (medicRate >= 0)
			return medicRate;

		return RateFor(account.GetFaction());
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
	//! Pure (#2874): how many unpaid seconds at newRate are worth `seconds` at oldRate, rounded up so
	//! the shift is never paid less than its part at the old rank (over by under one second's pay).
	//! Seconds at a rate of 0 are worth nothing. With newRate 0 the value cannot be carried, so the
	//! seconds stay as they are.
	static int CarrySeconds(int seconds, float oldRate, float newRate)
	{
		if (seconds <= 0 || oldRate <= 0)
			return 0;

		if (newRate <= 0 || oldRate == newRate)
			return seconds;

		float carried = seconds * oldRate / newRate;
		return Math.Ceil(carried - 0.0001);
	}

	//------------------------------------------------------------------------------------------------
	//! Before a rank change (#2874): the rate the account is paid at now. The caller has settled the
	//! running duty clock, so the unpaid seconds are all from the old rank.
	static float RateBeforeRankChange(LF_PlayerAccount account)
	{
		return RateForAccount(account);
	}

	//------------------------------------------------------------------------------------------------
	//! After a rank change (#2874): the unpaid seconds earned at oldRate are restated at the new
	//! rank's rate, so clock-out pays each part of the shift at the rank held during it.
	//! \return The unpaid seconds now on the account.
	static int AfterRankChange(LF_PlayerAccount account, float oldRate)
	{
		if (!account)
			return 0;

		int unpaid = account.GetUnpaidDutySeconds();
		if (unpaid <= 0)
			return 0;

		float newRate = RateForAccount(account);
		int carried = CarrySeconds(unpaid, oldRate, newRate);
		if (newRate <= 0 && oldRate > 0)
			LF_Debug.Warn("DutyPay", string.Format("%1: rank change to an unpaid rank; %2 unpaid s kept as they are", account.GetPersistentId(), unpaid));

		if (carried == unpaid)
			return unpaid;

		account.SetUnpaidDutySeconds(carried);
		LF_Debug.Info("DutyPay", string.Format("%1: rank change settled the shift: %2 unpaid s at rate %3 carried as %4 s at rate %5", account.GetPersistentId(), unpaid, oldRate, carried, newRate));
		return carried;
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
	//! Takes the account's pending outcome bonus (LF_DutyBonus, #4210) for the clock-out. A civilian is owed
	//! none. Only shift pay and capped bonuses reach the officer: fines and bail go to the town treasury.
	static int TakeBonus(LF_PlayerAccount account)
	{
		if (!account || account.GetFaction() == LF_Faction.CIVILIAN)
			return 0;

		return LF_DutyBonus.TakePending(account);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: hands the officer the shift's duty pay and outcome bonus as cash, with the pick-up sound first and
	//! a toast second. Call after the duty clock settled the clock-out.
	//! \return Cash placed on the officer.
	static int PayAtClockOut(LF_PlayerAccount account, IEntity officer)
	{
		if (!Replication.IsServer() && Replication.IsRunning())
			return 0;

		int owed = TakeShiftPay(account, RateForAccount(account));
		int bonus = TakeBonus(account);
		if (owed <= 0 && bonus <= 0)
			return 0;

		if (!officer)
		{
			LF_Debug.Error("DutyPay", string.Format("shift pay of %1 lost: no officer entity for %2", owed, account.GetPersistentId()));
			LF_DutyBonus.Restore(account, bonus);
			return 0;
		}

		int given = 0;
		if (owed > 0)
		{
			given = LF_MoneyUtils.GiveCash(officer, owed, "duty-pay");
			if (given != owed)
				LF_Debug.Warn("DutyPay", string.Format("shift pay short: paid %1 of %2 (player %3)", given, owed, officer));
			else
				LF_Debug.Info("DutyPay", string.Format("shift pay +%1 cash to %2", given, account.GetPersistentId()));
		}

		if (bonus > 0)
		{
			int bonusGiven = LF_MoneyUtils.GiveCash(officer, bonus, LF_DutyBonus.REASON);
			if (bonusGiven < bonus)
				LF_DutyBonus.Restore(account, bonus - LF_Utils.MaxInt(bonusGiven, 0));

			if (bonusGiven > 0)
			{
				given += bonusGiven;
				LF_Debug.Info("DutyPay", string.Format("duty bonus +%1 cash to %2", bonusGiven, account.GetPersistentId()));
			}
		}

		if (given <= 0)
			return 0;

		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(officer);
		if (inventory)
			inventory.RpcAsk_PlaySound(LF_NetworkUtils.GetRplId(officer), "SOUND_PICK_UP");

		LF_Utils.NotifyEntity(officer, "#EL-Duty_Paid", "#EL-Duty_Status", 4.0, given.ToString());
		return given;
	}
}
