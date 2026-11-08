//------------------------------------------------------------------------------------------------
//! Writing and paying a ticket in the world.
//!   Write ticket : the `fine` slot of the player interaction menu (LF_PlayerRoleAction on the
//!                  character, filled here). The server opens the ticket pad on the officer's
//!                  machine; the officer ticks one offence; the server checks duty, reach, rank
//!                  cap, and the open-ticket limit, then hands the ticket over.
//!   Pay ticket   : LF_FineTicketPayAction on the punch clock of the police station desk.
//! Cues: the officer's arm reaches out (commanding gesture), the paper rustles at the holder, the
//! holder's hand takes it, and the card opens on their screen. At the desk the clock stamps the
//! receipt for everyone nearby.
//------------------------------------------------------------------------------------------------
class LF_FineTicketRole : LF_PlayerRoleHandler
{
	static const string SLOT = "fine";
	static const string SOUND_PAPER = "SOUND_PICK_UP";
	static const string SOUND_STAMP = "SOUND_RANGECP_STARTBUTTON";
	//! Delay before the paper sound, so the new item has reached the clients.
	static const int SOUND_DELAY_MS = 400;

	//------------------------------------------------------------------------------------------------
	//! Every machine, once per world (EL_GameModeRoleplay.OnGameStart). Idempotent.
	static void Register()
	{
		if (LF_FineTicketRole.Cast(LF_PlayerInteraction.GetRole(SLOT)))
			return;

		LF_PlayerInteraction.RegisterRole(SLOT, new LF_FineTicketRole());
	}

	//------------------------------------------------------------------------------------------------
	override string GetActionName()
	{
		return "#LF-Ticket_Write";
	}

