//------------------------------------------------------------------------------------------------
//! Notification types for different use cases
enum LF_ENotificationType
{
	INFO,			// General information (blue/white)
	SUCCESS,		// Success messages (green)
	WARNING,		// Warnings (yellow/orange)
	ERROR,			// Errors (red)
	POLICE_ALERT,	// Police-specific alerts (blue with icon)
	EMS_ALERT,		// EMS-specific alerts (red with medical icon)
	ROBBERY_ALERT,	// Robbery alerts (red with warning icon)
	SMS_RECEIVED,	// SMS notification (custom color)
	CUSTOM			// Fully custom notification
}

//------------------------------------------------------------------------------------------------
//! Notification configuration class
class LF_NotificationConfig
{
	string m_sTitle;
	string m_sMessage;
	string m_sMessageArg;
	float m_fDuration;
	LF_ENotificationType m_eType;
	string m_sIconPath;
	Color m_Color;
	bool m_bShowSound;
	
	//------------------------------------------------------------------------------------------------
	void LF_NotificationConfig(string title, string message, float duration = 5.0, LF_ENotificationType type = LF_ENotificationType.INFO)
	{
		m_sTitle = title;
		m_sMessage = message;
		m_sMessageArg = "";
		m_fDuration = duration;
		m_eType = type;
		m_bShowSound = true;
		
		// Set default color based on type
		switch (type)
		{
			case LF_ENotificationType.SUCCESS:
				m_Color = Color.FromInt(Color.GREEN);
				break;
			case LF_ENotificationType.WARNING:
				m_Color = Color.FromInt(Color.ORANGE);
				break;
			case LF_ENotificationType.ERROR:
			case LF_ENotificationType.ROBBERY_ALERT:
			case LF_ENotificationType.EMS_ALERT:
				m_Color = Color.FromInt(Color.RED);
				break;
			case LF_ENotificationType.POLICE_ALERT:
				m_Color = Color.FromSRGBA(0, 100, 200, 255); // Blue
				break;
			case LF_ENotificationType.SMS_RECEIVED:
				m_Color = Color.FromSRGBA(150, 50, 200, 255); // Purple
				break;
			default:
				m_Color = Color.FromInt(Color.WHITE);
				break;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void SetCustomColor(int r, int g, int b, int a = 255)
	{
		m_Color = Color.FromSRGBA(r, g, b, a);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetIcon(string iconPath)
	{
		m_sIconPath = iconPath;
	}
	
	//------------------------------------------------------------------------------------------------
	void SetSound(bool playSound)
	{
		m_bShowSound = playSound;
	}
}

//------------------------------------------------------------------------------------------------
//! Notification Manager Component - Server and Client
[ComponentEditorProps(category: "EveronLife/Notifications", description: "Manages custom notifications")]
class LF_NotificationManagerComponentClass : ScriptComponentClass
{
}

class LF_NotificationManagerComponent : ScriptComponent
{
	protected static LF_NotificationManagerComponent s_Instance;
	
	//------------------------------------------------------------------------------------------------
	static LF_NotificationManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			// Try to get from game mode (same pattern as SCR_HintManagerComponent)
			BaseGameMode gameMode = GetGame().GetGameMode();
			if (gameMode)
				s_Instance = LF_NotificationManagerComponent.Cast(gameMode.FindComponent(LF_NotificationManagerComponent));
		}
		
		return s_Instance;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		// Initialize singleton if this is attached to the game mode
		// This works for any GameMode type, not just SCR_BaseGameMode
		if (GetGame().GetGameMode() == owner)
		{
			s_Instance = this;
			LF_Debug.Log("Notify", "notification manager initialized");
		}
		else
		{
			LF_Debug.Warn("Notify", string.Format("notification manager attached to %1 instead of the game mode", owner));
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
		{
			s_Instance = null;
			LF_Debug.Log("Notify", "notification manager shut down");
		}
			
		super.OnDelete(owner);
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	//--- SERVER-SIDE METHODS (Authority only)
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	//------------------------------------------------------------------------------------------------
	//! Send notification to specific player
	//! \param playerId Target player ID
	//! \param config Notification configuration
	void SendToPlayer(int playerId, LF_NotificationConfig config)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Error("Notify", "SendToPlayer refused: called on a client");
			return;
		}
			
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!controller)
		{
			LF_Debug.Error("Notify", string.Format("notify to player %1 failed: no player controller", playerId));
			return;
		}

		// Owner RPC on the target's own controller: only that client receives the payload.
		// The old Broadcast-and-filter sent every player's private toast to every client.
		LF_Debug.Log("Notify", string.Format("notify player %1: %2", playerId, config.m_sMessage));
		controller.LF_DeliverNotification(config.m_sTitle, config.m_sMessage, config.m_fDuration, config.m_eType, config.m_sMessageArg);
	}

