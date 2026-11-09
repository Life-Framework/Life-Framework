//------------------------------------------------------------------------------------------------
//! One callable service from Configs/Comms/LF_ServiceCalls.conf.
[BaseContainerProps()]
class LF_ServiceDef
{
	[Attribute("", UIWidgets.EditBox, "Service key callers pick, e.g. police")]
	string m_sKey;

	[Attribute("", UIWidgets.EditBox, "Localization key shown to the caller, e.g. #LF-Service_Police")]
	string m_sName;

	[Attribute("", UIWidgets.EditBox, "Who receives: 'duty:police' (on-duty police officers) or 'job:<LF_EJobType number>' (players currently in that job)")]
	string m_sReceiver;

	[Attribute("", UIWidgets.Auto, "Delivery channels: pager, board, radio")]
	ref array<string> m_aChannels;

	[Attribute("1", UIWidgets.Auto, "How many responders may take one call (#251)")]
	int m_iMaxTakers;

	static LF_ServiceDef Make(string key, string name, string receiver, string channels)
	{
		LF_ServiceDef def = new LF_ServiceDef();
		def.m_sKey = key;
		def.m_sName = name;
		def.m_sReceiver = receiver;
		def.m_aChannels = {};
		channels.Split(",", def.m_aChannels, true);
		return def;
	}
}

//------------------------------------------------------------------------------------------------
//! The service list. A missing or broken file logs once and falls back to police, medic, taxi.
[BaseContainerProps(configRoot: true)]
class LF_ServiceCallsConfig
{
	static const ResourceName CONFIG_PATH = "{4C81E7A2D9035B6F}Configs/Comms/LF_ServiceCalls.conf";

	[Attribute("", UIWidgets.Object, "Callable services")]
	ref array<ref LF_ServiceDef> m_aServices;

	static const int DEFAULT_PAYPHONE_COST = 1;
	static const int DEFAULT_PLAYER_COOLDOWN = 60;
	static const int DEFAULT_PHONE_COOLDOWN = 30;

	[Attribute("1", UIWidgets.Auto, "Cash a payphone call takes")]
	int m_iPayphoneCost;

	[Attribute("60", UIWidgets.Auto, "Seconds before the same player may use any payphone again")]
	int m_iPlayerCooldownSeconds;

	[Attribute("30", UIWidgets.Auto, "Seconds before the same payphone may be used again by anyone")]
	int m_iPhoneCooldownSeconds;

	static const int DEFAULT_BOARD_EXPIRY = 900;
	static const int DEFAULT_PAGER_MESSAGES = 5;

	[Attribute("900", UIWidgets.Auto, "Seconds a call stays on a dispatch board before it expires (#250)")]
	int m_iBoardExpirySeconds;

	[Attribute("5", UIWidgets.Auto, "Messages a pager keeps, newest first (#249)")]
	int m_iPagerMessages;

	protected static ref LF_ServiceCallsConfig s_Config;
	protected static ref array<ref LF_ServiceDef> s_aDefaults;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_aDefaults = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	static int PayphoneCost()
	{
		Load();
		if (!s_Config)
			return DEFAULT_PAYPHONE_COST;
		return s_Config.m_iPayphoneCost;
	}

	//------------------------------------------------------------------------------------------------
	static int PagerMessages()
	{
		Load();
		if (!s_Config || s_Config.m_iPagerMessages <= 0)
			return DEFAULT_PAGER_MESSAGES;
		return s_Config.m_iPagerMessages;
	}

