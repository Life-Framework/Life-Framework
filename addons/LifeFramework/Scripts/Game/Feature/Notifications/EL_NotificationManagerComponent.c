//------------------------------------------------------------------------------------------------
//! Notification types for different use cases
enum EL_ENotificationType
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
class EL_NotificationConfig
{
	string m_sTitle;
	string m_sMessage;
	float m_fDuration;
	EL_ENotificationType m_eType;
	string m_sIconPath;
	Color m_Color;
	bool m_bShowSound;
	
	//------------------------------------------------------------------------------------------------
	void EL_NotificationConfig(string title, string message, float duration = 5.0, EL_ENotificationType type = EL_ENotificationType.INFO)
	{
		m_sTitle = title;
		m_sMessage = message;
		m_fDuration = duration;
		m_eType = type;
		m_bShowSound = true;
		
		// Set default color based on type
		switch (type)
		{
			case EL_ENotificationType.SUCCESS:
				m_Color = Color.FromInt(Color.GREEN);
				break;
			case EL_ENotificationType.WARNING:
				m_Color = Color.FromInt(Color.ORANGE);
				break;
			case EL_ENotificationType.ERROR:
			case EL_ENotificationType.ROBBERY_ALERT:
			case EL_ENotificationType.EMS_ALERT:
				m_Color = Color.FromInt(Color.RED);
				break;
			case EL_ENotificationType.POLICE_ALERT:
				m_Color = Color.FromSRGBA(0, 100, 200, 255); // Blue
				break;
			case EL_ENotificationType.SMS_RECEIVED:
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
class EL_NotificationManagerComponentClass : ScriptComponentClass
{
}

class EL_NotificationManagerComponent : ScriptComponent
{
	protected static EL_NotificationManagerComponent s_Instance;
	
	//------------------------------------------------------------------------------------------------
	static EL_NotificationManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			// Try to get from game mode (same pattern as SCR_HintManagerComponent)
			BaseGameMode gameMode = GetGame().GetGameMode();
			if (gameMode)
				s_Instance = EL_NotificationManagerComponent.Cast(gameMode.FindComponent(EL_NotificationManagerComponent));
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
			Print("[EL_NotificationManagerComponent] ✓ Notification manager initialized successfully", LogLevel.NORMAL);
		}
		else
		{
			Print(string.Format("[EL_NotificationManagerComponent] WARNING: Component attached to %1 instead of GameMode", owner), LogLevel.WARNING);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
		{
			s_Instance = null;
			Print("[EL_NotificationManagerComponent] Notification manager shut down", LogLevel.NORMAL);
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
	void SendToPlayer(int playerId, EL_NotificationConfig config)
	{
		if (!Replication.IsServer())
		{
			Print("[EL_NotificationManagerComponent] ERROR: SendToPlayer called on client! Only server can send notifications.", LogLevel.ERROR);
			return;
		}
			
		PlayerController controller = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!controller)
		{
			Print(string.Format("[EL_NotificationManagerComponent] ERROR: PlayerController not found for player %1", playerId), LogLevel.ERROR);
			return;
		}
		
		Print(string.Format("[EL_NotificationManagerComponent] ✅ Sending notification to player %1: '%2'", playerId, config.m_sTitle), LogLevel.NORMAL);
		
		Rpc(RPC_ShowNotification, config.m_sTitle, config.m_sMessage, config.m_fDuration, config.m_eType, config.m_sIconPath, config.m_bShowSound);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Send notification to all players with specific job (only if they are on duty)
	//! \param jobType Target job type
	//! \param config Notification configuration
	//! \param onDutyOnly If true, only send to players who are on duty (not UNEMPLOYED)
	void SendToJob(EL_EJobType jobType, EL_NotificationConfig config, bool onDutyOnly = true)
	{
		if (!Replication.IsServer())
			return;
			
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		
		foreach (int playerId : playerIds)
		{
			IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!playerEntity)
				continue;
				
			EL_PlayerJobComponent jobComp = EL_Component<EL_PlayerJobComponent>.Find(playerEntity);
			if (!jobComp)
				continue;
				
			// Check if player has the target job
			if (jobComp.GetJob() != jobType)
				continue;
				
			// If onDutyOnly is true, skip players who are UNEMPLOYED (off duty)
			if (onDutyOnly && jobComp.GetJob() == EL_EJobType.UNEMPLOYED)
				continue;
				
			SendToPlayer(playerId, config);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Send notification to all players
	//! \param config Notification configuration
	void SendToAll(EL_NotificationConfig config)
	{
		if (!Replication.IsServer())
			return;
			
		Rpc(RPC_ShowNotification, config.m_sTitle, config.m_sMessage, config.m_fDuration, config.m_eType, config.m_sIconPath, config.m_bShowSound);
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	//--- STATIC HELPER METHODS
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	//------------------------------------------------------------------------------------------------
	//! Send notification to specific player (static)
	static void NotifyPlayer(int playerId, string title, string message, float duration = 5.0, EL_ENotificationType type = EL_ENotificationType.INFO)
	{
		EL_NotificationManagerComponent manager = GetInstance();
		if (!manager)
		{
			Print(string.Format("[EL_NotificationManagerComponent] ERROR: Instance not found! Cannot send notification to player %1", playerId), LogLevel.ERROR);
			return;
		}
		
		Print(string.Format("[EL_NotificationManagerComponent] Sending notification to player %1: %2", playerId, title), LogLevel.NORMAL);
		
		EL_NotificationConfig config = new EL_NotificationConfig(title, message, duration, type);
		manager.SendToPlayer(playerId, config);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Send notification to all players with specific job (static)
	static void NotifyJob(EL_EJobType jobType, string title, string message, float duration = 5.0, EL_ENotificationType type = EL_ENotificationType.INFO, bool onDutyOnly = true)
	{
		EL_NotificationManagerComponent manager = GetInstance();
		if (!manager)
			return;
			
		EL_NotificationConfig config = new EL_NotificationConfig(title, message, duration, type);
		manager.SendToJob(jobType, config, onDutyOnly);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Send notification to all players (static)
	static void NotifyAll(string title, string message, float duration = 5.0, EL_ENotificationType type = EL_ENotificationType.INFO)
	{
		EL_NotificationManagerComponent manager = GetInstance();
		if (!manager)
			return;
			
		EL_NotificationConfig config = new EL_NotificationConfig(title, message, duration, type);
		manager.SendToAll(config);
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////////
	//--- RPC METHODS
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	//------------------------------------------------------------------------------------------------
	// ✅ FIX: Changed from Broadcast to Owner to send only to target player
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RPC_ShowNotification(string title, string message, float duration, EL_ENotificationType type, string iconPath, bool playSound)
	{
		Print(string.Format("[EL_NotificationManagerComponent] ✅ CLIENT received notification: '%1' - '%2'", title, message), LogLevel.NORMAL);
		
		// Client-side: Show custom notification toast
		ShowNotificationToast(title, message, duration, type);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Show notification toast on client
	protected void ShowNotificationToast(string title, string message, float duration, EL_ENotificationType type)
	{
		// Add prefix based on type if no custom title
		string displayTitle = title;
		if (title.IsEmpty())
		{
			switch (type)
			{
				case EL_ENotificationType.POLICE_ALERT:
					displayTitle = "[POLICÍA] ALERTA";
					break;
				case EL_ENotificationType.EMS_ALERT:
					displayTitle = "[EMS] EMERGENCIA";
					break;
				case EL_ENotificationType.ROBBERY_ALERT:
					displayTitle = "[ALARMA] ROBO";
					break;
				case EL_ENotificationType.SMS_RECEIVED:
					displayTitle = "[SMS] MENSAJE";
					break;
				case EL_ENotificationType.SUCCESS:
					displayTitle = "[ÉXITO]";
					break;
				case EL_ENotificationType.WARNING:
					displayTitle = "[ADVERTENCIA]";
					break;
				case EL_ENotificationType.ERROR:
					displayTitle = "[ERROR]";
					break;
				default:
					displayTitle = "[INFO]";
					break;
			}
		}
		
		// Create notification widget on HUD layer
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
		{
			Print("[EL_NotificationManagerComponent] ERROR: No workspace found", LogLevel.ERROR);
			return;
		}
		
		// ✅ FIX: Add parent widget to ensure proper rendering and Z-order
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
				
			Print("[EL_NotificationManagerComponent] WARNING: NotificationContainer not found, using fallback parent", LogLevel.WARNING);
		}
		
		Widget toastWidget = workspace.CreateWidgets("{4C38FD68D45325D2}UI/Layouts/Notifications/EL_NotificationToast.layout", parent);
		if (!toastWidget)
		{
			Print("[EL_NotificationManagerComponent] ❌ ERROR: Failed to create notification widget", LogLevel.ERROR);
			return;
		}
		
		Print(string.Format("[EL_NotificationManagerComponent] ✅ Widget created successfully for: '%1'", displayTitle), LogLevel.NORMAL);
		
		// Get handler and show
		EL_NotificationToastUI handler = EL_NotificationToastUI.Cast(toastWidget.FindHandler(EL_NotificationToastUI));
		if (handler)
		{
			Print(string.Format("[EL_NotificationManagerComponent] ✅ Handler found, showing notification: '%1'", displayTitle), LogLevel.NORMAL);
			handler.Show(displayTitle, message, type, duration);
		}
		else
		{
			Print("[EL_NotificationManagerComponent] ❌ ERROR: No handler found on notification widget", LogLevel.ERROR);
		}
	}
}
