//------------------------------------------------------------------------------------------------
//! An invisible carrier of a calendar sound (#4477, WO-W6.2): the church bell on the hour, the
//! Sunday peal, the shift siren, the ship's horn. It is an empty entity (no mesh) a pack places at
//! the tower, the factory roof or the quay; the sound plays from its position. Never build one on a
//! visible prop. The carrier only says WHICH calendar operation it answers to and WHAT it plays; the
//! island calendar (LF_IslandCalendar) says WHEN. The core names no place and no sound: the pack
//! ships the prefabs and the .acp projects.
[ComponentEditorProps(category: "LifeFramework/Feature/Atmosphere", description: "Invisible carrier of an island calendar sound: the operation id it answers to and the sound it plays.")]
class LF_CalendarCarrierComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_CalendarCarrierComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "The calendar operation this carrier answers to: calendar_bell, calendar_service, calendar_siren or calendar_horn")]
	protected string m_sOperationId;

	[Attribute("", UIWidgets.ResourceNamePicker, "Sound project with the event", "acp")]
	protected ResourceName m_sSoundProject;

	[Attribute("SOUND_LF_AMBIENT_LP", UIWidgets.EditBox, "Sound event in the project")]
	protected string m_sSoundEvent;

	[Attribute("8", UIWidgets.Auto, "Seconds the sample runs; the voice is stopped after this, so a looped event cannot run on")]
	protected float m_fSeconds;

	//! Carriers alive on this machine (the server has them too, to count them).
	protected static ref array<LF_CalendarCarrierComponent> s_aAll = {};

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_aAll.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		s_aAll.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	string OperationId()
	{
		return m_sOperationId;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName SoundProject()
	{
		return m_sSoundProject;
	}

	//------------------------------------------------------------------------------------------------
	string SoundEvent()
	{
		return m_sSoundEvent;
	}

	//------------------------------------------------------------------------------------------------
	float Seconds()
	{
		return m_fSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static array<LF_CalendarCarrierComponent> GetAll()
	{
		return s_aAll;
	}

	//------------------------------------------------------------------------------------------------
	//! How many carriers answer to this operation on this machine.
	static int CountFor(string operationId)
	{
		int count;
		foreach (LF_CalendarCarrierComponent carrier : s_aAll)
		{
			if (carrier && carrier.m_sOperationId == operationId)
				count++;
		}

		return count;
	}
}

//------------------------------------------------------------------------------------------------
//! The calendar operation of a sound row: the server runs it when the calendar says the hour is
//! here and tells every client to play the carriers that answer to its id. Server only.
class LF_CalendarSoundOperation : LF_EventOperation
{
	string m_sOperationId;

	//------------------------------------------------------------------------------------------------
	void LF_CalendarSoundOperation(string operationId)
	{
		m_sOperationId = operationId;
	}

	//------------------------------------------------------------------------------------------------
	//! One broadcast per ring. \return true when the row was handled, also when no carrier is placed
	//! yet (said once): a row waiting for a carrier would be tried again every second.
	override bool Run(string eventId, string phaseName, LF_EventContext ctx)
	{
		if (!Replication.IsServer())
			return false;

		int carriers = LF_CalendarCarrierComponent.CountFor(m_sOperationId);
		if (carriers == 0)
		{
			LF_CalendarSound.WarnOnce(m_sOperationId, string.Format("calendar %1 is due and no carrier answers to it; it stays silent until a place card puts one in", m_sOperationId));
			return true;
		}

		LF_GameModeRoleplay mode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (!mode)
		{
			LF_Debug.Error(LF_CalendarSound.FEATURE, string.Format("calendar %1 skipped: no roleplay game mode to announce on", m_sOperationId));
			return false;
		}

		LF_Debug.Info(LF_CalendarSound.FEATURE, string.Format("calendar %1 rings (row %2): %3 carrier(s)", m_sOperationId, phaseName, carriers));
		mode.LF_AnnounceCalendarSound(m_sOperationId);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The calendar's sounds (#4477): registers the four operations on the event shell and plays a
//! ring on the machine that hears it. A dedicated server plays nothing.
class LF_CalendarSound
{
	static const string FEATURE = "Calendar";
	static const string OP_BELL = "calendar_bell";
	static const string OP_SERVICE = "calendar_service";
	static const string OP_SIREN = "calendar_siren";
	static const string OP_HORN = "calendar_horn";
	//! Carriers farther than this from the listener are not started: past the project's range anyway.
	static const float HEAR_M = 200;	// lf-fixed: the sound projects fade out by 120 m
	static const int STOP_PAD_MS = 500;	// lf-fixed: a little after the sample, so its tail is not cut

	protected static ref set<string> s_aWarned;
	protected static int s_iPlayed;

	//------------------------------------------------------------------------------------------------
	//! Server, game start: puts the four sound operations on the event shell. Safe to call twice.
	static void RegisterOperations()
	{
		RegisterOne(OP_BELL);
		RegisterOne(OP_SERVICE);
		RegisterOne(OP_SIREN);
		RegisterOne(OP_HORN);
	}

	//------------------------------------------------------------------------------------------------
	protected static void RegisterOne(string id)
	{
		if (LF_EventShell.FindOperation(id))
			return;

		LF_EventShell.RegisterOperation(id, new LF_CalendarSoundOperation(id));
	}

	//------------------------------------------------------------------------------------------------
	static void WarnOnce(string key, string message)
	{
		if (!s_aWarned)
			s_aWarned = new set<string>();
		if (s_aWarned.Contains(key))
			return;

		s_aWarned.Insert(key);
		LF_Debug.Warn(FEATURE, message);
	}

	//------------------------------------------------------------------------------------------------
	//! This machine: starts every carrier of the operation that is near enough to be heard.
	//! \return how many voices started (0 on a dedicated server).
	static int PlayLocal(string operationId)
	{
		if (RplSession.Mode() == RplMode.Dedicated || System.IsConsoleApp())
			return 0;

		vector listener;
		bool haveListener = ListenerPosition(listener);
		int started;
		foreach (LF_CalendarCarrierComponent carrier : LF_CalendarCarrierComponent.GetAll())
		{
			if (!carrier || carrier.OperationId() != operationId || !carrier.GetOwner())
				continue;

			vector at = carrier.GetOwner().GetOrigin();
			float distance;
			if (haveListener)
			{
				distance = vector.Distance(at, listener);
				if (distance > HEAR_M)
					continue;
			}

			if (PlayAt(carrier, at, distance))
				started++;
		}

		return started;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ListenerPosition(out vector position)
	{
		if (!GetGame())
			return false;

		CameraManager cameras = GetGame().GetCameraManager();
		if (!cameras)
			return false;

		CameraBase camera = cameras.CurrentCamera();
		if (!camera)
			return false;

		position = camera.GetOrigin();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool PlayAt(LF_CalendarCarrierComponent carrier, vector at, float distance)
	{
		if (carrier.SoundProject().IsEmpty() || carrier.SoundEvent().IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("carrier %1 has no sound project or event; it stays silent", carrier.OperationId()));
			return false;
		}

		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = at;
		AudioHandle handle = AudioSystem.PlayEvent(carrier.SoundProject(), carrier.SoundEvent(), mat);
		if (handle == AudioHandle.Invalid)
		{
			LF_Debug.Warn(FEATURE, string.Format("carrier %1 did not start at %2 m (out of earshot here, or %3 failed to load)", carrier.OperationId(), distance.ToString(-1, 1), FilePath.StripPath(carrier.SoundProject())));
			return false;
		}

		s_iPlayed++;
		GetGame().GetCallqueue().CallLater(Stop, carrier.Seconds() * 1000 + STOP_PAD_MS, false, handle);
		LF_Debug.Info(FEATURE, string.Format("play %1 at %2 m: %3", carrier.OperationId(), distance.ToString(-1, 1), FilePath.StripPath(carrier.SoundProject())));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stop(AudioHandle handle)
	{
		if (handle != AudioHandle.Invalid)
			AudioSystem.TerminateSound(handle);
	}

	//------------------------------------------------------------------------------------------------
	//! Voices started on this machine since the world began (a session proof and the test read it).
	static int PlayedCount()
	{
		return s_iPlayed;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aWarned = null;
		s_iPlayed = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_CalendarSound's statics (LF_StaticReset finds it by reflection).
class LF_CalendarSound_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_CalendarSound";
	}

	override void Apply()
	{
		LF_CalendarSound.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Registers the sound operations at game start and carries a ring from the server to the clients
//! on the game mode, without editing LF_GameModeRoleplay.c (M4 rule).
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer())
			LF_CalendarSound.RegisterOperations();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every client plays the carriers of `operationId`. A listen-server host plays them directly.
	void LF_AnnounceCalendarSound(string operationId)
	{
		if (!Replication.IsServer())
			return;

		Rpc(RpcDo_LF_CalendarSound, operationId);
		if (RplSession.Mode() != RplMode.Dedicated)
			RpcDo_LF_CalendarSound(operationId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_CalendarSound(string operationId)
	{
		LF_CalendarSound.PlayLocal(operationId);
	}
}
