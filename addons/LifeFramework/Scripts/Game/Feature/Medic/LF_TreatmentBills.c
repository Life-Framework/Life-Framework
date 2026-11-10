//------------------------------------------------------------------------------------------------
//! The treatment bill as a paper bill (#260, mirrors the police ticket, LF_FineTickets). A revive
//! by an on-duty medic and a hospital wake after a death each write one bill: the amount comes
//! from config, never the bank. The patient carries the paper (Prefabs/Items/Roleplay/
//! HospitalBill.et) and pays it in carried cash at the punch clock of the hospital desk. An
//! unpaid bill has no consequence in the flagship; it stays payable and is handed again with
//! every new body.
//!
//! The medic named on a revive bill earns a configured share of it, paid in cash at clock-out
//! once the patient has paid; a wake bill has no medic and no share.
//!
//! The server owns every number: the record (LF_TreatmentBillRecord) is the truth, the item is its
//! printed copy. PERSISTENCE: the records are the "bills" payload (save version 19). A carried
//! bill comes back blank after a restart; OnPlayerSpawned prints it again from the record and
//! hands a fresh copy of an unpaid bill the patient no longer carries.
//------------------------------------------------------------------------------------------------
class LF_TreatmentBillRecord : Managed
{
	int m_iNumber;
	string m_sPatientUid;
	string m_sPatientName;
	//! Empty for a hospital wake.
	string m_sMedicUid;
	string m_sMedicName;
	int m_iAmount;
	//! Cash the medic earns at clock-out once this bill is paid.
	int m_iShare;
	bool m_bPaid;
	//! True once the medic's share was handed out.
	bool m_bShareTaken;
	//! In-game date and time the bill was written, DD/MM/YYYY HH:MM.
	string m_sIssued;
	string m_sPaidAt;

