//------------------------------------------------------------------------------------------------
//! The licence desk (#1766, #1069): a player applies for the firearm permit in person, pays the fee
//! in carried cash, and collects the permit at the same desk once one in-game day has passed. Police
//! on duty revoke it face to face (LF_LicenceRevokeRole).
//!
//! RULES (maintainer, 2026-11-16). The fee is LF_LicencesConfig.PermitFee(). The record must be
//! clean at application and again at collection: wanted level 0, no open crime report
//! (LF_WantedReports) and no conviction in the last LF_LicencesConfig.PermitConvictionDays()
//! in-game days (LF_ConvictionRecord.IsRecent, #1995; 0 days turns that check off). The licence's own gate (the License switch, whitelist, job level) is the
//! licence manager's CanUnlockLicense. The wait is LF_LicencesConfig.PermitWaitDays() in-game days.
//!
//! TIME. The wait is counted on the in-game clock (date and time of day of the vanilla
//! TimeAndWeatherManagerEntity), which vanilla persistence saves, so a restart neither loses nor
//! skips the wait (LF_DayCycleSave). Stored as whole in-game minutes since 1970-01-01.
//!
//! SAVE. The open application is an account flag "permit_due:<minutes>" in the progression store
//! (LF_Progression.NoteFlag, save version 7), so no new payload; the issued permit is the licence
//! list the store already saves. A revoke rewrites that list and drops any open application.
//------------------------------------------------------------------------------------------------
enum LF_ELicenceDesk
{
	ISSUED,
	APPLIED,
	WAITING,
	ALREADY_HELD,
	WANTED,
	OPEN_CRIME,
	CONVICTED,
	NOT_ENOUGH_CASH,
	NOT_ALLOWED,
	NO_CLOCK,
	FAILED
}

//------------------------------------------------------------------------------------------------
class LF_LicenceDesk
{
	static const string FEATURE = "License";
	static const LF_ELicenseType PERMIT = LF_ELicenseType.FIREARM_PERMIT;
	static const string DUE_FLAG = "permit_due:";
	static const string LEDGER_REASON = "licence-fee";
	static const int MINUTES_PER_DAY = 1440;	// lf-fixed: minutes in a calendar day

	//! Tests: the in-game minute to use instead of the clock; -1 reads the clock.
	protected static int s_iNowOverride = -1;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iNowOverride = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: pin the in-game clock (minutes since 1970-01-01); -1 releases it.
	static void SetNowForTest(int minutes)
	{
		s_iNowOverride = minutes;
	}

