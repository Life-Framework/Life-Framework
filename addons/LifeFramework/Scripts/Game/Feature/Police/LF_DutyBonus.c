//------------------------------------------------------------------------------------------------
//! What a duty bonus was paid for (#4210, plan police D4). Each kind has one fixed amount.
enum LF_DutyBonusKind
{
	CALL_RESOLVED,		//!< Police: a dispatch call closed as resolved ($15)
	BOOKING,			//!< Police: a suspect booked at the desk ($10)
	STOLEN_CAR,			//!< Police: a stolen car recovered ($20)
	REVIVE,				//!< Medic: a player revived ($10)
	PATIENT_DELIVERED	//!< Medic: a patient delivered to a hospital bed ($15)
}

//------------------------------------------------------------------------------------------------
//! One duty account's bonus ledger: the bonus waiting for clock-out and the current hour's window.
class LF_DutyBonusLedger : Managed
{
	//! Bonus earned and not yet paid; paid with the shift at clock-out.
	int m_iPending;
	//! Unix second the current hour window opened; 0 before the first bonus.
	int m_iWindowStart;
	//! Bonus earned inside the current hour window, at most the rank's cap.
	int m_iWindowEarned;
}

//------------------------------------------------------------------------------------------------
//! Outcome bonuses for officers and medics (#4210). An outcome adds its fixed amount to the account's
//! pending bonus, up to the rank's hourly cap; the bonus is paid with the shift pay at clock-out
//! (LF_DutyPay.PayAtClockOut), never on the spot. The caps keep shift plus bonus under the next civilian
//! rung (plan "The economy table"): police $100/$100/$100/$95/$70 by rank, medic $40/$50/$50/$40.
//! Fines and bail are not bonuses: they go to the town treasury and never reach this ledger.
//! The ledger lives in memory: a server restart mid-shift drops the unpaid bonus (the shift seconds persist).
class LF_DutyBonus
{
	static const string FEATURE = "DutyBonus";
	//! Ledger reason of the bonus part of the clock-out pay (LF_MoneyUtils).
	static const string REASON = "duty-bonus";
	//! Length of one cap window.
	static const int WINDOW_SECONDS = 3600;	// lf-fixed: the cap is per hour (plan police D4)

	static const int CALL_RESOLVED_AMOUNT = 15;		// lf-fixed: plan "The economy table" bonus row
	static const int BOOKING_AMOUNT = 10;			// lf-fixed: plan "The economy table" bonus row
	static const int STOLEN_CAR_AMOUNT = 20;		// lf-fixed: plan "The economy table" bonus row
	static const int REVIVE_AMOUNT = 10;			// lf-fixed: plan "The economy table" bonus row
	static const int PATIENT_DELIVERED_AMOUNT = 15;	// lf-fixed: plan "The economy table" bonus row