	//------------------------------------------------------------------------------------------------
	static int BoardExpirySeconds()
	{
		Load();
		if (!s_Config)
			return DEFAULT_BOARD_EXPIRY;
		return s_Config.m_iBoardExpirySeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Responders allowed to take one call to this service; at least 1.
	static int MaxTakers(string service)
	{
		LF_ServiceDef def = LF_ServiceCalls.Find(Services(), service);
		if (!def)
			return 1;
		return Math.Max(def.m_iMaxTakers, 1);
	}

	//------------------------------------------------------------------------------------------------
	static int PlayerCooldown()
	{
		Load();
		if (!s_Config)
			return DEFAULT_PLAYER_COOLDOWN;
		return s_Config.m_iPlayerCooldownSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static int PhoneCooldown()
	{
		Load();
		if (!s_Config)
			return DEFAULT_PHONE_COOLDOWN;
		return s_Config.m_iPhoneCooldownSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_ServiceDef> Services()
	{
		Load();
		if (s_Config)
			return s_Config.m_aServices;

		if (!s_aDefaults)
		{
			s_aDefaults = {};
			s_aDefaults.Insert(LF_ServiceDef.Make("police", "#LF-Service_Police", "duty:police", "radio,board,pager"));
			s_aDefaults[0].m_iMaxTakers = 2;
			s_aDefaults.Insert(LF_ServiceDef.Make("medic", "#LF-Service_Medic", "job:4", "pager,board"));
			s_aDefaults.Insert(LF_ServiceDef.Make("taxi", "#LF-Service_Taxi", "job:7", "pager,board"));
		}
		return s_aDefaults;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Load()
	{
		if (!s_bLoadAttempted)
		{
			s_bLoadAttempted = true;
			Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
			if (container && container.GetResource())
				s_Config = LF_ServiceCallsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

			if (!s_Config || !s_Config.m_aServices || s_Config.m_aServices.IsEmpty())
			{
				LF_Debug.Error("ServiceCalls", string.Format("service list missing or empty, using defaults: %1", CONFIG_PATH));
				s_Config = null;
			}
		}
	}
}

//------------------------------------------------------------------------------------------------
//! A connected player as routing sees them. Built from live state by LF_ServiceCalls.Candidates;
//! tests build them by hand.
class LF_ServiceCandidate
{
	int m_iPlayerId;
	bool m_bPoliceOnDuty;
	int m_iJob;

	static LF_ServiceCandidate Make(int playerId, bool policeOnDuty, int job)
	{
		LF_ServiceCandidate c = new LF_ServiceCandidate();
		c.m_iPlayerId = playerId;
		c.m_bPoliceOnDuty = policeOnDuty;
		c.m_iJob = job;
		return c;
	}
}

//------------------------------------------------------------------------------------------------
//! The outcome of routing one call.
class LF_ServiceRoute
{
	string m_sService;
	//! The service key is in the list.
	bool m_bKnown;
	ref array<int> m_aReceivers = {};
	string m_sChannels;

	//! Somebody on duty will get the call.
	bool IsAnswered()
	{
		return m_bKnown && !m_aReceivers.IsEmpty();
	}
}

//------------------------------------------------------------------------------------------------
//! Calls route to any service in the config (#248). Place checks the service exists and who is on
//! duty for it, logs the call, and publishes LF_OnServiceCall on the bus. It never calls dispatch
//! itself; dispatch and the delivery channels subscribe. Server only.
class LF_ServiceCalls
{
	static const string FEATURE = "ServiceCalls";
	static const string NO_ANSWER_KEY = "#LF-Service_NoAnswer";

	//------------------------------------------------------------------------------------------------
	static LF_ServiceDef Find(notnull array<ref LF_ServiceDef> services, string key)
	{
		foreach (LF_ServiceDef def : services)
		{
			if (def && def.m_sKey == key)
				return def;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: does this player receive calls for the service right now?
	static bool Receives(notnull LF_ServiceDef def, notnull LF_ServiceCandidate player)
	{
		if (def.m_sReceiver == "duty:police")
			return player.m_bPoliceOnDuty;

		if (def.m_sReceiver.StartsWith("job:"))
			return player.m_iJob == def.m_sReceiver.Substring(4, def.m_sReceiver.Length() - 4).ToInt();

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: who a call to this service reaches. An unknown service is rejected with an Error.
	static LF_ServiceRoute Route(notnull array<ref LF_ServiceDef> services, string serviceKey, notnull array<ref LF_ServiceCandidate> players)
	{
		LF_ServiceRoute route = new LF_ServiceRoute();
		route.m_sService = serviceKey;
		LF_ServiceDef def = Find(services, serviceKey);
		if (!def)
		{
			LF_Debug.Error(FEATURE, string.Format("call rejected: unknown service '%1'", serviceKey));
			return route;
		}

		route.m_bKnown = true;
		foreach (string channel : def.m_aChannels)
		{
			if (!route.m_sChannels.IsEmpty())
				route.m_sChannels += ",";
			route.m_sChannels += channel;
		}

		foreach (LF_ServiceCandidate player : players)
		{
			if (player && Receives(def, player))
				route.m_aReceivers.Insert(player.m_iPlayerId);
		}
		return route;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: who a call to this service would reach right now. Publishes nothing.
	static LF_ServiceRoute Check(string serviceKey, array<ref LF_ServiceCandidate> players = null)
	{
		// A strong local: assigning to the parameter would free the fresh array on return.
		ref array<ref LF_ServiceCandidate> list = players;
		if (!list)
			list = Candidates();

		return Route(LF_ServiceCallsConfig.Services(), serviceKey, list);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: route a call and publish it. The caller shows NO_ANSWER_KEY when the result is not answered.
	//! \return The route; null off the server.
	static LF_ServiceRoute Place(string callerUid, string serviceKey, string place, string summary, vector position, array<ref LF_ServiceCandidate> players = null)
	{
		if (!Replication.IsServer())
			return null;

		LF_ServiceRoute route = Check(serviceKey, players);
		if (!route.m_bKnown)
			return route;

		if (!route.IsAnswered())
		{
			LF_Debug.Warn(FEATURE, string.Format("%1 called %2 from '%3': no answer, nobody on duty", callerUid, serviceKey, place));
			return route;
		}

		LF_Debug.Info(FEATURE, string.Format("%1 called %2 from '%3': %4 receiver(s) via %5", callerUid, serviceKey, place, route.m_aReceivers.Count(), route.m_sChannels));
		LF_EventBus.Publish(LF_OnServiceCall.Create(callerUid, serviceKey, place, summary, route.m_sChannels, position, route.m_aReceivers));
		return route;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every connected player with their duty state.
	static array<ref LF_ServiceCandidate> Candidates()
	{
		array<ref LF_ServiceCandidate> result = {};
		if (!GetGame() || !GetGame().GetPlayerManager())
			return result;

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			bool police;
			if (accounts)
				police = LF_PoliceUtils.IsOnDutyOfficer(accounts.GetAccount(LF_Utils.GetPlayerUID(playerId)));

			int job = LF_EJobType.UNEMPLOYED;
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (character)
			{
				LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(character);
				if (jobs)
					job = jobs.GetJob();
			}

			result.Insert(LF_ServiceCandidate.Make(playerId, police, job));
		}
		return result;
	}
}
