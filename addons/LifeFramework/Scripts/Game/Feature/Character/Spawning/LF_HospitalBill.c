// LF_HospitalBill.c — the paper hospital bill a player wakes up with (M1-26). The server prints
// the amount, the bank balance after it, and the in-game date on the item; the holder reads it
// on waking and again with Read when it lies in the world. The bill is a receipt: the bank was
// charged on the server before it was printed.
//
// PERSISTENCE. The printed fields are session state. A bill carried over a restart comes back
// blank (the carried snapshot keeps the prefab), and a blank bill hides its Read action.

[ComponentEditorProps(category: "LifeFramework/Respawn", description: "Printed hospital bill")]
class LF_HospitalBillComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_HospitalBillComponent : ScriptComponent
{
	[RplProp()]
	protected bool m_bIssued;

	[RplProp()]
	protected int m_iAmount;

	[RplProp()]
	protected int m_iBalanceAfter;

	[RplProp()]
	protected string m_sDate;

	[RplProp()]
	protected int m_iNewLifeMinutes;

	//------------------------------------------------------------------------------------------------
	//! Server: prints a blank bill once.
	//! \return False when not the server or already printed.
	bool Issue(int amount, int balanceAfter, string date, int newLifeMinutes)
	{
		if (!Replication.IsServer())
			return false;

		if (m_bIssued)
		{
			LF_Debug.Warn("Respawn", "bill not printed: already printed");
			return false;
		}

		m_bIssued = true;
		m_iAmount = amount;
		m_iBalanceAfter = balanceAfter;
		m_sDate = date;
		m_iNewLifeMinutes = newLifeMinutes;
		Replication.BumpMe();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool IsIssued()
	{
		return m_bIssued;
	}

	//------------------------------------------------------------------------------------------------
	int GetAmount()
	{
		return m_iAmount;
	}

	//------------------------------------------------------------------------------------------------
	int GetBalanceAfter()
	{
		return m_iBalanceAfter;
	}

	//------------------------------------------------------------------------------------------------
	string GetDate()
	{
		return m_sDate;
	}

	//------------------------------------------------------------------------------------------------
	int GetNewLifeMinutes()
	{
		return m_iNewLifeMinutes;
	}
}

//------------------------------------------------------------------------------------------------
//! Server helpers: print a bill into a character's pockets; the in-game date stamp.
class LF_HospitalBill
{
	static const ResourceName PREFAB = "{75055A5BFE4372D5}Prefabs/Items/Roleplay/HospitalBill.et";

	//------------------------------------------------------------------------------------------------
	//! Server: spawns a printed bill and puts it in the character's inventory. With no room the
	//! bill stays on the floor at the character's feet.
	//! \return The bill entity, or null when it could not be made.
	static IEntity Grant(IEntity character, int amount, int balanceAfter, string date, int newLifeMinutes)
	{
		if (!Replication.IsServer() || !character)
		{
			LF_Debug.Error("Respawn", "bill not granted: not the server or no character");
			return null;
		}

		IEntity item = LF_Utils.SpawnEntityPrefab(PREFAB, character.GetOrigin());
		if (!item)
		{
			LF_Debug.Error("Respawn", "bill not granted: prefab did not spawn: " + PREFAB);
			return null;
		}

		LF_HospitalBillComponent bill = LF_HospitalBillComponent.Cast(item.FindComponent(LF_HospitalBillComponent));
		if (!bill)
		{
			LF_Debug.Error("Respawn", "bill not granted: " + PREFAB + " has no LF_HospitalBillComponent");
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return null;
		}

		bill.Issue(amount, balanceAfter, date, newLifeMinutes);

		InventoryStorageManagerComponent storage = LF_Component<InventoryStorageManagerComponent>.Find(character);
		if (!storage || !storage.TryInsertItem(item))
		{
			LF_Debug.Warn("Respawn", "no room for the hospital bill; it stays at the bed");
			return item;
		}

		LF_Debug.Info("Respawn", string.Format("hospital bill printed: %1 on %2, balance %3", amount, date, balanceAfter));
		return item;
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
	static array<ref LF_UIPart> BuildParts(int amount, int balanceAfter, string date, int newLifeMinutes)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "BillTitle", "#LF-Hospital_BillTitle"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.ROW, "BillDate", date));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BillAmount", WidgetManager.Translate("#LF-Hospital_BillAmount", LF_MoneyFormat.FormatMoney(amount))));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BillBalance", WidgetManager.Translate("#LF-Hospital_BillBalance", LF_MoneyFormat.FormatMoney(balanceAfter))));
		if (balanceAfter < 0)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BillDebt", "#LF-Hospital_BillDebt"));

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BillNote", "#LF-Hospital_BillNote"));
		if (newLifeMinutes > 0)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BillNewLife", WidgetManager.Translate("#LF-Hospital_BillNewLife", newLifeMinutes)));

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "BillClose", "#LF-Hospital_BillClose"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! \param anchor The bill entity when read from the world; null on waking (no distance close).
	static LF_UIScreen Open(int amount, int balanceAfter, string date, int newLifeMinutes, IEntity anchor)
	{
		Hide();
		s_Screen = LF_UIScreen.Open(BuildParts(amount, balanceAfter, date, newLifeMinutes));
		if (!s_Screen)
		{
			LF_Debug.Error("Respawn", "hospital bill card failed to open");
			return null;
		}

		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		if (anchor)
			s_Screen.CloseBeyond(CLOSE_DISTANCE, anchor.GetID());

		LF_Debug.Info("Respawn", string.Format("hospital bill read: %1, balance %2", amount, balanceAfter));
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
			LF_Debug.Error("Respawn", "Read bill action on an entity with no LF_HospitalBillComponent; Read hidden");
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
			LF_Debug.Warn("Respawn", "Read performed on a blank bill; card not opened");
			return;
		}

		LF_HospitalBillCard.Open(m_Bill.GetAmount(), m_Bill.GetBalanceAfter(), m_Bill.GetDate(), m_Bill.GetNewLifeMinutes(), pOwnerEntity);
	}
}
