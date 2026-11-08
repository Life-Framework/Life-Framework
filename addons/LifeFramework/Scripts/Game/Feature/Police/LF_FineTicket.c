//------------------------------------------------------------------------------------------------
//! The fine as a paper ticket (M1-19, decision 2026-11-08 "A fine is a paper ticket paid at the
//! police desk"). An on-duty officer facing a player writes a ticket (LF_FineTicketRole, the
//! `fine` slot of the player interaction menu): one offence from the config list, at its fixed
//! amount, within the officer's rank cap. The holder carries the ticket item
//! (Prefabs/Items/Roleplay/FineTicket.et) and pays it in carried cash at the punch clock on the
//! police station desk. Never from the bank.
//!
//! DEADLINES COUNT IN ONLINE TIME, like the jail sentence: a holder who logs off does not run up
//! a doubled fine or a warrant while away. Unpaid at the first deadline, the amount is
//! multiplied once (flagship x2). Unpaid at the second, the holder gets +1 wanted and an
//! "Unpaid fine" poster on the wanted board. The ticket stays payable after both; paying it does
//! not take the wanted level back down (that is the arrest's job).
//!
//! The server owns every number: the record (LF_FineTicketRecord) is the truth, the item is its
//! printed copy. PERSISTENCE: the records are the "tickets" payload (save version 12). A carried
//! ticket comes back blank after a restart (the snapshot keeps the prefab, not the print);
//! OnPlayerSpawned prints it again from the record, and grants a fresh copy of an unpaid ticket
//! the holder no longer carries (death, a lost item).
//------------------------------------------------------------------------------------------------
class LF_FineTicketRecord : Managed
{
	int m_iNumber;
	string m_sHolderUid;
	string m_sHolderName;
	string m_sOfficerUid;
	string m_sOfficerName;
	int m_iOfficerRank;
	//! Localization key of the offence (LF_TicketOffence.m_sName).
	string m_sOffence;
	//! The amount written on the ticket.
	int m_iBaseAmount;
	//! The amount owed now (raised once at the first deadline).
	int m_iAmount;
	//! Online seconds since the ticket was written.
	float m_fOnlineSeconds;
	//! LF_FineTicketRules.STAGE_*.
	int m_iStage;
	bool m_bPaid;
	//! In-game date and time the ticket was written, DD/MM/YYYY HH:MM.
	string m_sIssued;
	string m_sPaidAt;

