[ComponentEditorProps(category: "LifeFramework/Feature/Comms", description: "A police scanner: receives the service calls, never transmits (#253).")]
class LF_ScannerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The scanner item. It listens to every service whose config has a radio frequency (police,
//! medic) and beeps when a call goes out. The beep is one broadcast from the item, heard by the
//! carrier and anyone beside them. It has no transmit: nothing here sends.
class LF_ScannerComponent : ScriptComponent
{
	[Attribute("SOUND_RADIO_SIGNAL_2", UIWidgets.EditBox, "Sound event on the scanner's SCR_SoundDataComponent: the chirp")]
	protected string m_sBeepSound;

	//------------------------------------------------------------------------------------------------
	//! Server: one chirp for one heard call.
	void Chirp()
	{
		if (!Replication.IsServer())
			return;

		Rpc(RpcDo_Chirp);
		RpcDo_Chirp();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_Chirp()
	{
		// A dedicated server renders and hears nothing.
		if (RplSession.Mode() == RplMode.Dedicated || !GetOwner() || m_sBeepSound.IsEmpty())
			return;

		SCR_SoundManagerModule.CreateAndPlayAudioSource(GetOwner(), m_sBeepSound);
	}
}

//------------------------------------------------------------------------------------------------
//! The law around scanners (#253): legality is config (`m_bScannerIllegal` in LF_ServiceCalls.conf).
//! A search that finds scanners on a person adds the offence once per search, however many it finds.
class LF_ScannerLaw
{
	//------------------------------------------------------------------------------------------------
	//! The offence keys a search adds. \p found is the number of scanners on the person.
	static void SearchOffences(int found, bool illegal, notnull array<string> outOffences)
	{
		outOffences.Clear();
		if (found > 0 && illegal)
			outOffences.Insert(LF_WantedReports.OFFENCE_SCANNER);
	}
}

//------------------------------------------------------------------------------------------------
class LF_Scanner
{
	static const string FEATURE = "Scanner";
	static const ResourceName PREFAB = "{D763DE7A23C5D1FB}Prefabs/Items/Fence/LF_Scanner.et";

	//------------------------------------------------------------------------------------------------
	//! True when a call goes out on a service that has a radio frequency, which a scanner hears.
	static bool Hears(notnull LF_OnServiceCall call)
	{
		return LF_ServiceCallsConfig.Frequency(call.m_sService) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Every scanner in a character's inventory or hands, each once.
	static void FindCarried(IEntity character, notnull array<LF_ScannerComponent> outScanners)
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

			LF_ScannerComponent scanner = LF_ScannerComponent.Cast(item.FindComponent(LF_ScannerComponent));
			if (scanner && outScanners.Find(scanner) < 0)
				outScanners.Insert(scanner);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a call reaches the scanners of players who are not its receivers. Each listener
	//! hears once however many scanners they carry, gets a toast, and `LF_OnScannerHeard` goes out.
	//! \param listenerIds Candidate player ids with their uid, parallel arrays; tests drive this directly.
	//! \return Listeners who heard the call.
	static int Hear(notnull LF_OnServiceCall call, notnull array<string> listenerUids, notnull array<ref array<LF_ScannerComponent>> carried, notnull array<IEntity> bodies)
	{
		if (!Hears(call))
			return 0;

		int heard;
		foreach (int i, string uid : listenerUids)
		{
			if (carried[i].IsEmpty())
				continue;

			foreach (LF_ScannerComponent scanner : carried[i])
			{
				scanner.Chirp();
				break;
			}

			if (bodies[i])
				LF_Utils.NotifyEntity(bodies[i], "#LF-Scanner_Heard", "#LF-Scanner_Title", 5.0, call.m_sSummary);

			LF_EventBus.Publish(LF_OnScannerHeard.Create(uid, call.m_sService, call.m_sSummary, call.m_sPlace));
			LF_Debug.Info(FEATURE, string.Format("%1 call '%2' heard on a scanner by %3", call.m_sService, call.m_sSummary, uid));
			heard++;
		}

		return heard;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gathers the connected players who carry a scanner and are not receivers of the call.
	static int HearLive(notnull LF_OnServiceCall call)
	{
		if (!GetGame() || !GetGame().GetPlayerManager() || !Hears(call))
			return 0;

		array<string> uids = {};
		array<ref array<LF_ScannerComponent>> carried = {};
		array<IEntity> bodies = {};
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (call.m_aReceiverIds.Find(playerId) >= 0)
				continue;

			IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			array<LF_ScannerComponent> scanners = {};
			FindCarried(body, scanners);
			if (scanners.IsEmpty())
				continue;

			uids.Insert(LF_Utils.GetPlayerUID(playerId));
			carried.Insert(scanners);
			bodies.Insert(body);
		}

		return Hear(call, uids, carried, bodies);
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: every service call is offered to the scanners.
class LF_ScannerSubscriber : LF_EventSubscriber
{
	static ref LF_ScannerSubscriber s_Subscriber;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnServiceCall call = LF_OnServiceCall.Cast(busEvent);
		if (!call)
			return false;

		LF_Scanner.HearLive(call);
		return true;
	}

	static void Install()
	{
		if (!Replication.IsServer())
			return;

		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnServiceCall, s_Subscriber);

		s_Subscriber = new LF_ScannerSubscriber();
		LF_EventBus.Subscribe(LF_OnServiceCall, s_Subscriber);
	}

	static void ResetStatics()
	{
		s_Subscriber = null;
	}
}
