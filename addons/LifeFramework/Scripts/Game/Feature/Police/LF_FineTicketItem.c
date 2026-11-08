//------------------------------------------------------------------------------------------------
//! The printed police ticket (Prefabs/Items/Roleplay/FineTicket.et). Only the server prints it,
//! from the LF_FineTicketRecord; the fields replicate as RplProps so whoever holds it can read
//! it: hovering it in the inventory, Read when it lies in the world, and the card that opens
//! when it is handed over or stamped. It says, for a stranger, what was done, what it costs,
//! where to pay, by when, and what happens if they do not.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Police", description: "Printed police ticket: offence, amount, deadline, officer.")]
class LF_FineTicketComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_FineTicketComponent : ScriptComponent
{
	[RplProp()]
	protected int m_iNumber;

	[RplProp()]
	protected string m_sHolderUid;

	[RplProp()]
	protected string m_sHolderName;

	[RplProp()]
	protected string m_sOffence;

	[RplProp()]
	protected int m_iAmount;

	[RplProp()]
	protected int m_iLateAmount;

	[RplProp()]
	protected int m_iStage;

	[RplProp()]
	protected int m_iMinutesLeft;

	[RplProp()]
	protected bool m_bPaid;

	[RplProp()]
	protected string m_sOfficerName;

	[RplProp()]
	protected int m_iOfficerRank;

	[RplProp()]
	protected string m_sIssued;

	[RplProp()]
	protected string m_sPaidAt;