	//------------------------------------------------------------------------------------------------
	LF_FineTicketRecord Copy()
	{
		LF_FineTicketRecord copy = new LF_FineTicketRecord();
		copy.m_iNumber = m_iNumber;
		copy.m_sHolderUid = m_sHolderUid;
		copy.m_sHolderName = m_sHolderName;
		copy.m_sOfficerUid = m_sOfficerUid;
		copy.m_sOfficerName = m_sOfficerName;
		copy.m_iOfficerRank = m_iOfficerRank;
		copy.m_sOffence = m_sOffence;
		copy.m_iBaseAmount = m_iBaseAmount;
		copy.m_iAmount = m_iAmount;
		copy.m_fOnlineSeconds = m_fOnlineSeconds;
		copy.m_iStage = m_iStage;
		copy.m_bPaid = m_bPaid;
		copy.m_sIssued = m_sIssued;
		copy.m_sPaidAt = m_sPaidAt;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
enum LF_ETicketRefusal
{
	NONE,
	NOT_OFFICER,
	NO_HOLDER,
	SELF,
	BAD_OFFENCE,
	ABOVE_CAP,
	TOO_MANY_OPEN
}

//------------------------------------------------------------------------------------------------
enum LF_ETicketPay
{
	PAID,
	NO_TICKET,
	NOT_ENOUGH_CASH,
	FAILED_TAKE
}

//------------------------------------------------------------------------------------------------
//! Pure rules, for LOGIC tests.
class LF_FineTicketRules
{
	static const int STAGE_OPEN = 0;
	static const int STAGE_LATE = 1;
	static const int STAGE_WANTED = 2;

	//------------------------------------------------------------------------------------------------
	//! The stage an unpaid ticket has reached after this much online time.
	static int StageFor(float onlineSeconds, int firstDeadline, int secondDeadline)
	{
		if (onlineSeconds >= firstDeadline + secondDeadline)
			return STAGE_WANTED;

		if (onlineSeconds >= firstDeadline)
			return STAGE_LATE;

		return STAGE_OPEN;
	}

	//------------------------------------------------------------------------------------------------
	//! The amount after the first deadline, rounded to whole cash. Never below the written amount.
	static int LateAmount(int baseAmount, float multiplier)
	{
		int late = Math.Round(baseAmount * multiplier);
		if (late < baseAmount)
			return baseAmount;

		return late;
	}

	//------------------------------------------------------------------------------------------------
	//! Online seconds until the next deadline; 0 once both have passed.
	static float SecondsToNext(float onlineSeconds, int firstDeadline, int secondDeadline)
	{
		int stage = StageFor(onlineSeconds, firstDeadline, secondDeadline);
		if (stage == STAGE_OPEN)
			return firstDeadline - onlineSeconds;

		if (stage == STAGE_LATE)
			return firstDeadline + secondDeadline - onlineSeconds;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whole minutes, rounded up: 61 seconds reads 2, 60 reads 1, 0 reads 0.
	static int MinutesLeft(float seconds)
	{
		if (seconds <= 0)
			return 0;

		int minutes = Math.Ceil(seconds / 60.0);
		return minutes;
	}

	//------------------------------------------------------------------------------------------------
	//! Can an officer write this offence? Rank cap and the open-ticket limit, no entities.
	static LF_ETicketRefusal CheckWrite(bool onDutyOfficer, int fineCap, int offenceAmount, bool offenceValid, bool hasHolder, bool self, int openCount, int maxOpen)
	{
		if (!onDutyOfficer)
			return LF_ETicketRefusal.NOT_OFFICER;

		if (!hasHolder)
			return LF_ETicketRefusal.NO_HOLDER;

		if (self)
			return LF_ETicketRefusal.SELF;

		if (!offenceValid || offenceAmount <= 0)
			return LF_ETicketRefusal.BAD_OFFENCE;

		if (offenceAmount > fineCap)
			return LF_ETicketRefusal.ABOVE_CAP;

		if (openCount >= maxOpen)
			return LF_ETicketRefusal.TOO_MANY_OPEN;

		return LF_ETicketRefusal.NONE;
	}
}

//------------------------------------------------------------------------------------------------
//! Server store of tickets and their online clocks. Records are kept oldest first.
class LF_FineTickets
{
	static const string FEATURE = "Ticket";
	static const string OFFENCE_UNPAID = "#LF-Offence_UnpaidFine";
	//! A holder carries at most this many unpaid tickets; one more is refused.
	static const int MAX_OPEN_PER_HOLDER = 3;
	//! Paid tickets kept per holder, so a receipt reprints after a restart.
	static const int KEEP_PAID_PER_HOLDER = 3;
	static const int TICK_MS = 1000;

	protected static ref array<ref LF_FineTicketRecord> s_aTickets;
	protected static int s_iLastNumber;
	protected static bool s_bTicking;
	//! Bodies without a player controller (WORLD test fixtures), by holder uid.
	protected static ref map<string, EntityID> s_mBoundBodies;

	protected static bool s_bConfigured;
	protected static int s_iFirstDeadline;
	protected static int s_iSecondDeadline;
	protected static float s_fLateMultiplier;
	protected static int s_iLateWanted;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_FineTicketRecord> Tickets()
	{
		if (!s_aTickets)
			s_aTickets = new array<ref LF_FineTicketRecord>();

		return s_aTickets;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(OnTick);

		s_bTicking = false;
		s_aTickets = null;
		s_mBoundBodies = null;
		s_iLastNumber = 0;
		s_bConfigured = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: treat body as the holder's character (a fixture has no player id). Null unbinds.
	static void BindBody(string holderUid, IEntity body)
	{
		if (!s_mBoundBodies)
			s_mBoundBodies = new map<string, EntityID>();

		if (!body)
		{
			s_mBoundBodies.Remove(holderUid);
			return;
		}

		s_mBoundBodies.Set(holderUid, body.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! The holder's character: a connected player, or a bound test body. Null when neither.
	static IEntity BodyOf(string holderUid, out EL_PlayerAccount outAccount)
	{
		outAccount = null;
		EntityID bound;
		if (s_mBoundBodies && s_mBoundBodies.Find(holderUid, bound) && GetGame() && GetGame().GetWorld())
		{
			IEntity body = GetGame().GetWorld().FindEntityByID(bound);
			if (body)
				return body;
		}

		return EL_PoliceUtils.FindPlayerEntityByUid(holderUid, outAccount);
	}

	//------------------------------------------------------------------------------------------------
	//! Deadlines and the late rule. Tests call it with short deadlines; otherwise the config loads.
	static void Configure(int firstDeadline, int secondDeadline, float lateMultiplier, int lateWanted)
	{
		if (firstDeadline < 1)
			firstDeadline = 1;

		if (secondDeadline < 1)
			secondDeadline = 1;

		if (lateMultiplier < 1)
			lateMultiplier = 1;

		if (lateWanted < 0)
			lateWanted = 0;

		s_iFirstDeadline = firstDeadline;
		s_iSecondDeadline = secondDeadline;
		s_fLateMultiplier = lateMultiplier;
		s_iLateWanted = lateWanted;
		s_bConfigured = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops a Configure override; the next read loads LF_Police.conf again.
	static void UseConfig()
	{
		s_bConfigured = false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureConfigured()
	{
		if (s_bConfigured)
			return;

		Configure(LF_PoliceConfig.TicketFirstDeadline(), LF_PoliceConfig.TicketSecondDeadline(), LF_PoliceConfig.TicketLateMultiplier(), LF_PoliceConfig.TicketLateWanted());
	}

	//------------------------------------------------------------------------------------------------
	static int FirstDeadline()
	{
		EnsureConfigured();
		return s_iFirstDeadline;
	}

	//------------------------------------------------------------------------------------------------
	static int SecondDeadline()
	{
		EnsureConfigured();
		return s_iSecondDeadline;
	}

	//------------------------------------------------------------------------------------------------
	static float LateMultiplier()
	{
		EnsureConfigured();
		return s_fLateMultiplier;
	}

	//------------------------------------------------------------------------------------------------
	//! Online seconds until this ticket's next deadline.
	static float SecondsToNext(LF_FineTicketRecord record)
	{
		if (!record || record.m_bPaid)
			return 0;

		return LF_FineTicketRules.SecondsToNext(record.m_fOnlineSeconds, FirstDeadline(), SecondDeadline());
	}

	//------------------------------------------------------------------------------------------------
	//! The amount the ticket rises to at the first deadline.
	static int LateAmountOf(LF_FineTicketRecord record)
	{
		if (!record)
			return 0;

		return LF_FineTicketRules.LateAmount(record.m_iBaseAmount, LateMultiplier());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: records a written ticket. The seam the officer path and tests share.
	//! \return The stored record, or null with no holder or a non-positive amount.
	static LF_FineTicketRecord Issue(string holderUid, string holderName, string officerUid, string officerName, int officerRank, string offence, int amount, string issued)
	{
		if (holderUid.IsEmpty() || amount <= 0)
		{
			EL_Debug.Error(FEATURE, string.Format("ticket not recorded: holder '%1' amount %2", holderUid, amount));
			return null;
		}

		LF_FineTicketRecord record = new LF_FineTicketRecord();
		s_iLastNumber++;
		record.m_iNumber = s_iLastNumber;
		record.m_sHolderUid = holderUid;
		record.m_sHolderName = holderName;
		record.m_sOfficerUid = officerUid;
		record.m_sOfficerName = officerName;
		record.m_iOfficerRank = officerRank;
		record.m_sOffence = offence;
		record.m_iBaseAmount = amount;
		record.m_iAmount = amount;
		record.m_iStage = LF_FineTicketRules.STAGE_OPEN;
		record.m_sIssued = issued;
		Tickets().Insert(record);
		EL_Debug.Info(FEATURE, string.Format("ticket #%1 written: %2 to %3 by %4, %5, due in %6 s online", record.m_iNumber, offence, holderUid, officerUid, amount, FirstDeadline()));
		return record;
	}

	//------------------------------------------------------------------------------------------------
	static LF_FineTicketRecord Find(int number)
	{
		foreach (LF_FineTicketRecord record : Tickets())
		{
			if (record.m_iNumber == number)
				return record;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Every ticket of the holder, paid or not, oldest first.
	static void CollectFor(string holderUid, notnull array<LF_FineTicketRecord> outRecords, bool unpaidOnly)
	{
		outRecords.Clear();
		foreach (LF_FineTicketRecord record : Tickets())
		{
			if (record.m_sHolderUid != holderUid)
				continue;

			if (unpaidOnly && record.m_bPaid)
				continue;

			outRecords.Insert(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	static int CountOpen(string holderUid)
	{
		array<LF_FineTicketRecord> open = {};
		CollectFor(holderUid, open, true);
		return open.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The oldest unpaid ticket of the holder, or null.
	static LF_FineTicketRecord OldestOpen(string holderUid)
	{
		foreach (LF_FineTicketRecord record : Tickets())
		{
			if (record.m_sHolderUid == holderUid && !record.m_bPaid)
				return record;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server rule for one write, from the officer's account. outOffence is the chosen line.
	static LF_ETicketRefusal CheckWrite(EL_PlayerAccount officer, string officerUid, string holderUid, int offenceIndex, out LF_TicketOffence outOffence)
	{
		array<ref LF_TicketOffence> offences = {};
		LF_PoliceConfig.TicketOffences(offences);
		bool valid = offenceIndex >= 0 && offenceIndex < offences.Count();
		int amount = 0;
		outOffence = null;
		if (valid)
		{
			outOffence = offences.Get(offenceIndex);
			amount = outOffence.m_iAmount;
		}

		int cap = 0;
		bool onDuty = EL_PoliceUtils.IsOnDutyOfficer(officer);
		if (officer)
			cap = LF_PoliceRanks.FineCap(officer.GetPoliceRank());

		return LF_FineTicketRules.CheckWrite(onDuty, cap, amount, valid, !holderUid.IsEmpty(), holderUid == officerUid, CountOpen(holderUid), MAX_OPEN_PER_HOLDER);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: advances every unpaid ticket whose holder is online. Public for LOGIC tests.
	static void TickOnline(float elapsedSeconds, notnull array<string> onlineUids)
	{
		if (elapsedSeconds <= 0)
			return;

		array<LF_FineTicketRecord> due = {};
		foreach (LF_FineTicketRecord record : Tickets())
		{
			if (record.m_bPaid || record.m_iStage >= LF_FineTicketRules.STAGE_WANTED)
				continue;

			if (onlineUids.Find(record.m_sHolderUid) < 0)
				continue;

			due.Insert(record);
		}

		foreach (LF_FineTicketRecord record : due)
		{
			Advance(record, elapsedSeconds);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Advance(LF_FineTicketRecord record, float elapsedSeconds)
	{
		int shownBefore = LF_FineTicketRules.MinutesLeft(SecondsToNext(record));
		record.m_fOnlineSeconds = record.m_fOnlineSeconds + elapsedSeconds;
		int reached = LF_FineTicketRules.StageFor(record.m_fOnlineSeconds, FirstDeadline(), SecondDeadline());
		bool changed = false;
		while (record.m_iStage < reached)
		{
			record.m_iStage = record.m_iStage + 1;
			ApplyStage(record);
			changed = true;
		}

		if (changed || LF_FineTicketRules.MinutesLeft(SecondsToNext(record)) != shownBefore)
			Reprint(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: what a deadline does. Runs once per stage per ticket.
	protected static void ApplyStage(LF_FineTicketRecord record)
	{
		EL_PlayerAccount holderAccount;
		IEntity holder = BodyOf(record.m_sHolderUid, holderAccount);
		int holderId = LF_PlayerInteraction.PlayerIdOf(holder);
		if (record.m_iStage == LF_FineTicketRules.STAGE_LATE)
		{
			record.m_iAmount = LateAmountOf(record);
			EL_Debug.Info(FEATURE, string.Format("ticket #%1 overdue: %2 now owes %3 (was %4)", record.m_iNumber, record.m_sHolderUid, record.m_iAmount, record.m_iBaseAmount));
			if (holderId > 0)
				EL_NotificationManagerComponent.NotifyPlayer(holderId, "#LF-Ticket_Title", "#LF-Ticket_Overdue", 6.0, EL_ENotificationType.WARNING, EL_MoneyFormat.FormatMoney(record.m_iAmount));

			return;
		}

		if (record.m_iStage != LF_FineTicketRules.STAGE_WANTED)
			return;

		EL_PlayerAccount account = holderAccount;
		if (!account)
			account = EL_PlayerAccountManager.GetOrCreate(record.m_sHolderUid);

		if (!account)
		{
			EL_Debug.Error(FEATURE, string.Format("ticket #%1 unpaid past the second deadline, but %2 has no account; no wanted level added", record.m_iNumber, record.m_sHolderUid));
			return;
		}

		account.IncreaseWantedLevel(s_iLateWanted);
		LF_WantedReports.Report(record.m_sHolderUid, holder, OFFENCE_UNPAID);
		LF_WantedReports.Identify(record.m_sHolderUid, "unpaid ticket");
		EL_PlayerAccountManager accounts = EL_PlayerAccountManager.GetInstance();
		if (accounts)
			accounts.SaveAndReleaseAccount(account);

		EL_Debug.Info(FEATURE, string.Format("ticket #%1 unpaid past the second deadline: %2 wanted +%3, now %4", record.m_iNumber, record.m_sHolderUid, s_iLateWanted, account.GetWantedLevel()));
		if (holderId > 0)
			EL_NotificationManagerComponent.NotifyPlayer(holderId, "#LF-Ticket_Title", "#LF-Ticket_NowWanted", 6.0, EL_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: pays the holder's oldest unpaid ticket from the wallet, all or nothing.
	static LF_ETicketPay Pay(string holderUid, notnull LF_CashWallet wallet, string paidAt, out LF_FineTicketRecord outRecord)
	{
		outRecord = OldestOpen(holderUid);
		if (!outRecord)
			return LF_ETicketPay.NO_TICKET;

		int amount = outRecord.m_iAmount;
		if (wallet.Carried() < amount)
			return LF_ETicketPay.NOT_ENOUGH_CASH;

		int taken = wallet.Take(amount);
		if (taken != amount)
		{
			if (taken > 0)
				wallet.Put(taken);

			EL_Debug.Error(FEATURE, string.Format("ticket #%1 not paid: took %2 of %3; returned", outRecord.m_iNumber, taken, amount));
			return LF_ETicketPay.FAILED_TAKE;
		}

		outRecord.m_bPaid = true;
		outRecord.m_sPaidAt = paidAt;
		EL_Debug.Info(FEATURE, string.Format("ticket #%1 paid by %2: %3 in cash", outRecord.m_iNumber, holderUid, amount));
		PrunePaid(holderUid);
		return LF_ETicketPay.PAID;
	}

	//------------------------------------------------------------------------------------------------
	protected static void PrunePaid(string holderUid)
	{
		int paidSeen = 0;
		for (int i = Tickets().Count() - 1; i >= 0; i--)
		{
			LF_FineTicketRecord record = Tickets().Get(i);
			if (record.m_sHolderUid != holderUid || !record.m_bPaid)
				continue;

			paidSeen++;
			if (paidSeen > KEEP_PAID_PER_HOLDER)
				Tickets().Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_FineTicketRecord> ExportAll()
	{
		array<ref LF_FineTicketRecord> records = new array<ref LF_FineTicketRecord>();
		foreach (LF_FineTicketRecord record : Tickets())
		{
			records.Insert(record.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the store (idempotent). The number sequence continues after the highest loaded.
	static void ApplyAll(array<ref LF_FineTicketRecord> records)
	{
		s_aTickets = new array<ref LF_FineTicketRecord>();
		s_iLastNumber = 0;
		if (!records)
			return;

		foreach (LF_FineTicketRecord record : records)
		{
			if (!record || record.m_sHolderUid.IsEmpty() || record.m_iNumber <= 0)
			{
				EL_Debug.Warn(FEATURE, "load skipped a ticket with no holder or number");
				continue;
			}

			if (Find(record.m_iNumber))
			{
				EL_Debug.Warn(FEATURE, string.Format("load skipped a second ticket #%1", record.m_iNumber));
				continue;
			}

			s_aTickets.Insert(record.Copy());
			s_iLastNumber = Math.Max(s_iLastNumber, record.m_iNumber);
		}

		for (int i = 1; i < s_aTickets.Count(); i++)
		{
			for (int j = i; j > 0 && s_aTickets[j - 1].m_iNumber > s_aTickets[j].m_iNumber; j--)
				s_aTickets.SwapItems(j, j - 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: start the online clock once per world.
	static void StartTicking()
	{
		if (s_bTicking || !Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(OnTick, TICK_MS, true);
		EL_Debug.Info(FEATURE, string.Format("ticket clock started: first deadline %1 s, second %2 s, x%3, +%4 wanted", FirstDeadline(), SecondDeadline(), LateMultiplier(), s_iLateWanted));
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnTick()
	{
		if (!s_aTickets || s_aTickets.IsEmpty())
			return;

		array<string> online = {};
		LF_Jail.CollectOnlineUids(online);
		TickOnline(TICK_MS * 0.001, online);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: prints the record on the holder's carried copy, when the holder is online.
	static void Reprint(LF_FineTicketRecord record)
	{
		if (!record || !Replication.IsServer())
			return;

		EL_PlayerAccount account;
		IEntity holder = BodyOf(record.m_sHolderUid, account);
		if (!holder)
			return;

		LF_FineTicketComponent ticket = LF_FineTicketItem.FindNumber(holder, record.m_iNumber);
		if (ticket)
			ticket.Print(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a fresh body carries every unpaid ticket again, and the blank copies the restart
	//! left are printed (unpaid first, then the kept receipts). Called on every spawn path.
	static void OnPlayerSpawned(string holderUid, IEntity body)
	{
		if (!Replication.IsServer() || holderUid.IsEmpty() || !body)
			return;

		array<LF_FineTicketRecord> records = {};
		CollectFor(holderUid, records, false);
		if (records.IsEmpty())
			return;

		array<LF_FineTicketRecord> missing = {};
		foreach (LF_FineTicketRecord record : records)
		{
			LF_FineTicketComponent carried = LF_FineTicketItem.FindNumber(body, record.m_iNumber);
			if (carried)
				carried.Print(record);
			else
				missing.Insert(record);
		}

		int reprinted = 0;
		int granted = 0;
		foreach (LF_FineTicketRecord record : missing)
		{
			LF_FineTicketComponent blank = LF_FineTicketItem.FindBlank(body);
			if (blank)
			{
				blank.Print(record);
				reprinted++;
				continue;
			}

			if (record.m_bPaid)
				continue;

			if (LF_FineTicketItem.Grant(body, record))
				granted++;
		}

		EL_Debug.Info(FEATURE, string.Format("spawn of %1: %2 ticket(s) on record, %3 blank copies printed, %4 unpaid handed again", holderUid, records.Count(), reprinted, granted));
	}
}

//------------------------------------------------------------------------------------------------
//! Ticket clock and the role slot, once per world on every machine.
modded class EL_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_FineTicketRole.Register();
		if (Replication.IsServer())
			LF_FineTickets.StartTicking();
	}
}