	//------------------------------------------------------------------------------------------------
	//! Send notification to every connected on-duty police officer. Police is the account faction
	//! (LF_PlayerAccount), the same definition the robbery minimum-police gate counts.
	//! \return Officers notified.
	int SendToPolice(LF_NotificationConfig config)
	{
		if (!Replication.IsServer())
			return 0;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (!playerManager || !accountManager)
			return 0;

		int sent = 0;
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			LF_PlayerAccount account = accountManager.GetAccount(LF_Utils.GetPlayerUID(playerId));
			if (!LF_AccountStateBits.IsOnDutyPolice(LF_AccountStateBits.FromAccount(account)))
				continue;

			SendToPlayer(playerId, config);
			sent++;
		}

		LF_Debug.Log("Notify", string.Format("police alert sent to %1 officer(s): %2", sent, config.m_sMessage));
		return sent;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Send notification to all players with specific job (only if they are on duty)
	//! \param jobType Target job type
	//! \param config Notification configuration
	//! \param onDutyOnly If true, only send to players who are on duty
	void SendToJob(LF_EJobType jobType, LF_NotificationConfig config, bool onDutyOnly = true)
	{
		if (!Replication.IsServer())
			return;
			
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		
		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		
		foreach (int playerId : playerIds)
		{
			IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!playerEntity)
				continue;
				
			LF_PlayerJobComponent jobComp = LF_Component<LF_PlayerJobComponent>.Find(playerEntity);
			if (!jobComp)
				continue;
				
			// Check if player has the target job
			if (jobComp.GetJob() != jobType)
				continue;
				
			// Duty lives on the account, not the job component: a police officer can be off duty
			// while holding the POLICE job. The old UNEMPLOYED check was a tautology (the job
			// already matched) and let off-duty officers receive alerts.
			if (onDutyOnly)
			{
				LF_PlayerAccount account = accountManager.GetAccount(LF_Utils.GetPlayerUID(playerEntity));
				if (!account || !account.IsOnDuty())
					continue;
			}
				
			SendToPlayer(playerId, config);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Send notification to all players
	//! \param config Notification configuration
	void SendToAll(LF_NotificationConfig config)
	{
		if (!Replication.IsServer())
			return;
			
		Rpc(RpcDo_ShowNotification, config.m_sTitle, config.m_sMessage, config.m_fDuration, config.m_eType, config.m_sIconPath, config.m_bShowSound, -1, config.m_sMessageArg);
		RpcDo_ShowNotification(config.m_sTitle, config.m_sMessage, config.m_fDuration, config.m_eType, config.m_sIconPath, config.m_bShowSound, -1, config.m_sMessageArg);
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	//--- STATIC HELPER METHODS
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	//------------------------------------------------------------------------------------------------
	//! Send notification to specific player (static)
	static void NotifyPlayer(int playerId, string title, string message, float duration = 5.0, LF_ENotificationType type = LF_ENotificationType.INFO, string messageArg = "")
	{
		LF_NotificationManagerComponent manager = GetInstance();
		if (!manager)
		{
			LF_Debug.Error("Notify", string.Format("notify to player %1 dropped: no LF_NotificationManagerComponent on the game mode", playerId));
			return;
		}
		
		
		LF_NotificationConfig config = new LF_NotificationConfig(title, message, duration, type);
		config.m_sMessageArg = messageArg;
		manager.SendToPlayer(playerId, config);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Send notification to all players with specific job (static)
	static void NotifyJob(LF_EJobType jobType, string title, string message, float duration = 5.0, LF_ENotificationType type = LF_ENotificationType.INFO, bool onDutyOnly = true, string messageArg = "")
	{
		LF_NotificationManagerComponent manager = GetInstance();
		if (!manager)
			return;
			
		LF_NotificationConfig config = new LF_NotificationConfig(title, message, duration, type);
		config.m_sMessageArg = messageArg;
		manager.SendToJob(jobType, config, onDutyOnly);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Send notification to every connected on-duty police officer (static, server).
	static void NotifyPolice(string title, string message, float duration = 5.0, LF_ENotificationType type = LF_ENotificationType.POLICE_ALERT, string messageArg = "")
	{
		LF_NotificationManagerComponent manager = GetInstance();
		if (!manager)
		{
			LF_Debug.Error("Notify", "police alert dropped: no LF_NotificationManagerComponent on the game mode");
			return;
		}

		LF_NotificationConfig config = new LF_NotificationConfig(title, message, duration, type);
		config.m_sMessageArg = messageArg;
		manager.SendToPolice(config);
	}

	//------------------------------------------------------------------------------------------------
	//! Send notification to all players (static)
	static void NotifyAll(string title, string message, float duration = 5.0, LF_ENotificationType type = LF_ENotificationType.INFO, string messageArg = "")
	{
		LF_NotificationManagerComponent manager = GetInstance();
		if (!manager)
			return;
			
		LF_NotificationConfig config = new LF_NotificationConfig(title, message, duration, type);
		config.m_sMessageArg = messageArg;
		manager.SendToAll(config);
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	//--- RPC METHODS
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	//------------------------------------------------------------------------------------------------
	//! Server->all clients (SendToAll only, playerId -1). Player-targeted toasts never use this:
	//! they go through the target's controller (SCR_PlayerController.LF_DeliverNotification).
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ShowNotification(string title, string message, float duration, LF_ENotificationType type, string iconPath, bool playSound, int playerId, string messageArg)
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		if (playerId > 0)
		{
			PlayerController playerController = GetGame().GetPlayerController();
			if (!playerController)
				return;

			if (playerId != playerController.GetPlayerId())
				return;
		}

		// Client-side: Show custom notification toast
		ShowNotificationToast(title, message, duration, type, messageArg);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Show notification toast on this machine. Called by the owner-RPC on the local controller.
	void ShowNotificationToast(string title, string message, float duration, LF_ENotificationType type, string messageArg = "")
	{
		// Add prefix based on type if no custom title
		// TODO: these fallback titles are Spanish literals; move to localization keys in the
		// localization phase.
		string displayTitle = title;
		if (title.IsEmpty())
		{
			switch (type)
			{
				case LF_ENotificationType.POLICE_ALERT:
					displayTitle = "[POLICÍA] ALERTA";
					break;
				case LF_ENotificationType.EMS_ALERT:
					displayTitle = "[EMS] EMERGENCIA";
					break;
				case LF_ENotificationType.ROBBERY_ALERT:
					displayTitle = "[ALARMA] ROBO";
					break;
				case LF_ENotificationType.SMS_RECEIVED:
					displayTitle = "[SMS] MENSAJE";
					break;
				case LF_ENotificationType.SUCCESS:
					displayTitle = "[ÉXITO]";
					break;
				case LF_ENotificationType.WARNING:
					displayTitle = "[ADVERTENCIA]";
					break;
				case LF_ENotificationType.ERROR:
					displayTitle = "[ERROR]";
					break;
				default:
					displayTitle = "[INFO]";
					break;
			}
		}
		else if (displayTitle.StartsWith("#"))
		{
			displayTitle = WidgetManager.Translate(displayTitle);
		}

		string displayMessage = message;
		if (displayMessage.StartsWith("#"))
		{
			// The argument can be a key too (e.g. "#LF-Duty_OnDuty"); translate it on the client.
			string displayArg = messageArg;
			if (displayArg.StartsWith("#"))
				displayArg = WidgetManager.Translate(displayArg);
			displayMessage = WidgetManager.Translate(displayMessage, displayArg);
		}
		
		// Create notification widget on HUD layer
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
		{
			LF_Debug.Error("Notify", "toast dropped: no workspace");
			return;
		}
		
		Widget hudParent = workspace.GetParent();
		Widget notificationContainer = null;
		
		// Try to find the container in the Beta HUD first
		if (hudParent)
		{
			notificationContainer = hudParent.FindAnyWidget("NotificationContainer");
		}
		
		// If not found, try workspace level (fallback)
		if (!notificationContainer)
		{
			notificationContainer = workspace.FindAnyWidget("NotificationContainer");
		}
		
		// If still not found, use hudParent or workspace as fallback
		Widget parent = notificationContainer;
		if (!parent)
		{
			if (hudParent)
				parent = hudParent;
			else
				parent = workspace;
				
			LF_Debug.Warn("Notify", "NotificationContainer not found, using fallback parent");
		}
		
		Widget toastWidget = workspace.CreateWidgets("{4C38FD68D45325D2}UI/Layouts/Notifications/EL_NotificationToast.layout", parent);
		if (!toastWidget)
		{
			LF_Debug.Error("Notify", "toast dropped: failed to create the notification widget");
			return;
		}
		
		
		// Get handler and show
		LF_NotificationToastUI handler = LF_NotificationToastUI.Cast(toastWidget.FindHandler(LF_NotificationToastUI));
		if (handler)
		{
			handler.Show(displayTitle, displayMessage, type, duration);
		}
		else
		{
			LF_Debug.Error("Notify", "toast dropped: no LF_NotificationToastUI handler on the widget");
		}
	}
}