	//------------------------------------------------------------------------------------------------
	//! Server: prints the record on this paper. Replicates only when something printed changes.
	//! \return False off the server or with no record.
	bool Print(LF_FineTicketRecord record)
	{
		if (!Replication.IsServer() || !record)
			return false;

		int minutes = LF_FineTicketRules.MinutesLeft(LF_FineTickets.SecondsToNext(record));
		int lateAmount = LF_FineTickets.LateAmountOf(record);
		if (m_iNumber == record.m_iNumber && m_iAmount == record.m_iAmount && m_iStage == record.m_iStage && m_bPaid == record.m_bPaid && m_iMinutesLeft == minutes && m_iLateAmount == lateAmount)
			return true;

		m_iNumber = record.m_iNumber;
		m_sHolderUid = record.m_sHolderUid;
		m_sHolderName = record.m_sHolderName;
		m_sOffence = record.m_sOffence;
		m_iAmount = record.m_iAmount;
		m_iLateAmount = lateAmount;
		m_iStage = record.m_iStage;
		m_iMinutesLeft = minutes;
		m_bPaid = record.m_bPaid;
		m_sOfficerName = record.m_sOfficerName;
		m_iOfficerRank = record.m_iOfficerRank;
		m_sIssued = record.m_sIssued;
		m_sPaidAt = record.m_sPaidAt;
		Replication.BumpMe();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPrinted()
	{
		return m_iNumber > 0;
	}

	//------------------------------------------------------------------------------------------------
	int GetNumber()
	{
		return m_iNumber;
	}

	//------------------------------------------------------------------------------------------------
	string GetHolderUid()
	{
		return m_sHolderUid;
	}

	//------------------------------------------------------------------------------------------------
	int GetAmount()
	{
		return m_iAmount;
	}

	//------------------------------------------------------------------------------------------------
	int GetStage()
	{
		return m_iStage;
	}

	//------------------------------------------------------------------------------------------------
	int GetMinutesLeft()
	{
		return m_iMinutesLeft;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPaid()
	{
		return m_bPaid;
	}

	//------------------------------------------------------------------------------------------------
	//! The printed lines, localized on the reading client, top to bottom.
	void DescribeLines(notnull array<string> outLines)
	{
		outLines.Clear();
		if (!IsPrinted())
		{
			outLines.Insert(WidgetManager.Translate("#LF-Ticket_Blank"));
			return;
		}

		outLines.Insert(WidgetManager.Translate("#LF-Ticket_Number", m_iNumber.ToString(4), m_sIssued));
		outLines.Insert(WidgetManager.Translate("#LF-Ticket_IssuedTo", m_sHolderName));
		outLines.Insert(WidgetManager.Translate("#LF-Ticket_Offence", WidgetManager.Translate(m_sOffence)));
		outLines.Insert(WidgetManager.Translate("#LF-Ticket_Amount", EL_MoneyFormat.FormatMoney(m_iAmount)));
		if (m_bPaid)
		{
			outLines.Insert(WidgetManager.Translate("#LF-Ticket_PaidLine", m_sPaidAt));
		}
		else if (m_iStage == LF_FineTicketRules.STAGE_OPEN)
		{
			outLines.Insert(WidgetManager.Translate("#LF-Ticket_DueOpen", m_iMinutesLeft, EL_MoneyFormat.FormatMoney(m_iLateAmount)));
			outLines.Insert(WidgetManager.Translate("#LF-Ticket_HowToPay"));
		}
		else if (m_iStage == LF_FineTicketRules.STAGE_LATE)
		{
			outLines.Insert(WidgetManager.Translate("#LF-Ticket_DueLate", m_iMinutesLeft));
			outLines.Insert(WidgetManager.Translate("#LF-Ticket_HowToPay"));
		}
		else
		{
			outLines.Insert(WidgetManager.Translate("#LF-Ticket_DueWanted"));
			outLines.Insert(WidgetManager.Translate("#LF-Ticket_HowToPay"));
		}

		string rank = WidgetManager.Translate("#" + LF_PoliceRanks.TitleKey(m_iOfficerRank));
		outLines.Insert(WidgetManager.Translate("#LF-Ticket_Officer", rank, m_sOfficerName));
	}

	//------------------------------------------------------------------------------------------------
	//! The whole paper as one text (inventory hover).
	string Describe()
	{
		array<string> lines = {};
		DescribeLines(lines);
		string text;
		foreach (int i, string line : lines)
		{
			if (i > 0)
				text = text + "\n";

			text = text + line;
		}

		return text;
	}
}

//------------------------------------------------------------------------------------------------
//! Inventory read: hovering the ticket shows what is printed on it.
[BaseContainerProps()]
class LF_FineTicketUIInfo : SCR_InventoryUIInfo
{
	//------------------------------------------------------------------------------------------------
	override string GetInventoryItemDescription(InventoryItemComponent item)
	{
		if (!item || !item.GetOwner())
			return super.GetInventoryItemDescription(item);

		LF_FineTicketComponent ticket = LF_FineTicketComponent.Cast(item.GetOwner().FindComponent(LF_FineTicketComponent));
		if (!ticket)
			return super.GetInventoryItemDescription(item);

		return ticket.Describe();
	}
}

//------------------------------------------------------------------------------------------------
//! Item helpers: find the tickets a character carries, hand one over.
class LF_FineTicketItem
{
	static const ResourceName PREFAB = "{3D6B0E8A41C7F259}Prefabs/Items/Roleplay/FineTicket.et";

	//------------------------------------------------------------------------------------------------
	//! Every ticket in the character's inventory or hands, each counted once.
	static void FindCarried(IEntity character, notnull array<LF_FineTicketComponent> outTickets)
	{
		outTickets.Clear();
		if (!character)
			return;

		array<IEntity> items = {};
		SCR_InventoryStorageManagerComponent inventory = EL_Component<SCR_InventoryStorageManagerComponent>.Find(character);
		if (inventory)
			inventory.GetItems(items);

		CharacterControllerComponent controller = EL_Component<CharacterControllerComponent>.Find(character);
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

			LF_FineTicketComponent ticket = LF_FineTicketComponent.Cast(item.FindComponent(LF_FineTicketComponent));
			if (ticket && outTickets.Find(ticket) < 0)
				outTickets.Insert(ticket);
		}
	}

	//------------------------------------------------------------------------------------------------
	static LF_FineTicketComponent FindNumber(IEntity character, int number)
	{
		array<LF_FineTicketComponent> tickets = {};
		FindCarried(character, tickets);
		foreach (LF_FineTicketComponent ticket : tickets)
		{
			if (ticket.GetNumber() == number)
				return ticket;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static LF_FineTicketComponent FindBlank(IEntity character)
	{
		array<LF_FineTicketComponent> tickets = {};
		FindCarried(character, tickets);
		foreach (LF_FineTicketComponent ticket : tickets)
		{
			if (!ticket.IsPrinted())
				return ticket;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The unpaid ticket with the lowest number the character carries, or null. Client and server.
	static LF_FineTicketComponent FirstUnpaid(IEntity character)
	{
		array<LF_FineTicketComponent> tickets = {};
		FindCarried(character, tickets);
		LF_FineTicketComponent first;
		foreach (LF_FineTicketComponent ticket : tickets)
		{
			if (!ticket.IsPrinted() || ticket.IsPaid())
				continue;

			if (!first || ticket.GetNumber() < first.GetNumber())
				first = ticket;
		}

		return first;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: prints the record on a new ticket and puts it in the character's inventory. With no
	//! room the ticket lies at their feet.
	//! \return The printed ticket, or null when the prefab did not spawn.
	static LF_FineTicketComponent Grant(IEntity character, LF_FineTicketRecord record)
	{
		if (!Replication.IsServer() || !character || !record)
		{
			EL_Debug.Error(LF_FineTickets.FEATURE, "ticket not handed: not the server, or no character or record");
			return null;
		}

		IEntity item = EL_Utils.SpawnEntityPrefab(PREFAB, character.GetOrigin());
		if (!item)
		{
			EL_Debug.Error(LF_FineTickets.FEATURE, "ticket not handed: prefab did not spawn " + PREFAB);
			return null;
		}

		LF_FineTicketComponent ticket = LF_FineTicketComponent.Cast(item.FindComponent(LF_FineTicketComponent));
		if (!ticket)
		{
			EL_Debug.Error(LF_FineTickets.FEATURE, "ticket not handed: " + PREFAB + " has no LF_FineTicketComponent");
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return null;
		}

		ticket.Print(record);
		InventoryStorageManagerComponent storage = EL_Component<InventoryStorageManagerComponent>.Find(character);
		if (!storage || !storage.TryInsertItem(item))
		{
			EL_Debug.Warn(LF_FineTickets.FEATURE, string.Format("ticket #%1: no room in the pockets of %2; it lies at their feet", record.m_iNumber, record.m_sHolderUid));
			return ticket;
		}

		return ticket;
	}
}

//------------------------------------------------------------------------------------------------
//! The ticket as a paper card. Client-only, on LF_UIScreen.
class LF_FineTicketCard
{
	static const float CLOSE_DISTANCE = 4;

	protected static ref LF_UIScreen s_Screen;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts(LF_FineTicketComponent ticket)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "TicketTitle", "#LF-Ticket_CardTitle"));
		array<string> lines = {};
		ticket.DescribeLines(lines);
		foreach (int i, string line : lines)
		{
			LF_EUIPartKind kind = LF_EUIPartKind.LINE;
			if (i == 0)
				kind = LF_EUIPartKind.ROW;

			parts.Insert(LF_UIPart.Create(kind, "TicketLine" + i, line));
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "TicketClose", "#LF-Ticket_CardClose"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! \param anchor The ticket when read from the world; null when it is in the pockets.
	static LF_UIScreen Open(LF_FineTicketComponent ticket, IEntity anchor)
	{
		Hide();
		if (!ticket)
		{
			EL_Debug.Error(LF_FineTickets.FEATURE, "ticket card not opened: no ticket");
			return null;
		}

		s_Screen = LF_UIScreen.Open(BuildParts(ticket));
		if (!s_Screen)
		{
			EL_Debug.Error(LF_FineTickets.FEATURE, "ticket card failed to open");
			return null;
		}

		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		if (anchor)
			s_Screen.CloseBeyond(CLOSE_DISTANCE, anchor.GetID());

		EL_Debug.Info(LF_FineTickets.FEATURE, string.Format("ticket #%1 read: amount %2 stage %3 paid %4", ticket.GetNumber(), ticket.GetAmount(), ticket.GetStage(), ticket.IsPaid()));
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
//! "Read ticket" on a printed ticket lying in the world. Local effect only.
class LF_FineTicketReadAction : ScriptedUserAction
{
	protected LF_FineTicketComponent m_Ticket;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Ticket = LF_FineTicketComponent.Cast(pOwnerEntity.FindComponent(LF_FineTicketComponent));
		if (!m_Ticket)
			EL_Debug.Error(LF_FineTickets.FEATURE, "Read ticket on an entity with no LF_FineTicketComponent; Read hidden");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return m_Ticket && m_Ticket.IsPrinted();
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || pUserEntity != EL_Utils.GetLocalCharacter())
			return;

		LF_FineTicketCard.Open(m_Ticket, pOwnerEntity);
	}
}
