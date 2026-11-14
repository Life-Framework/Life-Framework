// LF_HospitalBill.c — the paper treatment bill (M1-26, #260). The server prints the bill number,
// the amount, the date and the issuing medic from the LF_TreatmentBillRecord onto the item. It is
// NOT a receipt: the bill is paid in carried cash at the hospital desk (LF_TreatmentBillPayAction)
// and the paper is then stamped paid. The record in LF_TreatmentBills is the truth; the item is
// its printed copy.
//
// PERSISTENCE. The printed fields are session state. A bill carried over a restart comes back
// blank (the carried snapshot keeps the prefab); LF_TreatmentBills.OnPlayerSpawned prints it
// again from the record, and a blank bill hides its Read action.

[ComponentEditorProps(category: "LifeFramework/Respawn", description: "Printed treatment bill")]
class LF_HospitalBillComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_HospitalBillComponent : ScriptComponent
{
	[RplProp()]
	protected int m_iNumber;

	[RplProp()]
	protected int m_iAmount;

	[RplProp()]
	protected string m_sDate;

	[RplProp()]
	protected string m_sMedicName;

	[RplProp()]
	protected bool m_bPaid;

	[RplProp()]
	protected string m_sPaidAt;

	[RplProp()]
	protected int m_iNewLifeMinutes;