	//------------------------------------------------------------------------------------------------
	LF_TreatmentBillRecord Copy()
	{
		LF_TreatmentBillRecord copy = new LF_TreatmentBillRecord();
		copy.m_iNumber = m_iNumber;
		copy.m_sPatientUid = m_sPatientUid;
		copy.m_sPatientName = m_sPatientName;
		copy.m_sMedicUid = m_sMedicUid;
		copy.m_sMedicName = m_sMedicName;
		copy.m_iAmount = m_iAmount;
		copy.m_iShare = m_iShare;
		copy.m_bPaid = m_bPaid;
		copy.m_bShareTaken = m_bShareTaken;
		copy.m_sIssued = m_sIssued;
		copy.m_sPaidAt = m_sPaidAt;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
enum LF_EBillPay
{
	PAID,
	NO_BILL,
	NOT_ENOUGH_CASH,
	FAILED_TAKE
}

//------------------------------------------------------------------------------------------------
//! Server store of treatment bills, oldest first.
class LF_TreatmentBills
{
	static const string FEATURE = "Bill";
	//! A patient carries at most this many unpaid bills; a further one is not written.
	static const int MAX_OPEN_PER_PATIENT = 5;
	//! Paid bills kept per patient, so a receipt reprints after a restart.
	static const int KEEP_PAID_PER_PATIENT = 3;

	protected static ref array<ref LF_TreatmentBillRecord> s_aBills;
	protected static int s_iLastNumber;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_TreatmentBillRecord> Bills()
	{
		if (!s_aBills)
			s_aBills = new array<ref LF_TreatmentBillRecord>();

		return s_aBills;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aBills = null;
		s_iLastNumber = 0;
		if (s_bDebtSweepRunning && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(DebtSweepTick);

		s_bDebtSweepRunning = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the medic's cut of a bill, rounded down. Never negative, never above the bill.
	static int ShareOf(int amount, float fraction)
	{
		if (amount <= 0 || fraction <= 0)
			return 0;

		if (fraction > 1)
			fraction = 1;

		return Math.Floor(amount * fraction);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: writes a bill. The seam the revive, the wake and tests share.
	//! \param medicUid Empty for a hospital wake (no share).
	//! \return The stored record, or null with no patient, a non-positive amount or too many open bills.
	static LF_TreatmentBillRecord Issue(string patientUid, string patientName, string medicUid, string medicName, int amount, int share, string issued)
	{
		if (patientUid.IsEmpty() || amount <= 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("bill not written: patient '%1' amount %2", patientUid, amount));
			return null;
		}

		if (CountOpen(patientUid) >= MAX_OPEN_PER_PATIENT)
		{
			LF_Debug.Warn(FEATURE, string.Format("bill not written: %1 already carries %2 unpaid bills", patientUid, MAX_OPEN_PER_PATIENT));
			return null;
		}

		if (medicUid.IsEmpty())
			share = 0;

		if (share > amount)
			share = amount;

		if (share < 0)
			share = 0;

		LF_TreatmentBillRecord record = new LF_TreatmentBillRecord();
		s_iLastNumber++;
		record.m_iNumber = s_iLastNumber;
		record.m_sPatientUid = patientUid;
		record.m_sPatientName = patientName;
		record.m_sMedicUid = medicUid;
		record.m_sMedicName = medicName;
		record.m_iAmount = amount;
		record.m_iShare = share;
		record.m_sIssued = issued;
		Bills().Insert(record);
		LF_Debug.Info(FEATURE, string.Format("bill #%1 written: %2 for %3 by '%4', share %5", record.m_iNumber, amount, patientUid, medicUid, share));
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the bill a revive writes, amount and share from the medic config. A patient with no
	//! player id (an NPC) is not billed.
	static LF_TreatmentBillRecord IssueForRevive(IEntity medic, IEntity patient)
	{
		string patientUid = LF_Utils.GetPlayerUID(patient);
		if (patientUid.IsEmpty())
		{
			LF_Debug.Info(FEATURE, "no bill for a revive of a patient with no player id");
			return null;
		}

		LF_MedicConfig config = LF_MedicConfig.Get();
		string medicUid = LF_Utils.GetPlayerUID(medic);
		LF_TreatmentBillRecord record = Issue(patientUid, LF_PlayerInteraction.DisplayName(patientUid), medicUid, LF_PlayerInteraction.DisplayName(medicUid), config.m_iReviveBillAmount, ShareOf(config.m_iReviveBillAmount, config.m_fBillMedicShare), LF_HospitalBill.InGameDate());
		if (record)
		{
			LF_HospitalBill.Grant(patient, record);
			LF_Utils.NotifyEntity(patient, "#LF-Bill_Received", "#LF-Bill_Title", 6.0, LF_MoneyFormat.FormatMoney(record.m_iAmount));
		}

		return record;
	}

	//------------------------------------------------------------------------------------------------
	static LF_TreatmentBillRecord Find(int number)
	{
		foreach (LF_TreatmentBillRecord record : Bills())
		{
			if (record.m_iNumber == number)
				return record;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Every bill of the patient, paid or not, oldest first.
	static void CollectFor(string patientUid, notnull array<LF_TreatmentBillRecord> outRecords, bool unpaidOnly)
	{
		outRecords.Clear();
		foreach (LF_TreatmentBillRecord record : Bills())
		{
			if (record.m_sPatientUid != patientUid)
				continue;

			if (unpaidOnly && record.m_bPaid)
				continue;

			outRecords.Insert(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	static int CountOpen(string patientUid)
	{
		array<LF_TreatmentBillRecord> open = {};
		CollectFor(patientUid, open, true);
		return open.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The oldest unpaid bill of the patient, or null.
	static LF_TreatmentBillRecord OldestOpen(string patientUid)
	{
		foreach (LF_TreatmentBillRecord record : Bills())
		{
			if (record.m_sPatientUid == patientUid && !record.m_bPaid)
				return record;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: pays the patient's oldest unpaid bill from the wallet, all or nothing, once.
	static LF_EBillPay Pay(string patientUid, notnull LF_CashWallet wallet, string paidAt, out LF_TreatmentBillRecord outRecord)
	{
		outRecord = OldestOpen(patientUid);
		if (!outRecord)
			return LF_EBillPay.NO_BILL;

		int amount = Owed(outRecord);
		if (wallet.Carried() < amount)
			return LF_EBillPay.NOT_ENOUGH_CASH;

		int taken = wallet.Take(amount);
		if (taken != amount)
		{
			if (taken > 0)
				wallet.Put(taken);

			LF_Debug.Error(FEATURE, string.Format("bill #%1 not paid: took %2 of %3; returned", outRecord.m_iNumber, taken, amount));
			return LF_EBillPay.FAILED_TAKE;
		}

		outRecord.m_bPaid = true;
		outRecord.m_sPaidAt = paidAt;
		LF_Debug.Info(FEATURE, string.Format("bill #%1 paid by %2: %3 in cash", outRecord.m_iNumber, patientUid, amount));
		PrunePaid(patientUid);
		return LF_EBillPay.PAID;
	}

	//------------------------------------------------------------------------------------------------
	//! Cash a medic is owed for paid bills, not yet handed out.
	static int ShareOwed(string medicUid)
	{
		if (medicUid.IsEmpty())
			return 0;

		int owed = 0;
		foreach (LF_TreatmentBillRecord record : Bills())
		{
			if (record.m_sMedicUid == medicUid && record.m_bPaid && !record.m_bShareTaken)
				owed += record.m_iShare;
		}

		return owed;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the medic's owed share and marks those bills settled, so a second call returns 0.
	//! \return The cash owed.
	static int TakeShare(string medicUid)
	{
		if (medicUid.IsEmpty())
			return 0;

		int owed = 0;
		foreach (LF_TreatmentBillRecord record : Bills())
		{
			if (record.m_sMedicUid != medicUid || !record.m_bPaid || record.m_bShareTaken)
				continue;

			record.m_bShareTaken = true;
			owed += record.m_iShare;
		}

		return owed;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, at clock-out: hands the medic the share of the bills paid since the last shift, as cash.
	//! \return Cash placed on the medic.
	static int PayShareAtClockOut(string medicUid, IEntity medic)
	{
		if (!Replication.IsServer() && Replication.IsRunning())
			return 0;

		int owed = TakeShare(medicUid);
		if (owed <= 0)
			return 0;

		if (!medic)
		{
			LF_Debug.Error(FEATURE, string.Format("medic share of %1 lost: no medic entity for %2", owed, medicUid));
			return 0;
		}

		int given = LF_MoneyUtils.GiveCash(medic, owed, "bill-share");
		if (given != owed)
			LF_Debug.Warn(FEATURE, string.Format("medic share short: paid %1 of %2 to %3", given, owed, medicUid));
		else
			LF_Debug.Info(FEATURE, string.Format("medic share +%1 cash to %2", given, medicUid));

		if (given > 0)
			LF_Utils.NotifyEntity(medic, "#LF-Bill_ShareToast", "#LF-Bill_Title", 5.0, LF_MoneyFormat.FormatMoney(given));

		return given;
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps the newest paid bills per patient. A bill whose medic share is still unpaid stays.
	protected static void PrunePaid(string patientUid)
	{
		int paidSeen = 0;
		for (int i = Bills().Count() - 1; i >= 0; i--)
		{
			LF_TreatmentBillRecord record = Bills().Get(i);
			if (record.m_sPatientUid != patientUid || !record.m_bPaid)
				continue;

			paidSeen++;
			if (paidSeen > KEEP_PAID_PER_PATIENT && (record.m_bShareTaken || record.m_iShare <= 0))
				Bills().Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Debt (#464). A bill still unpaid past its deadline becomes debt. It stays in the same bills
	//! record, marked in m_sPaidAt as "debt:<remaining>" while m_bPaid is false, so the save layout does
	//! not change. The bank pays it off as soon as it holds money (ledger reason DEBT_REASON).
	static const string DEBT_MARK = "debt:";
	static const string DEBT_REASON = "hospital-debt";
	protected static const int DEBT_SWEEP_MS = 60000;
	protected static bool s_bDebtSweepRunning;

	static bool IsDebt(notnull LF_TreatmentBillRecord record)
	{
		return !record.m_bPaid && record.m_sPaidAt.StartsWith(DEBT_MARK);
	}

	//! What this unpaid bill still costs: the remaining debt, or the full amount before the deadline.
	static int Owed(notnull LF_TreatmentBillRecord record)
	{
		if (IsDebt(record))
			return Math.Max(record.m_sPaidAt.Substring(DEBT_MARK.Length(), record.m_sPaidAt.Length() - DEBT_MARK.Length()).ToInt(), 0);

		return record.m_iAmount;
	}

	//! In-game minutes of a DD/MM/YYYY HH:MM stamp; -1 when it does not parse.
	static int StampMinutes(string stamp)
	{
		if (stamp.Length() != 16)
			return -1;

		int d = stamp.Substring(0, 2).ToInt();
		int m = stamp.Substring(3, 2).ToInt();
		int y = stamp.Substring(6, 4).ToInt();
		int h = stamp.Substring(11, 2).ToInt();
		int min = stamp.Substring(14, 2).ToInt();
		if (d < 1 || d > 31 || m < 1 || m > 12 || y < 1)
			return -1;

		int shifted = m - 3;
		if (m <= 2)
		{
			y--;
			shifted = m + 9;
		}

		int era = y / 400;
		int yoe = y - era * 400;
		int doy = (153 * shifted + 2) / 5 + d - 1;
		int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		int days = era * 146097 + doe;
		return days * 1440 + h * 60 + min;
	}

	//! True when an unpaid bill that is not yet debt is at least deadlineHours old at nowStamp.
	static bool PastDeadline(notnull LF_TreatmentBillRecord record, string nowStamp, int deadlineHours)
	{
		if (record.m_bPaid || IsDebt(record) || deadlineHours <= 0)
			return false;

		int issued = StampMinutes(record.m_sIssued);
		int now = StampMinutes(nowStamp);
		if (issued < 0 || now < 0)
			return false;

		return now - issued >= deadlineHours * 60;
	}

	//! Outstanding debt (bills already turned into debt) of one patient.
	static int TotalDebt(string patientUid)
	{
		int total = 0;
		foreach (LF_TreatmentBillRecord record : Bills())
		{
			if (record.m_sPatientUid == patientUid && IsDebt(record))
				total += Owed(record);
		}

		return total;
	}

	//! Server: turns this patient's overdue bills into debt, then takes what the bank holds against the
	//! debt, oldest first, partially if it must. Each take is a bank charge with DEBT_REASON, so the
	//! ledger records it.
	//! \return Cash taken from the bank.
	static int Sweep(string patientUid, LF_BankAccount bank, string nowStamp, int deadlineHours)
	{
		if (patientUid.IsEmpty())
			return 0;

		int taken = 0;
		foreach (LF_TreatmentBillRecord record : Bills())
		{
			if (record.m_sPatientUid != patientUid || record.m_bPaid)
				continue;

			if (PastDeadline(record, nowStamp, deadlineHours))
			{
				record.m_sPaidAt = DEBT_MARK + record.m_iAmount.ToString();
				LF_Debug.Info(FEATURE, string.Format("bill #%1 unpaid past %2 h: %3 is now debt on %4", record.m_iNumber, deadlineHours, record.m_iAmount, patientUid));
			}

			if (!IsDebt(record) || !bank)
				continue;

			int owed = Owed(record);
			int take = Math.Min(owed, bank.GetBalance());
			if (take <= 0)
				continue;

			int charged = bank.Charge(take, false, DEBT_REASON);
			owed -= charged;
			taken += charged;
			if (owed <= 0)
			{
				record.m_bPaid = true;
				record.m_sPaidAt = nowStamp;
				LF_Debug.Info(FEATURE, string.Format("bill #%1 debt cleared from the bank of %2", record.m_iNumber, patientUid));
			}
			else
			{
				record.m_sPaidAt = DEBT_MARK + owed.ToString();
				LF_Debug.Info(FEATURE, string.Format("bill #%1 debt: took %2 from the bank of %3, %4 left", record.m_iNumber, charged, patientUid, owed));
			}
		}

		return taken;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: Sweep with the live clock and config, for one bank account (run when it is credited).
	static int CollectDebt(LF_BankAccount bank)
	{
		if (!bank || (!Replication.IsServer() && Replication.IsRunning()))
			return 0;

		LF_MedicConfig config = LF_MedicConfig.Get();
		if (!config)
			return 0;

		return Sweep(bank.GetPersistentId(), bank, LF_HospitalBill.InGameDate(), config.m_iBillDeadlineHours);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DebtSweepTick()
	{
		array<string> patients = {};
		foreach (LF_TreatmentBillRecord record : Bills())
		{
			if (!record.m_bPaid && patients.Find(record.m_sPatientUid) < 0)
				patients.Insert(record.m_sPatientUid);
		}

		foreach (string uid : patients)
		{
			CollectDebt(LF_ATMManager.GetOrCreate(uid));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: starts the once-a-minute check that matures overdue bills (idempotent).
	static void StartDebtSweep()
	{
		if (s_bDebtSweepRunning || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bDebtSweepRunning = true;
		GetGame().GetCallqueue().CallLater(DebtSweepTick, DEBT_SWEEP_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_TreatmentBillRecord> ExportAll()
	{
		array<ref LF_TreatmentBillRecord> records = new array<ref LF_TreatmentBillRecord>();
		foreach (LF_TreatmentBillRecord record : Bills())
		{
			records.Insert(record.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the store (idempotent). The number sequence continues after the highest loaded.
	static void ApplyAll(array<ref LF_TreatmentBillRecord> records)
	{
		s_aBills = new array<ref LF_TreatmentBillRecord>();
		s_iLastNumber = 0;
		if (!records)
			return;

		foreach (LF_TreatmentBillRecord record : records)
		{
			if (!record || record.m_sPatientUid.IsEmpty() || record.m_iNumber <= 0)
			{
				LF_Debug.Warn(FEATURE, "load skipped a bill with no patient or number");
				continue;
			}

			if (Find(record.m_iNumber))
			{
				LF_Debug.Warn(FEATURE, string.Format("load skipped a second bill #%1", record.m_iNumber));
				continue;
			}

			s_aBills.Insert(record.Copy());
			s_iLastNumber = Math.Max(s_iLastNumber, record.m_iNumber);
		}

		for (int i = 1; i < s_aBills.Count(); i++)
		{
			for (int j = i; j > 0 && s_aBills[j - 1].m_iNumber > s_aBills[j].m_iNumber; j--)
				s_aBills.SwapItems(j, j - 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a fresh body carries every unpaid bill again, and the blank copies a restart left are
	//! printed (unpaid first, then the kept receipts). Called on every spawn path.
	static void OnPlayerSpawned(string patientUid, IEntity body)
	{
		if (!Replication.IsServer() || patientUid.IsEmpty() || !body)
			return;

		StartDebtSweep();
		CollectDebt(LF_ATMManager.GetOrCreate(patientUid));
		array<LF_TreatmentBillRecord> records = {};
		CollectFor(patientUid, records, false);
		if (records.IsEmpty())
			return;

		array<LF_TreatmentBillRecord> missing = {};
		foreach (LF_TreatmentBillRecord record : records)
		{
			LF_HospitalBillComponent carried = LF_HospitalBill.FindNumber(body, record.m_iNumber);
			if (carried)
				carried.Print(record);
			else
				missing.Insert(record);
		}

		int reprinted = 0;
		int granted = 0;
		foreach (LF_TreatmentBillRecord record : missing)
		{
			LF_HospitalBillComponent blank = LF_HospitalBill.FindBlank(body);
			if (blank)
			{
				blank.Print(record);
				reprinted++;
				continue;
			}

			if (record.m_bPaid)
				continue;

			if (LF_HospitalBill.Grant(body, record))
				granted++;
		}

		LF_Debug.Info(FEATURE, string.Format("spawn of %1: %2 bill(s) on record, %3 blank copies printed, %4 unpaid handed again", patientUid, records.Count(), reprinted, granted));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the payer pays their oldest unpaid bill in carried cash at the hospital desk.
	static LF_EBillPay PayAtDesk(IEntity payer)
	{
		string uid = LF_Utils.GetPlayerUID(payer);
		if (uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "desk payment refused: the payer has no player id");
			return LF_EBillPay.NO_BILL;
		}

		return PayAs(payer, uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: PayAtDesk with the payer's uid given (the seam a WORLD test drives).
	static LF_EBillPay PayAs(IEntity payer, string uid)
	{
		if (!Replication.IsServer() || !payer)
			return LF_EBillPay.NO_BILL;

		int payerId = LF_PlayerInteraction.PlayerIdOf(payer);
		LF_TreatmentBillRecord open = OldestOpen(uid);
		string counterparty;
		if (open)
			counterparty = open.m_sMedicUid;

		LF_TreatmentBillRecord record;
		LF_EBillPay result = Pay(uid, new LF_EntityCashWallet(payer, "treatment-bill", counterparty), LF_HospitalBill.InGameDate(), record);
		if (result == LF_EBillPay.NO_BILL)
		{
			LF_Debug.Info(FEATURE, "desk payment refused: " + uid + " has no unpaid bill");
			LF_NotificationManagerComponent.NotifyPlayer(payerId, "#LF-Bill_Title", "#LF-Bill_NoneOpen", 4.0, LF_ENotificationType.INFO);
			return result;
		}

		if (result != LF_EBillPay.PAID)
		{
			LF_Debug.Info(FEATURE, string.Format("desk payment refused for #%1: %2", record.m_iNumber, typename.EnumToString(LF_EBillPay, result)));
			LF_NotificationManagerComponent.NotifyPlayer(payerId, "#LF-Bill_Title", "#LF-Bill_NotEnoughCash", 5.0, LF_ENotificationType.WARNING, LF_MoneyFormat.FormatMoney(record.m_iAmount));
			return result;
		}

		LF_HospitalBillComponent receipt = LF_HospitalBill.FindNumber(payer, record.m_iNumber);
		if (receipt)
			receipt.Print(record);
		else
			LF_HospitalBill.Grant(payer, record);

		LF_NotificationManagerComponent.NotifyPlayer(payerId, "#LF-Bill_Title", "#LF-Bill_PaidToast", 5.0, LF_ENotificationType.SUCCESS, LF_MoneyFormat.FormatMoney(record.m_iAmount));
		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! "Pay bill" on the punch clock of the hospital desk. Shown while the user carries an unpaid
//! bill; the name reads the number and the amount. The server pays from the records, not from
//! the paper.
class LF_TreatmentBillPayAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_HospitalBill.FirstUnpaid(user) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_HospitalBillComponent bill = LF_HospitalBill.FirstUnpaid(user);
		if (!bill)
			return false;

		if (LF_MoneyUtils.GetCash(user) >= bill.GetAmount())
			return true;

		SetCannotPerformReason(WidgetManager.Translate("#LF-Bill_NeedCash", LF_MoneyFormat.FormatMoney(bill.GetAmount())));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		LF_HospitalBillComponent bill = LF_HospitalBill.FirstUnpaid(LF_Utils.GetLocalCharacter());
		if (!bill)
			return false;

		outName = WidgetManager.Translate("#LF-Bill_Pay", bill.GetNumber().ToString(4), LF_MoneyFormat.FormatMoney(bill.GetAmount()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_TreatmentBills.PayAtDesk(pUserEntity);
	}
}