	protected static ref map<string, ref LF_DutyBonusLedger> s_mLedgers;
	protected static int s_iRefusals;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mLedgers = null;
		s_iRefusals = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Awards refused (off duty, wrong role, unknown kind, cap reached) since the last reset.
	static int GetRefusalCount()
	{
		return s_iRefusals;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_DutyBonusLedger> Ledgers()
	{
		if (!s_mLedgers)
			s_mLedgers = new map<string, ref LF_DutyBonusLedger>();

		return s_mLedgers;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the fixed amount of an outcome; 0 for anything else.
	static int AmountFor(LF_DutyBonusKind kind)
	{
		switch (kind)
		{
			case LF_DutyBonusKind.CALL_RESOLVED:
				return CALL_RESOLVED_AMOUNT;
			case LF_DutyBonusKind.BOOKING:
				return BOOKING_AMOUNT;
			case LF_DutyBonusKind.STOLEN_CAR:
				return STOLEN_CAR_AMOUNT;
			case LF_DutyBonusKind.REVIVE:
				return REVIVE_AMOUNT;
			case LF_DutyBonusKind.PATIENT_DELIVERED:
				return PATIENT_DELIVERED_AMOUNT;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the role an outcome belongs to.
	static LF_Faction RoleFor(LF_DutyBonusKind kind)
	{
		if (kind == LF_DutyBonusKind.REVIVE || kind == LF_DutyBonusKind.PATIENT_DELIVERED)
			return LF_Faction.MEDIC;

		return LF_Faction.POLICE;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a police rank's hourly bonus cap. Ranks past the table keep the last row.
	static int PoliceCap(int rank)
	{
		array<int> caps = {100, 100, 100, 95, 70};	// lf-fixed: plan "The economy table", mirrored by tools/lib/economy-lumber.mjs bonusCaps
		if (rank < 0)
			return 0;

		if (rank >= caps.Count())
			return caps[caps.Count() - 1];

		return caps[rank];
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a medic rank's hourly bonus cap. Ranks past the table keep the last row.
	static int MedicCap(int rank)
	{
		return LF_MedicDutyBonus.Cap(rank);
	}

	//------------------------------------------------------------------------------------------------
	//! The hourly cap of this account's role and rank; 0 for a civilian.
	static int CapFor(LF_PlayerAccount account)
	{
		if (!account)
			return 0;

		LF_Faction role = account.GetFaction();
		if (role == LF_Faction.POLICE)
			return PoliceCap(account.GetPoliceRank());

		if (role == LF_Faction.MEDIC)
			return MedicCap(account.GetMedicRank());

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: awards an outcome bonus to an on-duty account at the current time.
	//! \return The amount added to the pending bonus (0 when refused or capped).
	static int Award(LF_PlayerAccount account, LF_DutyBonusKind kind)
	{
		return AwardAt(account, kind, System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Awards an outcome bonus at unix second `now`. The amount is cut to what the hour window has left
	//! under the rank's cap; the window opens at the first bonus and runs WINDOW_SECONDS.
	//! \return The amount added to the pending bonus (0 when refused or capped).
	static int AwardAt(LF_PlayerAccount account, LF_DutyBonusKind kind, int now)
	{
		if (!account)
		{
			s_iRefusals++;
			LF_Debug.Error(FEATURE, "bonus refused: no account");
			return 0;
		}

		string id = account.GetPersistentId();
		int amount = AmountFor(kind);
		if (amount <= 0)
		{
			s_iRefusals++;
			LF_Debug.Warn(FEATURE, string.Format("bonus refused for %1: unknown outcome %2", id, kind));
			return 0;
		}

		if (!account.IsOnDuty() || account.GetFaction() != RoleFor(kind))
		{
			s_iRefusals++;
			LF_Debug.Info(FEATURE, string.Format("bonus refused for %1: %2 needs an on-duty %3", id, typename.EnumToString(LF_DutyBonusKind, kind), typename.EnumToString(LF_Faction, RoleFor(kind))));
			return 0;
		}

		LF_DutyBonusLedger ledger = Ledgers().Get(id);
		if (!ledger)
		{
			ledger = new LF_DutyBonusLedger();
			Ledgers().Set(id, ledger);
		}

		if (ledger.m_iWindowStart <= 0 || now - ledger.m_iWindowStart >= WINDOW_SECONDS || now < ledger.m_iWindowStart)
		{
			ledger.m_iWindowStart = now;
			ledger.m_iWindowEarned = 0;
		}

		int cap = CapFor(account);
		int room = LF_Utils.MaxInt(cap - ledger.m_iWindowEarned, 0);
		int added = LF_Utils.MinInt(amount, room);
		if (added <= 0)
		{
			s_iRefusals++;
			LF_Debug.Info(FEATURE, string.Format("bonus capped for %1: $%2 of $%3 this hour already earned", id, ledger.m_iWindowEarned, cap));
			return 0;
		}

		ledger.m_iWindowEarned += added;
		ledger.m_iPending += added;
		LF_Debug.Info(FEATURE, string.Format("bonus +$%1 (%2) for %3: $%4 of $%5 this hour, $%6 pending", added, typename.EnumToString(LF_DutyBonusKind, kind), id, ledger.m_iWindowEarned, cap, ledger.m_iPending));
		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! The bonus waiting for this account's clock-out.
	static int Pending(LF_PlayerAccount account)
	{
		if (!account || !s_mLedgers)
			return 0;

		LF_DutyBonusLedger ledger = s_mLedgers.Get(account.GetPersistentId());
		if (!ledger)
			return 0;

		return ledger.m_iPending;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the pending bonus for payment; a second call returns 0. The hour window stays, so clocking out
	//! and back in does not reopen the cap.
	static int TakePending(LF_PlayerAccount account)
	{
		if (!account || !s_mLedgers)
			return 0;

		LF_DutyBonusLedger ledger = s_mLedgers.Get(account.GetPersistentId());
		if (!ledger)
			return 0;

		int pending = ledger.m_iPending;
		ledger.m_iPending = 0;
		return pending;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes back an award that a later outcome replaces (a bed delivery's revive, LF_MedicDutyBonus): the
	//! pending bonus and the hour's count both drop, never below 0.
	static void Refund(LF_PlayerAccount account, int amount)
	{
		if (!account || amount <= 0 || !s_mLedgers)
			return;

		LF_DutyBonusLedger ledger = s_mLedgers.Get(account.GetPersistentId());
		if (!ledger)
			return;

		ledger.m_iPending = LF_Utils.MaxInt(ledger.m_iPending - amount, 0);
		ledger.m_iWindowEarned = LF_Utils.MaxInt(ledger.m_iWindowEarned - amount, 0);
		LF_Debug.Info(FEATURE, string.Format("bonus -$%1 for %2: replaced by a later outcome, $%3 pending", amount, account.GetPersistentId(), ledger.m_iPending));
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a taken bonus back when the clock-out could not pay it (no body, a full inventory).
	static void Restore(LF_PlayerAccount account, int amount)
	{
		if (!account || amount <= 0)
			return;

		LF_DutyBonusLedger ledger = Ledgers().Get(account.GetPersistentId());
		if (!ledger)
		{
			ledger = new LF_DutyBonusLedger();
			Ledgers().Set(account.GetPersistentId(), ledger);
		}

		ledger.m_iPending += amount;
		LF_Debug.Warn(FEATURE, string.Format("bonus $%1 kept for %2's next clock-out", amount, account.GetPersistentId()));
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_DutyBonus's statics (LF_StaticReset finds it by reflection).
class LF_DutyBonus_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_DutyBonus";
	}

	override void Apply()
	{
		LF_DutyBonus.ResetStatics();
	}
}