	//------------------------------------------------------------------------------------------------
	override bool CanOffer(IEntity user, IEntity target)
	{
		return LF_PlayerInteraction.IsOnDutyPolice(user);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(IEntity user, IEntity target)
	{
		OpenPad(user, target);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: opens the ticket pad on the officer's machine with their rank cap.
	static bool OpenPad(IEntity officer, IEntity target)
	{
		EL_PlayerAccount account = EL_PoliceUtils.GetAccountForEntity(officer);
		if (!EL_PoliceUtils.IsOnDutyOfficer(account))
		{
			EL_Debug.Warn(LF_FineTickets.FEATURE, "ticket pad refused: not an on-duty officer");
			return false;
		}

		SCR_PlayerController controller = LF_PlayerInteraction.ControllerOf(officer);
		if (!controller)
		{
			EL_Debug.Error(LF_FineTickets.FEATURE, "ticket pad refused: the officer has no player controller");
			return false;
		}

		int cap = LF_PoliceRanks.FineCap(account.GetPoliceRank());
		controller.LF_OpenTicketPad(EL_NetworkUtils.GetRplId(target), cap);
		EL_Debug.Info(LF_FineTickets.FEATURE, string.Format("ticket pad opened for player %1, cap %2", LF_PlayerInteraction.PlayerIdOf(officer), cap));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_ETicketRefusal refusal)
	{
		if (refusal == LF_ETicketRefusal.ABOVE_CAP)
			return "#EL-Rank_FineDenied";

		if (refusal == LF_ETicketRefusal.TOO_MANY_OPEN)
			return "#LF-Ticket_TooMany";

		return "#LF-Ticket_Refused";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the officer wrote offenceIndex on a ticket for target. Every check again.
	//! \return The record, or null when refused.
	static LF_FineTicketRecord Write(IEntity officer, IEntity target, int offenceIndex)
	{
		if (!Replication.IsServer())
			return null;

		int officerId = LF_PlayerInteraction.PlayerIdOf(officer);
		LF_EInteractionRefusal reach = LF_PlayerInteraction.CheckPair(officer, target);
		if (reach != LF_EInteractionRefusal.NONE)
		{
			EL_Debug.Info(LF_FineTickets.FEATURE, "ticket refused: " + typename.EnumToString(LF_EInteractionRefusal, reach));
			EL_NotificationManagerComponent.NotifyPlayer(officerId, "#LF-Ticket_Title", LF_PlayerInteraction.RefusalKey(reach), 4.0, EL_ENotificationType.WARNING);
			return null;
		}

		EL_PlayerAccount account = EL_PoliceUtils.GetAccountForEntity(officer);
		string officerUid = EL_Utils.GetPlayerUID(officer);
		string holderUid = EL_Utils.GetPlayerUID(target);
		LF_TicketOffence offence;
		LF_ETicketRefusal refusal = LF_FineTickets.CheckWrite(account, officerUid, holderUid, offenceIndex, offence);
		if (refusal != LF_ETicketRefusal.NONE)
		{
			EL_Debug.Warn(LF_FineTickets.FEATURE, string.Format("ticket refused for player %1 -> %2 offence %3: %4", officerId, holderUid, offenceIndex, typename.EnumToString(LF_ETicketRefusal, refusal)));
			EL_NotificationManagerComponent.NotifyPlayer(officerId, "#LF-Ticket_Title", RefusalKey(refusal), 4.0, EL_ENotificationType.WARNING, LF_FineTickets.MAX_OPEN_PER_HOLDER.ToString());
			return null;
		}

		string holderName = LF_PlayerInteraction.DisplayName(holderUid);
		string officerName = LF_PlayerInteraction.DisplayName(officerUid);
		LF_FineTicketRecord record = LF_FineTickets.Issue(holderUid, holderName, officerUid, officerName, account.GetPoliceRank(), offence.m_sName, offence.m_iAmount, LF_HospitalBill.InGameDate());
		if (!record)
			return null;

		LF_FineTicketComponent ticket = LF_FineTicketItem.Grant(target, record);
		LF_PlayerInteraction.PlayGesture(officer, LF_PlayerInteraction.GESTURE_POINT);
		if (ticket)
		{
			RplId itemId = EL_NetworkUtils.GetRplId(ticket.GetOwner());
			PlayPaperLater(target, itemId);
			SCR_PlayerController holderController = LF_PlayerInteraction.ControllerOf(target);
			if (holderController)
				holderController.LF_TicketCue(itemId);
		}

		EL_NotificationManagerComponent.NotifyPlayer(officerId, "#LF-Ticket_Title", "#LF-Ticket_Written", 4.0, EL_ENotificationType.SUCCESS, EL_MoneyFormat.FormatMoney(record.m_iAmount));
		EL_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(target), "#LF-Ticket_Title", "#LF-Ticket_Received", 6.0, EL_ENotificationType.WARNING);
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the paper sound from the ticket, for everyone nearby, once clients have the item.
	static void PlayPaperLater(IEntity holder, RplId itemId)
	{
		LF_TicketPaperSound.Schedule(holder, itemId, SOUND_PAPER, SOUND_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the payer pays their oldest unpaid ticket in carried cash at the desk.
	static LF_ETicketPay PayAtDesk(IEntity payer)
	{
		string uid = EL_Utils.GetPlayerUID(payer);
		if (uid.IsEmpty())
		{
			EL_Debug.Error(LF_FineTickets.FEATURE, "desk payment refused: the payer has no player id");
			return LF_ETicketPay.NO_TICKET;
		}

		return PayAs(payer, uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: PayAtDesk with the payer's uid given (the seam a WORLD test drives).
	static LF_ETicketPay PayAs(IEntity payer, string uid)
	{
		if (!Replication.IsServer() || !payer)
			return LF_ETicketPay.NO_TICKET;

		int payerId = LF_PlayerInteraction.PlayerIdOf(payer);
		LF_FineTicketRecord record;
		LF_ETicketPay result = LF_FineTickets.Pay(uid, new LF_EntityCashWallet(payer), LF_HospitalBill.InGameDate(), record);
		if (result == LF_ETicketPay.NO_TICKET)
		{
			EL_Debug.Info(LF_FineTickets.FEATURE, "desk payment refused: " + uid + " has no unpaid ticket");
			EL_NotificationManagerComponent.NotifyPlayer(payerId, "#LF-Ticket_Title", "#LF-Ticket_NoneOpen", 4.0, EL_ENotificationType.INFO);
			return result;
		}

		if (result != LF_ETicketPay.PAID)
		{
			EL_Debug.Info(LF_FineTickets.FEATURE, string.Format("desk payment refused for #%1: %2", record.m_iNumber, typename.EnumToString(LF_ETicketPay, result)));
			EL_NotificationManagerComponent.NotifyPlayer(payerId, "#LF-Ticket_Title", "#LF-Ticket_NotEnoughCash", 5.0, EL_ENotificationType.WARNING, EL_MoneyFormat.FormatMoney(record.m_iAmount));
			return result;
		}

		LF_FineTicketComponent receipt = LF_FineTicketItem.FindNumber(payer, record.m_iNumber);
		if (receipt)
			receipt.Print(record);
		else
			receipt = LF_FineTicketItem.Grant(payer, record);

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(payer);
		if (character)
			character.LF_BroadcastStationSound("LF_FineTicketPayAction", SOUND_STAMP);

		if (receipt)
		{
			RplId itemId = EL_NetworkUtils.GetRplId(receipt.GetOwner());
			PlayPaperLater(payer, itemId);
			SCR_PlayerController controller = LF_PlayerInteraction.ControllerOf(payer);
			if (controller)
				controller.LF_TicketCue(itemId);
		}

		EL_NotificationManagerComponent.NotifyPlayer(payerId, "#LF-Ticket_Title", "#LF-Ticket_PaidToast", 5.0, EL_ENotificationType.SUCCESS, EL_MoneyFormat.FormatMoney(record.m_iAmount));
		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! A delayed sound from a fresh item: the call queue cannot carry an EntityID, so the pending
//! cue is an object the queue calls back. Kept alive by s_aPending until it plays.
class LF_TicketPaperSound : Managed
{
	protected static ref array<ref LF_TicketPaperSound> s_aPending;

	protected EntityID m_Holder;
	protected RplId m_Item;
	protected string m_sEvent;

	//------------------------------------------------------------------------------------------------
	static void Schedule(IEntity holder, RplId item, string soundEvent, int delayMs)
	{
		if (!holder || !GetGame() || !GetGame().GetCallqueue())
			return;

		LF_TicketPaperSound cue = new LF_TicketPaperSound();
		cue.m_Holder = holder.GetID();
		cue.m_Item = item;
		cue.m_sEvent = soundEvent;
		if (!s_aPending)
			s_aPending = new array<ref LF_TicketPaperSound>();

		s_aPending.Insert(cue);
		GetGame().GetCallqueue().CallLater(cue.Play, delayMs);
	}

	//------------------------------------------------------------------------------------------------
	protected void Play()
	{
		IEntity holder;
		if (GetGame() && GetGame().GetWorld())
			holder = GetGame().GetWorld().FindEntityByID(m_Holder);

		SCR_InventoryStorageManagerComponent inventory;
		if (holder)
			inventory = EL_Component<SCR_InventoryStorageManagerComponent>.Find(holder);

		if (inventory)
			inventory.RpcAsk_PlaySound(m_Item, m_sEvent);

		if (s_aPending)
			s_aPending.RemoveItem(this);
	}
}

//------------------------------------------------------------------------------------------------
//! "Pay ticket" on the punch clock of the police station desk. Shown while the user carries an
//! unpaid ticket; the name reads the number and the amount owed now. The server pays from the
//! records, not from the paper.
class LF_FineTicketPayAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_FineTicketItem.FirstUnpaid(user) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_FineTicketComponent ticket = LF_FineTicketItem.FirstUnpaid(user);
		if (!ticket)
			return false;

		if (EL_MoneyUtils.GetCash(user) >= ticket.GetAmount())
			return true;

		SetCannotPerformReason(WidgetManager.Translate("#LF-Ticket_NeedCash", EL_MoneyFormat.FormatMoney(ticket.GetAmount())));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		LF_FineTicketComponent ticket = LF_FineTicketItem.FirstUnpaid(EL_Utils.GetLocalCharacter());
		if (!ticket)
			return false;

		outName = WidgetManager.Translate("#LF-Ticket_Pay", ticket.GetNumber().ToString(4), EL_MoneyFormat.FormatMoney(ticket.GetAmount()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_FineTicketRole.PayAtDesk(pUserEntity);
	}
}

//------------------------------------------------------------------------------------------------
//! The ticket pad on the officer's machine: one button per offence at its fixed amount. Offences
//! above the officer's rank cap are printed as a plain line. Client-only, on LF_UIScreen.
class LF_FineTicketPad
{
	static const string ROW_PREFIX = "TicketOffence";
	static const float CLOSE_DISTANCE = 4;

	protected static ref LF_FineTicketPad s_Pad;

	protected ref LF_UIScreen m_Screen;
	protected SCR_PlayerController m_Controller;
	protected RplId m_Target;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts(int cap)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "TicketPadTitle", "#LF-Ticket_PadTitle"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "TicketPadTo", "#LF-Ticket_PadTo"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "TicketPadPick", "#LF-Ticket_PadPick"));
		array<ref LF_TicketOffence> offences = {};
		LF_PoliceConfig.TicketOffences(offences);
		foreach (int i, LF_TicketOffence offence : offences)
		{
			string name = WidgetManager.Translate(offence.m_sName);
			string amount = EL_MoneyFormat.FormatMoney(offence.m_iAmount);
			if (offence.m_iAmount > cap)
				parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, RowName(i) + "Locked", WidgetManager.Translate("#LF-Ticket_PadAboveRank", name, amount)));
			else
				parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, RowName(i), WidgetManager.Translate("#LF-Ticket_PadWrite", name, amount)));
		}

		int minutes = LF_FineTicketRules.MinutesLeft(LF_PoliceConfig.TicketFirstDeadline());
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "TicketPadNote", WidgetManager.Translate("#LF-Ticket_PadNote", minutes)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "TicketPadClose", "#LF-Ticket_PadClose"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	static string RowName(int index)
	{
		return ROW_PREFIX + index;
	}

	//------------------------------------------------------------------------------------------------
	//! The offence index a row name stands for, or -1.
	static int IndexOf(string rowName)
	{
		if (rowName.IndexOf(ROW_PREFIX) != 0)
			return -1;

		int prefixLength = ROW_PREFIX.Length();
		string digits = rowName.Substring(prefixLength, rowName.Length() - prefixLength);
		if (digits.IsEmpty())
			return -1;

		string allowed = "0123456789";
		for (int i = 0; i < digits.Length(); i++)
		{
			if (!allowed.Contains(digits.Get(i)))
				return -1;
		}

		return digits.ToInt();
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open(SCR_PlayerController controller, RplId target, int cap)
	{
		Hide();
		if (!controller)
		{
			EL_Debug.Error(LF_FineTickets.FEATURE, "ticket pad refused: no player controller");
			return null;
		}

		LF_FineTicketPad pad = new LF_FineTicketPad();
		pad.m_Controller = controller;
		pad.m_Target = target;
		pad.m_Screen = LF_UIScreen.Open(BuildParts(cap));
		if (!pad.m_Screen)
		{
			EL_Debug.Error(LF_FineTickets.FEATURE, "ticket pad failed to open");
			return null;
		}

		pad.m_Screen.GetOnButton().Insert(pad.OnButton);
		pad.m_Screen.CloseOnDeath();
		pad.m_Screen.CloseWhenUncontrolled();
		IEntity suspect = EL_NetworkUtils.FindEntityByRplId(target);
		if (suspect)
			pad.m_Screen.CloseBeyond(CLOSE_DISTANCE, suspect.GetID());
		else
			EL_Debug.Warn(LF_FineTickets.FEATURE, "ticket pad open without the person in this client's world; no distance close");

		s_Pad = pad;
		EL_Debug.Info(LF_FineTickets.FEATURE, string.Format("ticket pad open, cap %1", cap));
		return pad.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		if (!s_Pad)
			return null;

		return s_Pad.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Pad)
			return;

		LF_FineTicketPad pad = s_Pad;
		s_Pad = null;
		if (pad.m_Screen)
			pad.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		int index = IndexOf(name);
		if (index < 0)
			return;

		SCR_PlayerController controller = m_Controller;
		RplId target = m_Target;
		Hide();
		if (!controller)
		{
			EL_Debug.Error(LF_FineTickets.FEATURE, "ticket not sent: no player controller");
			return;
		}

		EL_Debug.Info(LF_FineTickets.FEATURE, "ticket pad signed: offence " + index);
		controller.LF_AskWriteTicket(target, index);
	}
}

//------------------------------------------------------------------------------------------------
//! Client <-> server bridge for the ticket, on the PlayerController.
//!   server -> owner : LF_OpenTicketPad, LF_TicketCue
//!   owner -> server : LF_AskWriteTicket
//! The server takes the officer from this controller's controlled entity, never from the client.
modded class SCR_PlayerController
{
	static const int LF_TICKET_CUE_TRIES = 10;
	static const int LF_TICKET_CUE_RETRY_MS = 200;

	//------------------------------------------------------------------------------------------------
	//! Server: open the ticket pad on this officer's machine.
	void LF_OpenTicketPad(RplId target, int cap)
	{
		if (LF_IsLocalInteraction())
			RpcDo_LF_OpenTicketPad(target, cap);
		else
			Rpc(RpcDo_LF_OpenTicketPad, target, cap);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_OpenTicketPad(RplId target, int cap)
	{
		LF_FineTicketPad.Open(this, target, cap);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: write offenceIndex on a ticket for the character with RplId target.
	void LF_AskWriteTicket(RplId target, int offenceIndex)
	{
		if (Replication.IsServer())
			RpcAsk_LF_WriteTicket(target, offenceIndex);
		else
			Rpc(RpcAsk_LF_WriteTicket, target, offenceIndex);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_WriteTicket(RplId target, int offenceIndex)
	{
		IEntity suspect = EL_NetworkUtils.FindEntityByRplId(target);
		LF_FineTicketRole.Write(GetControlledEntity(), suspect, offenceIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the holder's hand takes the paper and the card opens on their screen.
	void LF_TicketCue(RplId item)
	{
		if (LF_IsLocalInteraction())
			RpcDo_LF_TicketCue(item);
		else
			Rpc(RpcDo_LF_TicketCue, item);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_TicketCue(RplId item)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(GetControlledEntity());
		if (character)
		{
			CharacterControllerComponent controller = character.GetCharacterController();
			if (controller && controller.CanPlayItemGesture())
				controller.TryPlayItemGesture(EItemGesture.EItemGesturePickUp);
		}

		LF_ShowTicketCard(item, LF_TICKET_CUE_TRIES);
	}

	//------------------------------------------------------------------------------------------------
	//! Owner: opens the card once the ticket's printed fields have arrived.
	protected void LF_ShowTicketCard(RplId item, int triesLeft)
	{
		IEntity entity = EL_NetworkUtils.FindEntityByRplId(item);
		LF_FineTicketComponent ticket;
		if (entity)
			ticket = LF_FineTicketComponent.Cast(entity.FindComponent(LF_FineTicketComponent));

		if (ticket && ticket.IsPrinted())
		{
			LF_FineTicketCard.Open(ticket, null);
			return;
		}

		if (triesLeft <= 0 || !GetGame() || !GetGame().GetCallqueue())
		{
			EL_Debug.Warn(LF_FineTickets.FEATURE, "ticket card not opened: the printed ticket did not reach this machine");
			return;
		}

		GetGame().GetCallqueue().CallLater(LF_ShowTicketCard, LF_TICKET_CUE_RETRY_MS, false, item, triesLeft - 1);
	}
}