	//------------------------------------------------------------------------------------------------
	//! Days from 1970-01-01 to a civil date (proleptic Gregorian, H. Hinnant's days_from_civil).
	static int DaysFromCivil(int year, int month, int day)
	{
		if (month <= 2)
			year -= 1;

		int era;
		if (year >= 0)
			era = year / 400;
		else
			era = (year - 399) / 400;

		int yoe = year - era * 400;
		int mp = month + 9;
		if (month > 2)
			mp = month - 3;

		int doy = (153 * mp + 2) / 5 + day - 1;
		int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		return era * 146097 + doe - 719468;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The in-game minute now, or -1 when the world has no clock (logged).
	static int NowMinutes()
	{
		if (s_iNowOverride >= 0)
			return s_iNowOverride;

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		TimeAndWeatherManagerEntity clock;
		if (world)
			clock = world.GetTimeAndWeatherManager();

		if (!clock)
		{
			LF_Debug.Error(FEATURE, "licence desk: the world has no TimeAndWeatherManagerEntity; no wait can be counted");
			return -1;
		}

		int days = DaysFromCivil(clock.GetYear(), clock.GetMonth(), clock.GetDay());
		return days * MINUTES_PER_DAY + (int)(clock.GetTimeOfTheDay() * 60);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The in-game minute the open application is due, or -1 when there is none.
	static int DueMinutes(string uid)
	{
		LF_ProgressionRecord record = LF_Progression.Get(uid);
		if (!record)
			return -1;

		string prefix = LF_Progression.FLAG_PREFIX + DUE_FLAG;
		foreach (string entry : record.m_aTurnedInQuests)
		{
			if (!entry.StartsWith(prefix))
				continue;

			string digits = entry.Substring(prefix.Length(), entry.Length() - prefix.Length());
			if (digits.IsEmpty() || digits.ToInt().ToString() != digits)
			{
				LF_Debug.Error(FEATURE, string.Format("licence desk: the application of %1 has an unreadable due minute '%2'; treated as none", uid, digits));
				return -1;
			}

			return digits.ToInt();
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ClearDue(string uid)
	{
		int due = DueMinutes(uid);
		while (due >= 0)
		{
			LF_Progression.ClearFlag(uid, DUE_FLAG + due.ToString());
			int next = DueMinutes(uid);
			if (next == due)
				return;

			due = next;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: why the record is not clean (wanted, open crime report, recent conviction), or ISSUED (used as "clean") when it is.
	static LF_ELicenceDesk RecordRefusal(string uid)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts)
		{
			LF_PlayerAccount account = accounts.GetAccount(uid);
			if (account && account.GetWantedLevel() > 0)
				return LF_ELicenceDesk.WANTED;
		}

		if (LF_WantedReports.CountFor(uid) > 0)
			return LF_ELicenceDesk.OPEN_CRIME;

		if (LF_ConvictionRecord.IsRecent(uid, LF_LicencesConfig.PermitConvictionDays()))
			return LF_ELicenceDesk.CONVICTED;

		return LF_ELicenceDesk.ISSUED;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the player holds the permit: the body's licences when there is one, else the store.
	static bool Holds(IEntity player, string uid)
	{
		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(player);
		if (licences)
			return licences.HasLicense(PERMIT);

		array<LF_ELicenseType> stored = {};
		LF_Progression.GetLicenses(uid, stored);
		return stored.Contains(PERMIT);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one visit to the desk, by the player's own account. See VisitFor.
	static LF_ELicenceDesk Visit(IEntity player, out int minutesLeft)
	{
		return VisitFor(player, LF_Utils.GetPlayerUID(player), new LF_EntityCashWallet(player, LEDGER_REASON, string.Empty), minutesLeft);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one visit. Collects a due permit, says how long an open application still waits, or
	//! takes a new application (fee once, clean record, the licence's gate). player may be null
	//! (tests, an account with no body): the store is used then.
	//! \param[out] minutesLeft In-game minutes until collection (APPLIED, WAITING).
	static LF_ELicenceDesk VisitFor(IEntity player, string uid, notnull LF_CashWallet wallet, out int minutesLeft)
	{
		minutesLeft = 0;
		if (!Replication.IsServer() || uid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("licence desk refused: server %1, account '%2'", Replication.IsServer(), uid));
			return LF_ELicenceDesk.FAILED;
		}

		if (!LF_Systems.IsEnabled(LF_LicenseGate.SYSTEM))
		{
			LF_Debug.Info(FEATURE, string.Format("licence desk refused for %1: the License system is off", uid));
			return LF_ELicenceDesk.NOT_ALLOWED;
		}

		if (Holds(player, uid))
		{
			ClearDue(uid);
			LF_Debug.Info(FEATURE, string.Format("licence desk: %1 already holds the firearm permit", uid));
			return LF_ELicenceDesk.ALREADY_HELD;
		}

		int now = NowMinutes();
		if (now < 0)
			return LF_ELicenceDesk.NO_CLOCK;

		LF_ELicenceDesk record = RecordRefusal(uid);
		int due = DueMinutes(uid);
		int wait = (int)(LF_LicencesConfig.PermitWaitDays() * MINUTES_PER_DAY);
		if (due >= 0)
		{
			// The clock was set back (a reset date, a restored world): wait one more full term, never years.
			if (due - now > wait)
			{
				ClearDue(uid);
				due = now + wait;
				LF_Progression.NoteFlag(uid, DUE_FLAG + due.ToString());
				LF_Debug.Warn(FEATURE, string.Format("licence desk: the clock is behind the application of %1; due again at in-game minute %2", uid, due));
			}

			if (now < due)
			{
				minutesLeft = due - now;
				LF_Debug.Info(FEATURE, string.Format("licence desk: %1 waits %2 more in-game minutes for the firearm permit", uid, minutesLeft));
				return LF_ELicenceDesk.WAITING;
			}

			if (record != LF_ELicenceDesk.ISSUED)
			{
				LF_Debug.Info(FEATURE, string.Format("licence desk: the permit of %1 is held back at collection: %2", uid, typename.EnumToString(LF_ELicenceDesk, record)));
				return record;
			}

			LF_LicenseManagerComponent gate = LF_Component<LF_LicenseManagerComponent>.Find(player);
			if (gate && !gate.CanUnlockLicense(PERMIT))
			{
				LF_Debug.Info(FEATURE, string.Format("licence desk: the permit of %1 is held back at collection: the licence's gate", uid));
				return LF_ELicenceDesk.NOT_ALLOWED;
			}

			if (!Grant(player, uid))
				return LF_ELicenceDesk.FAILED;

			ClearDue(uid);
			LF_Debug.Info(FEATURE, string.Format("licence desk: firearm permit issued to %1", uid));
			return LF_ELicenceDesk.ISSUED;
		}

		if (record != LF_ELicenceDesk.ISSUED)
		{
			LF_Debug.Info(FEATURE, string.Format("licence desk refused the application of %1: %2", uid, typename.EnumToString(LF_ELicenceDesk, record)));
			return record;
		}

		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(player);
		if (licences && !licences.CanUnlockLicense(PERMIT))
		{
			LF_Debug.Info(FEATURE, string.Format("licence desk refused the application of %1: the licence's gate", uid));
			return LF_ELicenceDesk.NOT_ALLOWED;
		}

		int fee = LF_LicencesConfig.PermitFee();
		if (wallet.Carried() < fee)
		{
			LF_Debug.Info(FEATURE, string.Format("licence desk refused the application of %1: carries %2, the fee is %3", uid, wallet.Carried(), fee));
			return LF_ELicenceDesk.NOT_ENOUGH_CASH;
		}

		if (fee > 0)
		{
			int taken = wallet.Take(fee);
			if (taken != fee)
			{
				if (taken > 0)
					wallet.Put(taken);

				LF_Debug.Error(FEATURE, string.Format("licence desk: the fee of %1 could not be taken from %2 (took %3, given back)", fee, uid, taken));
				return LF_ELicenceDesk.NOT_ENOUGH_CASH;
			}
		}

		if (wait <= 0)
		{
			if (!Grant(player, uid))
			{
				if (fee > 0)
					wallet.Put(fee);

				return LF_ELicenceDesk.FAILED;
			}

			LF_Debug.Info(FEATURE, string.Format("licence desk: firearm permit issued at once to %1 for %2", uid, fee));
			return LF_ELicenceDesk.ISSUED;
		}

		LF_Progression.NoteFlag(uid, DUE_FLAG + (now + wait).ToString());
		minutesLeft = wait;
		LF_Debug.Info(FEATURE, string.Format("licence desk: %1 applied for the firearm permit for %2; due at in-game minute %3", uid, fee, now + wait));
		return LF_ELicenceDesk.APPLIED;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: put the permit on the body and in the saved list (the body may be absent).
	protected static bool Grant(IEntity player, string uid)
	{
		array<LF_ELicenseType> list = {};
		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(player);
		if (licences)
		{
			if (!licences.HasLicense(PERMIT) && !licences.UnlockLicense(PERMIT, true))
			{
				LF_Debug.Error(FEATURE, string.Format("licence desk: the licence manager refused the permit for %1", uid));
				return false;
			}

			list.Copy(licences.GetUnlockedLicenses());
		}
		else
		{
			LF_Progression.GetLicenses(uid, list);
			if (!list.Contains(PERMIT))
				list.Insert(PERMIT);
		}

		LF_Progression.NoteLicenses(uid, list);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an on-duty officer takes the target's permit (and any open application). The target has
	//! to be restrained first (LF_Restraint); a free player is refused and the refusal is logged.
	//! \return True when a permit or an application was taken.
	static bool Revoke(IEntity officer, IEntity target)
	{
		if (!Replication.IsServer() || !LF_PoliceUtils.IsOnDutyCharacter(officer))
		{
			LF_Debug.Warn(FEATURE, "permit revoke refused: only an on-duty officer, on the server");
			return false;
		}

		if (!LF_Restraint.IsRestrained(target))
		{
			LF_Debug.Warn(FEATURE, "permit revoke refused: the target is not restrained");
			return false;
		}

		return RevokeFor(target, LF_PoliceRanks.ResolveUid(target), LF_PoliceRanks.ResolveUid(officer));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take the permit of uid (body optional). The officer check is Revoke's.
	static bool RevokeFor(IEntity target, string uid, string officerUid)
	{
		if (!Replication.IsServer())
			return false;

		if (uid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "permit revoke refused: the target has no player id");
			return false;
		}

		bool hadApplication = DueMinutes(uid) >= 0;
		ClearDue(uid);

		array<LF_ELicenseType> list = {};
		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(target);
		if (licences)
			list.Copy(licences.GetUnlockedLicenses());
		else
			LF_Progression.GetLicenses(uid, list);

		bool held = list.RemoveItem(PERMIT);
		if (!held)
		{
			LF_Debug.Info(FEATURE, string.Format("permit revoke by %1: %2 holds no permit (open application dropped: %3)", officerUid, uid, hadApplication));
			return hadApplication;
		}

		if (licences)
			licences.SetUnlockedLicenses(list);

		LF_Progression.NoteLicenses(uid, list);
		LF_Debug.Info(FEATURE, string.Format("firearm permit of %1 revoked by %2", uid, officerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The desk's answer for an outcome.
	static string AnswerKey(LF_ELicenceDesk outcome)
	{
		switch (outcome)
		{
			case LF_ELicenceDesk.ISSUED: return "#LF-LicenceDesk_Issued";
			case LF_ELicenceDesk.APPLIED: return "#LF-LicenceDesk_Applied";
			case LF_ELicenceDesk.WAITING: return "#LF-LicenceDesk_Waiting";
			case LF_ELicenceDesk.ALREADY_HELD: return "#LF-LicenceDesk_AlreadyHeld";
			case LF_ELicenceDesk.WANTED: return "#LF-LicenceDesk_Wanted";
			case LF_ELicenceDesk.OPEN_CRIME: return "#LF-LicenceDesk_OpenCrime";
			case LF_ELicenceDesk.CONVICTED: return "#LF-LicenceDesk_Convicted";
			case LF_ELicenceDesk.NOT_ENOUGH_CASH: return "#LF-LicenceDesk_NoCash";
			case LF_ELicenceDesk.NOT_ALLOWED: return "#LF-LicenceDesk_NotAllowed";
		}

		return "#LF-LicenceDesk_Closed";
	}
}

//------------------------------------------------------------------------------------------------
//! Police on duty, facing a player who holds the permit: "Revoke firearm permit".
class LF_LicenceRevokeRole : LF_PlayerRoleHandler
{
	static const string SLOT = "revoke_permit";

	//------------------------------------------------------------------------------------------------
	//! Every machine, once per world. Idempotent.
	static void Register()
	{
		if (LF_LicenceRevokeRole.Cast(LF_PlayerInteraction.GetRole(SLOT)))
			return;

		LF_PlayerInteraction.RegisterRole(SLOT, new LF_LicenceRevokeRole());
	}

	//------------------------------------------------------------------------------------------------
	override string GetActionName()
	{
		return "#LF-LicenceDesk_RevokeAction";
	}

	//------------------------------------------------------------------------------------------------
	override bool CanOffer(IEntity user, IEntity target)
	{
		if (!LF_PoliceUtils.IsOnDutyCharacter(user) || !LF_Restraint.IsRestrained(target))
			return false;

		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(target);
		return licences && licences.HasLicense(LF_LicenceDesk.PERMIT);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(IEntity user, IEntity target)
	{
		if (!LF_LicenceDesk.Revoke(user, target))
			return;

		LF_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(user), "#LF-LicenceDesk_Title", "#LF-LicenceDesk_RevokedOfficer", 4.0, LF_ENotificationType.INFO);
		LF_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(target), "#LF-LicenceDesk_Title", "#LF-LicenceDesk_Revoked", 6.0, LF_ENotificationType.WARNING);
	}
}

//------------------------------------------------------------------------------------------------
//! The revoke slot, once per world on every machine.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_LicenceRevokeRole.Register();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LicenceDesk's statics (registered at script load; Core/LF_StaticReset.c).
class LF_LicenceDesk_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_LicenceDesk_StaticResetHook());

	override string Owner()
	{
		return "LF_LicenceDesk";
	}

	override void Apply()
	{
		LF_LicenceDesk.ResetStatics();
	}
}
