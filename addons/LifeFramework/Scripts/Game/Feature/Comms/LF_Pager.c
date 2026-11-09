[ComponentEditorProps(category: "LifeFramework/Feature/Comms", description: "A pager: keeps the last service calls paged to its carrier and beeps on each (#249).")]
class LF_PagerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The pager item. The server writes a message per call (LF_Pager.Page); the last N (config
//! `m_iPagerMessages`) ride one short replicated string, newest first, each as the board entry
//! "time|summary|0|place", so a late joiner and a client that picks the pager up read the same
//! list. The beep is one broadcast; its audio source is the pager, so the carrier and anyone
//! beside them hear it.
class LF_PagerComponent : ScriptComponent
{
	static const string MESSAGE_SEPARATOR = "~";

	[Attribute("SOUND_RADIO_SIGNAL_2", UIWidgets.EditBox, "Sound event on the pager's SCR_SoundDataComponent: the beep")]
	protected string m_sBeepSound;

	[RplProp()]
	protected string m_sMessages;

	//------------------------------------------------------------------------------------------------
	//! Newest first.
	void GetMessages(notnull array<string> outMessages)
	{
		outMessages.Clear();
		if (!m_sMessages.IsEmpty())
			m_sMessages.Split(MESSAGE_SEPARATOR, outMessages, true);
	}

	//------------------------------------------------------------------------------------------------
	int MessageCount()
	{
		array<string> messages = {};
		GetMessages(messages);
		return messages.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: store one message (newest first, trimmed to maxMessages) and beep.
	void Receive(string entry, int maxMessages)
	{
		if (!Replication.IsServer())
			return;

		array<string> messages = {};
		GetMessages(messages);
		messages.InsertAt(entry, 0);
		while (messages.Count() > Math.Max(maxMessages, 1))
		{
			messages.Remove(messages.Count() - 1);
		}

		m_sMessages = string.Empty;
		foreach (int index, string message : messages)
		{
			if (index > 0)
				m_sMessages += MESSAGE_SEPARATOR;
			m_sMessages += message;
		}

		Replication.BumpMe();
		Rpc(RpcDo_Beep);
		RpcDo_Beep();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_Beep()
	{
		// A dedicated server renders and hears nothing.
		if (RplSession.Mode() == RplMode.Dedicated || !GetOwner() || m_sBeepSound.IsEmpty())
			return;

		SCR_SoundManagerModule.CreateAndPlayAudioSource(GetOwner(), m_sBeepSound);
	}
}

//------------------------------------------------------------------------------------------------
//! Paging: a call whose channels include "pager" is written to the pager of every receiver who
//! carries one. Receivers are the people the service reaches right now (on-duty police, a current
//! medic or taxi driver), so only on-duty staff are paged.
class LF_Pager
{
	static const string FEATURE = "Pager";
	static const string CHANNEL = "pager";

	//------------------------------------------------------------------------------------------------
	//! Pure: does this channel list include the pager?
	static bool HasPagerChannel(string channels)
	{
		array<string> list = {};
		channels.Split(",", list, true);
		return list.Find(CHANNEL) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the stored form of a call, "time|summary|0|place" (the board entry format).
	static string Entry(string time, string summary, string place)
	{
		return string.Format("%1|%2|0|%3", time, summary, place);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: write the call to each pager once and beep it. The seam tests drive.
	//! \return Pagers written.
	static int Page(notnull LF_OnServiceCall call, notnull array<LF_PagerComponent> pagers, string time)
	{
		string entry = Entry(time, call.m_sSummary, call.m_sPlace);
		int written;
		foreach (LF_PagerComponent pager : pagers)
		{
			if (!pager)
				continue;

			pager.Receive(entry, LF_ServiceCallsConfig.PagerMessages());
			written++;
		}

		LF_Debug.Info(FEATURE, string.Format("%1 call '%2' paged to %3 pager(s) for %4 receiver(s)", call.m_sService, call.m_sSummary, written, call.m_aReceiverIds.Count()));
		return written;
	}

	//------------------------------------------------------------------------------------------------
	//! Every pager in a character's inventory or hands, each once.
	static void FindCarried(IEntity character, notnull array<LF_PagerComponent> outPagers)
	{
		if (!character)
			return;

		array<IEntity> items = {};
		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(character);
		if (inventory)
			inventory.GetItems(items);

		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			LF_PagerComponent pager = LF_PagerComponent.Cast(item.FindComponent(LF_PagerComponent));
			if (pager && outPagers.Find(pager) < 0)
				outPagers.Insert(pager);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the pagers carried by the call's receivers.
	static void ReceiverPagers(notnull LF_OnServiceCall call, notnull array<LF_PagerComponent> outPagers)
	{
		if (!GetGame() || !GetGame().GetPlayerManager())
			return;

		foreach (int playerId : call.m_aReceiverIds)
		{
			FindCarried(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId), outPagers);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the card of a pager's messages (no UI), so a test reads what a player would see.
	static array<ref LF_UIPart> BuildParts(notnull array<string> messages)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", "#LF-Pager_Title"));
		if (messages.IsEmpty())
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "None", "#LF-Pager_None"));
		}
		else
		{
			foreach (int index, string message : messages)
			{
				string line = LF_ServiceBoard.EntryLine(message);
				if (!line.IsEmpty())
					parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Message" + index, line));
			}
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-Sign_Close"));
		return parts;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: pages the carriers' pagers for calls whose channels include "pager".
class LF_PagerSubscriber : LF_EventSubscriber
{
	static ref LF_PagerSubscriber s_Subscriber;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnServiceCall call = LF_OnServiceCall.Cast(busEvent);
		if (!call)
			return false;

		if (!LF_Pager.HasPagerChannel(call.m_sChannels))
			return true;

		array<LF_PagerComponent> pagers = {};
		LF_Pager.ReceiverPagers(call, pagers);
		LF_Pager.Page(call, pagers, LF_Dispatch.ClockText());
		return true;
	}

	static void Install()
	{
		if (!Replication.IsServer())
			return;

		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnServiceCall, s_Subscriber);

		s_Subscriber = new LF_PagerSubscriber();
		LF_EventBus.Subscribe(LF_OnServiceCall, s_Subscriber);
	}

	static void ResetStatics()
	{
		s_Subscriber = null;
	}
}

//------------------------------------------------------------------------------------------------
//! Look at the pager: opens the card of its last messages. Reading is local; the messages are
//! already replicated on the item.
class LF_ReadPagerAction : ScriptedUserAction
{
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Pager_Read";
		return true;
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		LF_PagerComponent pager = LF_PagerComponent.Cast(pOwnerEntity.FindComponent(LF_PagerComponent));
		if (!pager)
			return;

		array<string> messages = {};
		pager.GetMessages(messages);
		LF_Debug.Info(LF_Pager.FEATURE, string.Format("pager read: %1 message(s)", messages.Count()));
		LF_WantedBoard.OpenParts(LF_Pager.BuildParts(messages));
	}
}