	//------------------------------------------------------------------------------------------------
	//! Server: prints the record on this paper. Replicates only when something printed changes.
	//! \param newLifeMinutes The New Life Rule minutes to print, or -1 to leave them as printed.
	//! \return False off the server or with no record.
	bool Print(LF_TreatmentBillRecord record, int newLifeMinutes = -1)
	{
		if (!Replication.IsServer() || !record)
			return false;

		int minutes = m_iNewLifeMinutes;
		if (newLifeMinutes >= 0)
			minutes = newLifeMinutes;

		if (m_iNumber == record.m_iNumber && m_iAmount == record.m_iAmount && m_bPaid == record.m_bPaid && m_iNewLifeMinutes == minutes)
			return true;

		m_iNumber = record.m_iNumber;
		m_iAmount = record.m_iAmount;
		m_sDate = record.m_sIssued;
		m_sMedicName = record.m_sMedicName;
		m_bPaid = record.m_bPaid;
		m_sPaidAt = record.m_sPaidAt;
		m_iNewLifeMinutes = minutes;
		Replication.BumpMe();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool IsIssued()
	{
		return m_iNumber > 0;
	}

	//------------------------------------------------------------------------------------------------
	int GetNumber()
	{
		return m_iNumber;
	}

	//------------------------------------------------------------------------------------------------
	int GetAmount()
	{
		return m_iAmount;
	}

	//------------------------------------------------------------------------------------------------
	string GetDate()
	{
		return m_sDate;
	}

	//------------------------------------------------------------------------------------------------
	string GetMedicName()
	{
		return m_sMedicName;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPaid()
	{
		return m_bPaid;
	}

	//------------------------------------------------------------------------------------------------
	string GetPaidAt()
	{
		return m_sPaidAt;
	}

	//------------------------------------------------------------------------------------------------
	int GetNewLifeMinutes()
	{
		return m_iNewLifeMinutes;
	}
}

//------------------------------------------------------------------------------------------------
//! Item helpers: the bills a character carries, hand one over; the in-game date stamp.
class LF_HospitalBill
{
	//! The prefab slot (#1128): The hospital bill a treated character carries.
	static const string SLOT_BILL = "hospital_bill";

	//! The prefab (slot "hospital_bill"), or empty when no usable prefab resolves (logged).
	static ResourceName Prefab()
	{
		return LF_PrefabSlots.Get(SLOT_BILL);
	}

	//------------------------------------------------------------------------------------------------
	//! Every bill in the character's inventory or hands, each counted once.
	static void FindCarried(IEntity character, notnull array<LF_HospitalBillComponent> outBills)
	{
		outBills.Clear();
		if (!character)
			return;

		array<IEntity> items = {};
		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(character);
		if (inventory)
			inventory.GetItems(items);

		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(character);
		if (controller)
		{
			IEntity inHand = controller.GetAttachedGadgetAtLeftHandSlot();
			if (inHand && items.Find(inHand) < 0)
				items.Insert(inHand);
		}

		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			LF_HospitalBillComponent bill = LF_HospitalBillComponent.Cast(item.FindComponent(LF_HospitalBillComponent));
			if (bill && outBills.Find(bill) < 0)
				outBills.Insert(bill);
		}
	}

	//------------------------------------------------------------------------------------------------
	static LF_HospitalBillComponent FindNumber(IEntity character, int number)
	{
		array<LF_HospitalBillComponent> bills = {};
		FindCarried(character, bills);
		foreach (LF_HospitalBillComponent bill : bills)
		{
			if (bill.GetNumber() == number)
				return bill;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static LF_HospitalBillComponent FindBlank(IEntity character)
	{
		array<LF_HospitalBillComponent> bills = {};
		FindCarried(character, bills);
		foreach (LF_HospitalBillComponent bill : bills)
		{
			if (!bill.IsIssued())
				return bill;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The unpaid bill with the lowest number the character carries, or null. Client and server.
	static LF_HospitalBillComponent FirstUnpaid(IEntity character)
	{
		array<LF_HospitalBillComponent> bills = {};
		FindCarried(character, bills);
		LF_HospitalBillComponent first;
		foreach (LF_HospitalBillComponent bill : bills)
		{
			if (!bill.IsIssued() || bill.IsPaid())
				continue;

			if (!first || bill.GetNumber() < first.GetNumber())
				first = bill;
		}

		return first;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: spawns a bill printed from the record and puts it in the character's inventory. With
	//! no room the bill lies at the character's feet.
	//! \return The printed bill, or null when it could not be made.
	static LF_HospitalBillComponent Grant(IEntity character, LF_TreatmentBillRecord record, int newLifeMinutes = -1)
	{
		if (!Replication.IsServer() || !character || !record)
		{
			LF_Debug.Error(LF_TreatmentBills.FEATURE, "bill not granted: not the server, or no character or record");
			return null;
		}

		IEntity item = LF_Utils.SpawnEntityPrefab(Prefab(), character.GetOrigin());
		if (!item)
		{
			LF_Debug.Error(LF_TreatmentBills.FEATURE, "bill not granted: prefab did not spawn: " + Prefab());
			return null;
		}

		LF_HospitalBillComponent bill = LF_HospitalBillComponent.Cast(item.FindComponent(LF_HospitalBillComponent));
		if (!bill)
		{
			LF_Debug.Error(LF_TreatmentBills.FEATURE, "bill not granted: " + Prefab() + " has no LF_HospitalBillComponent");
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return null;
		}

		bill.Print(record, newLifeMinutes);

		InventoryStorageManagerComponent storage = LF_Component<InventoryStorageManagerComponent>.Find(character);
		if (!storage || !storage.TryInsertItem(item))
		{
			LF_Debug.Warn(LF_TreatmentBills.FEATURE, string.Format("bill #%1: no room in the pockets of %2; it lies at their feet", record.m_iNumber, record.m_sPatientUid));
			return bill;
		}

		LF_Debug.Info(LF_TreatmentBills.FEATURE, string.Format("treatment bill #%1 printed: %2 on %3", record.m_iNumber, record.m_iAmount, record.m_sIssued));
		return bill;
	}

	//------------------------------------------------------------------------------------------------
	//! The world's date and time as DD/MM/YYYY HH:MM, or the real date when the world has no clock.
	static string InGameDate()
	{
		int year, month, day, hours, minutes, seconds;
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		TimeAndWeatherManagerEntity clock;
		if (world)
			clock = world.GetTimeAndWeatherManager();

		if (clock)
		{
			year = clock.GetYear();
			month = clock.GetMonth();
			day = clock.GetDay();
			clock.GetHoursMinutesSeconds(hours, minutes, seconds);
		}
		else
		{
			System.GetYearMonthDay(year, month, day);
			System.GetHourMinuteSecond(hours, minutes, seconds);
		}

		return string.Format("%1/%2/%3 %4:%5", day.ToString(2), month.ToString(2), year, hours.ToString(2), minutes.ToString(2));
	}
}

//------------------------------------------------------------------------------------------------
//! What the holder sees on the bill. Client-only, on LF_UIScreen.
class LF_HospitalBillCard
{
	static const float CLOSE_DISTANCE = 4;

	protected static ref LF_UIScreen s_Screen;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts(int number, int amount, string date, string medicName, bool paid, string paidAt, int newLifeMinutes)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "BillTitle", "#LF-Hospital_BillTitle"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.ROW, "BillDate", WidgetManager.Translate("#LF-Hospital_BillNumber", number.ToString(4), date)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BillAmount", WidgetManager.Translate("#LF-Hospital_BillAmount", LF_MoneyFormat.FormatMoney(amount))));
		if (!medicName.IsEmpty())
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BillMedic", WidgetManager.Translate("#LF-Hospital_BillMedic", medicName)));

		if (paid)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BillPaid", WidgetManager.Translate("#LF-Hospital_BillPaidLine", paidAt)));
		else
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BillHowToPay", "#LF-Hospital_BillHowToPay"));

		if (newLifeMinutes > 0)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BillNote", "#LF-Hospital_BillNote"));
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BillNewLife", WidgetManager.Translate("#LF-Hospital_BillNewLife", newLifeMinutes)));
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "BillClose", "#LF-Hospital_BillClose"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! \param anchor The bill entity when read from the world; null on waking (no distance close).
	static LF_UIScreen Open(int number, int amount, string date, string medicName, bool paid, string paidAt, int newLifeMinutes, IEntity anchor)
	{
		Hide();
		s_Screen = LF_UIScreen.Open(BuildParts(number, amount, date, medicName, paid, paidAt, newLifeMinutes));
		if (!s_Screen)
		{
			LF_Debug.Error(LF_TreatmentBills.FEATURE, "treatment bill card failed to open");
			return null;
		}

		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		if (anchor)
			s_Screen.CloseBeyond(CLOSE_DISTANCE, anchor.GetID());

		LF_Debug.Info(LF_TreatmentBills.FEATURE, string.Format("treatment bill #%1 read: %2, paid %3", number, amount, paid));
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Screen)
			return;

		s_Screen.Close();
		s_Screen = null;
	}
}

//------------------------------------------------------------------------------------------------
//! "Read hospital bill" on a printed bill lying in the world. Local effect only; the printed
//! fields arrived as RplProps.
class LF_HospitalBillReadAction : ScriptedUserAction
{
	protected LF_HospitalBillComponent m_Bill;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Bill = LF_HospitalBillComponent.Cast(pOwnerEntity.FindComponent(LF_HospitalBillComponent));
		if (!m_Bill)
			LF_Debug.Error(LF_TreatmentBills.FEATURE, "Read bill action on an entity with no LF_HospitalBillComponent; Read hidden");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return m_Bill && m_Bill.IsIssued();
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		if (!m_Bill || !m_Bill.IsIssued())
		{
			LF_Debug.Warn(LF_TreatmentBills.FEATURE, "Read performed on a blank bill; card not opened");
			return;
		}

		LF_HospitalBillCard.Open(m_Bill.GetNumber(), m_Bill.GetAmount(), m_Bill.GetDate(), m_Bill.GetMedicName(), m_Bill.IsPaid(), m_Bill.GetPaidAt(), m_Bill.GetNewLifeMinutes(), pOwnerEntity);
	}
}
