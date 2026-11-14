class LF_SirenManagerComponentClass : ScriptComponentClass
{
}

/**
\brief Manages siren modes, light animations and knob procedural animation
**/
class LF_SirenManagerComponent : ScriptComponent
{
	// Sound volume when on/off
	protected const float SOUND_OFF = 0;
	protected const float SOUND_ON = 1;

	[Attribute()]
	protected ref LF_SirenModes m_Modes;

	[Attribute("0", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(LF_Faction), desc: "Service this vehicle belongs to: only an on-duty member of it may run the siren (police vehicle: police, ambulance: medic). Civilian: nobody (#464)")]
	protected LF_Faction m_eService;

	[Attribute("", UIWidgets.ResourceNamePicker, "Sound project of this vehicle's own siren loop (#923). Set: it plays in every mode but default, and the shared vehicle siren (Vehicles_Siren.acp) stays silent on this vehicle.", "acp")]
	protected ResourceName m_sLoopProject;

	[Attribute("", UIWidgets.Auto, "Event of the loop in m_sLoopProject")]
	protected string m_sLoopEvent;

	//! Client only: the loop that plays now, and whether the mode wants one.
	protected ref SCR_AudioSource m_LoopAudio;
	protected bool m_bLoopOn;

	// Used for sending only the necessary signals to the sound system
	protected ref LF_SirenMode m_CurrentMode;
	// Replicated: a client that streams the car in later or joins late gets the current value with
	// the snapshot and runs OnModeReplicated. A one-shot RPC reaches only streamed-in clients (#471).
	[RplProp(onRplName: "OnModeReplicated")]
	protected int m_CurrentModeIndex;
	// Server throttle state: world time of the last accepted change, and how many were broadcast.
	protected int m_iLastChangeMs;
	protected bool m_bHasChanged;
	protected int m_iBroadcasts;

	protected LF_SirenKnobComponent m_Knob;
	protected SignalsManagerComponent m_KnobSigComp;

	// Signals manager of the vehicle, drives the audio graph signals
	protected SignalsManagerComponent m_SigComp;
	// Sound component of the vehicle
	protected SoundComponent m_SoundComp;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		EventHandlerManagerComponent ev = EventHandlerManagerComponent.Cast(owner.FindComponent(EventHandlerManagerComponent));
		if (ev)
		{
			ev.RegisterScriptHandler("OnCompartmentEntered", this, OnCompartmentEntered);
			ev.RegisterScriptHandler("OnCompartmentLeft", this, OnCompartmentLeft);
		}

		m_SoundComp = SoundComponent.Cast(owner.FindComponent(SoundComponent));
		m_SigComp = SignalsManagerComponent.Cast(owner.FindComponent(SignalsManagerComponent));
		if (m_Modes)
		{
			m_CurrentModeIndex = m_Modes.Find("default");
			if (m_CurrentModeIndex < 0)
			{
				m_CurrentModeIndex = 0;
				LF_Debug.Warn("Siren", "no 'default' mode in config, falling back to index 0");
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCompartmentLeft(IEntity vehicle, BaseCompartmentManagerComponent mgr, IEntity occupant, int managerId, int slotID)
	{
		BaseCompartmentSlot slot = mgr.FindCompartment(slotID);
		if (!slot || slot.GetType() != ECompartmentType.PILOT)
			return;

		// Disable controls if pilot compartment was left
		GetGame().GetInputManager().RemoveActionListener("LF_CycleSirenModesSingle", EActionTrigger.DOWN, CycleModesSingle);
		GetGame().GetInputManager().RemoveActionListener("LF_CycleSirenModesDouble", EActionTrigger.DOWN, CycleModesDouble);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCompartmentEntered(IEntity vehicle, BaseCompartmentManagerComponent mgr, IEntity occupant, int managerId, int slotID)
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return;

		BaseCompartmentSlot slot = mgr.FindCompartment(slotID);
		if (!slot || slot.GetType() != ECompartmentType.PILOT)
			return;

		// Enable siren controls if current player entered the pilot compartment
		if (playerController.GetControlledEntity() == occupant)
		{
			GetGame().GetInputManager().AddActionListener("LF_CycleSirenModesSingle", EActionTrigger.DOWN, CycleModesSingle);
			GetGame().GetInputManager().AddActionListener("LF_CycleSirenModesDouble", EActionTrigger.DOWN, CycleModesDouble);
		}
	}

	//------------------------------------------------------------------------------------------------
	// \brief Broadcasts the next mode to the server
	protected void CycleModesSingle()
	{
		if (!m_CurrentMode || !m_Modes) return;
		RequestMode(m_Modes.Find(m_CurrentMode.GetSingleClickMode()));
	}

	//------------------------------------------------------------------------------------------------
	// \brief Broadcasts the next mode to the server
	protected void CycleModesDouble()
	{
		if (!m_CurrentMode || !m_Modes) return;
		RequestMode(m_Modes.Find(m_CurrentMode.GetDoubleClickMode()));
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestMode(int mode)
	{
		if (Replication.IsServer())
			RpcAsk_Authority_Method(mode);
		else
			Rpc(RpcAsk_Authority_Method, mode);
	}

	//------------------------------------------------------------------------------------------------
	// \brief Server side code. Set current mode and broadcasts to all clients
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Authority_Method(int mode)
	{
		RequestBy(PilotOf(GetOwner()), mode, GetGame().GetWorld().GetWorldTime());
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity PilotOf(IEntity vehicle)
	{
		if (!vehicle)
			return null;

		BaseVehicleControllerComponent controller = BaseVehicleControllerComponent.Cast(vehicle.FindComponent(BaseVehicleControllerComponent));
		if (!controller)
			return null;

		PilotCompartmentSlot pilot = controller.GetPilotCompartmentSlot();
		if (!pilot)
			return null;

		return pilot.GetOccupant();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one toggle request from the vehicle's occupant. Only an on-duty member of the vehicle's
	//! service may run the siren: a thief in a stolen police car drives it but gets no siren (#464).
	//! A refusal is logged. An allowed request goes through ApplyRequest.
	bool RequestBy(IEntity occupant, int mode, int nowMs)
	{
		bool hasAccount;
		LF_Faction faction;
		bool onDuty;
		string uid = LF_Utils.GetPlayerUID(occupant);
		if (!uid.IsEmpty())
		{
			LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
			LF_PlayerAccount account;
			if (accounts)
				account = accounts.GetAccount(uid);

			if (account)
			{
				hasAccount = true;
				faction = account.GetFaction();
				onDuty = account.IsOnDuty();
			}
		}

		if (!LF_SirenRules.DutyAllows(m_eService, hasAccount, faction, onDuty))
		{
			LF_Debug.Warn("Siren", string.Format("toggle to mode %1 refused: occupant '%2' is not on duty for service %3", mode, uid, typename.EnumToString(LF_Faction, m_eService)));
			return false;
		}

		return ApplyRequest(mode, nowMs);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one toggle request. A mode that is out of range, equal to the current one, or asked for
	//! within MIN_TOGGLE_MS of the last accepted change is refused and nothing is broadcast, so a held
	//! or spammed key replicates once per change (#263).
	//! \param nowMs World time of the request.
	//! \return True when the mode changed and was broadcast.
	bool ApplyRequest(int mode, int nowMs)
	{
		int count = 0;
		if (m_Modes)
			count = m_Modes.GetModes().Count();

		LF_ESirenRequest verdict = LF_SirenRules.Judge(mode, count, m_CurrentModeIndex, nowMs, m_iLastChangeMs, m_bHasChanged);
		if (verdict != LF_ESirenRequest.ACCEPT)
		{
			LF_Debug.Info("Siren", string.Format("toggle to mode %1 refused: %2", mode, typename.EnumToString(LF_ESirenRequest, verdict)));
			return false;
		}

		m_CurrentModeIndex = mode;
		m_iLastChangeMs = nowMs;
		m_bHasChanged = true;
		m_iBroadcasts++;
		SetMode();
		Replication.BumpMe();
		LF_Debug.Info("Siren", string.Format("mode %1 broadcast (change #%2)", mode, m_iBroadcasts));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Mode changes this vehicle has broadcast since it spawned (server).
	int GetBroadcastCount()
	{
		return m_iBroadcasts;
	}

	//------------------------------------------------------------------------------------------------
	//! True while a mode other than "default" (siren and lights off) is running.
	bool IsOn()
	{
		return m_CurrentMode && m_CurrentMode.GetName() != "default";
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the replicated mode changed or arrived with the snapshot (#471). SetMode tolerates an
	//! unset m_Modes and unregistered lights; the knob re-runs SetMode when it registers.
	protected void OnModeReplicated()
	{
		LF_Debug.Info("Siren", string.Format("replicated mode index %1", m_CurrentModeIndex));
		SetMode();
		if (m_CurrentMode)
		{
			LF_LightAnimation anim = m_CurrentMode.GetAnimation();
			if (anim) anim.Tick(GetSyncTime());
		}
	}

	//------------------------------------------------------------------------------------------------
	/**
	\brief Called when a light component registers itself
	\param light - the light that is registering itself
	**/
	void RegisterLight(LF_SirenLightComponent light)
	{
		if (m_Modes) m_Modes.InsertLight(light);
	}

	//------------------------------------------------------------------------------------------------
	/**
	\brief Called when a knob component registers itself
	\param knob - the knob that is registering itself
	**/
	void RegisterKnob(LF_SirenKnobComponent knob)
	{
		if (m_Knob) Print("Siren with multiple knobs", LogLevel.WARNING);
		else
		{
			m_Knob = knob;
			m_KnobSigComp = SignalsManagerComponent.Cast(knob.GetOwner().FindComponent(SignalsManagerComponent));
			SetMode();
		}
	}

	//------------------------------------------------------------------------------------------------
	// \brief Sets the signal values that select the horn/siren pair of the mode
	protected void SetSirenMode(LF_SirenMode mode)
	{
		if (!m_SigComp) return;

		if (m_CurrentMode)
		{
			SetSignal(m_CurrentMode.GetSirenActive() + "Active", SOUND_OFF);
			SetSignal(m_CurrentMode.GetSirenInactive() + "Inactive", SOUND_OFF);
		}

		SetSignal(mode.GetSirenActive() + "Active", SOUND_ON);
		SetSignal(mode.GetSirenInactive() + "Inactive", SOUND_ON);
	}

	//------------------------------------------------------------------------------------------------
	// \brief Sets a named signal on the vehicle signals manager
	protected void SetSignal(string name, float value)
	{
		if (!m_SigComp) return;
		m_SigComp.SetSignalValue(m_SigComp.FindSignal(name), value);
	}

	//------------------------------------------------------------------------------------------------
	// \brief Sets the signals of the knob procedural animation. Called from SetMode()
	// \param mode - the mode that is going to be set
	protected void SetKnobMode(LF_SirenMode mode)
	{
		LF_LightAnimation anim = mode.GetAnimation();
		if (anim)
		{
			anim.Reset();
			m_Knob.SetAnimation(anim);
		}

		if (m_KnobSigComp)
		{
			foreach (SignalValuePair sig : mode.GetKnobSignals())
			{
				m_KnobSigComp.SetSignalValue(m_KnobSigComp.FindSignal(sig.GetName()), sig.GetValue());
			}
		}

		if (m_SoundComp) m_SoundComp.SoundEventBone("SOUND_VEHICLE_CLOSE_LIGHT_ON", "Scene_Root");
	}

	//------------------------------------------------------------------------------------------------
	/**
	\brief Sets the current mode based on m_CurrentModeIndex. Changes knob position and siren sounds
	**/
	protected void SetMode()
	{
		if (!m_Modes) return;
		LF_SirenMode mode = m_Modes.GetMode(m_CurrentModeIndex);
		if (mode)
		{
			if (m_Knob) SetKnobMode(mode);

			bool ownSiren = HasOwnLoop();
			if (m_SoundComp && !ownSiren) SetSirenMode(mode);
			if (m_SoundComp && !ownSiren && mode.GetName() != "default")
			{
				SetSignal("animSelector", 1);
				m_SoundComp.SoundEventBone("SOUND_VEHICLE_HORN", "Scene_Root");
				SetSignal("animSelector", 0);
			}
		}

		m_CurrentMode = mode;

		string modeName = "<none>";
		if (mode)
			modeName = mode.GetName();

		UpdateLoop(modeName);
		LF_Debug.Info("Siren", string.Format("mode -> %1 (index %2)", modeName, m_CurrentModeIndex));
	}

	//------------------------------------------------------------------------------------------------
	/**
	\brief Sets mode by name. See SetMode()
	\param name - name of the mode
	**/
	void SetModeStr(string name)
	{
		if (!m_Modes) return;
		int nextModeIndex = m_Modes.Find(name);
		if (nextModeIndex == m_CurrentModeIndex) return;
		m_CurrentModeIndex = nextModeIndex;
		SetMode();
	}

	//------------------------------------------------------------------------------------------------
	//! True when this vehicle has its own siren sound (the ambulance's hi-lo, #923).
	bool HasOwnLoop()
	{
		return !m_sLoopProject.IsEmpty() && !m_sLoopEvent.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: start or stop the vehicle's own siren loop to match the mode. A dedicated server has
	//! no listener, so it plays nothing.
	protected void UpdateLoop(string modeName)
	{
		if (System.IsConsoleApp() || RplSession.Mode() == RplMode.Dedicated)
			return;

		bool wanted = LF_AmbulanceSiren.LoopWanted(HasOwnLoop(), modeName);
		if (wanted == m_bLoopOn)
			return;

		m_bLoopOn = wanted;
		if (wanted)
		{
			StartLoop();
			GetGame().GetCallqueue().CallLater(RetryLoop, 2000, true);
			return;
		}

		GetGame().GetCallqueue().Remove(RetryLoop);
		StopLoop();
	}

	//------------------------------------------------------------------------------------------------
	//! The listener was out of the siren's range when it started: try again until it is in range.
	protected void RetryLoop()
	{
		if (m_bLoopOn && !m_LoopAudio)
			StartLoop();
	}

	//------------------------------------------------------------------------------------------------
	protected void StartLoop()
	{
		if (m_LoopAudio)
			return;

		SCR_SoundManagerModule sound = SCR_SoundManagerModule.GetInstance(GetOwner().GetWorld());
		if (!sound)
		{
			LF_Debug.Warn("Siren", "no sound manager in this world; the siren is silent");
			return;
		}

		SCR_AudioSourceConfiguration config = new SCR_AudioSourceConfiguration();
		config.m_sSoundProject = m_sLoopProject;
		config.m_sSoundEventName = m_sLoopEvent;
		// Not static: the vehicle drives.
		config.m_eFlags = 0;

		// Null while the listener is out of the project's audible range; RetryLoop tries again.
		SCR_AudioSource audio = sound.CreateAudioSource(GetOwner(), config);
		if (!audio)
			return;

		sound.PlayAudioSource(audio);
		m_LoopAudio = audio;
		LF_Debug.Info("Siren", string.Format("own siren loop started: %1", m_sLoopEvent));
	}

	//------------------------------------------------------------------------------------------------
	protected void StopLoop()
	{
		if (!m_LoopAudio)
			return;

		m_LoopAudio.Terminate();
		m_LoopAudio = null;
		LF_Debug.Info("Siren", "own siren loop stopped");
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_bLoopOn)
			GetGame().GetCallqueue().Remove(RetryLoop);

		StopLoop();
		m_bLoopOn = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this user may run this vehicle's siren: an on-duty member of its service, read from the
	//! replicated account state so a client can decide what its action menu shows.
	bool MayOperate(IEntity user)
	{
		if (!user)
			return false;

		int state = LF_AccountReplication.GetState(user);
		return LF_SirenRules.DutyAllows(m_eService, true, LF_AccountStateBits.GetFaction(state), LF_AccountStateBits.IsOnDuty(state));
	}

	//------------------------------------------------------------------------------------------------
	//! True when the named mode is the one running now.
	bool IsModeName(string name)
	{
		return m_CurrentMode && m_CurrentMode.GetName() == name;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a driver's menu pick of a named mode. The same duty rule and throttle as the key.
	bool RequestModeStr(IEntity user, string name)
	{
		if (!Replication.IsServer() || !m_Modes)
			return false;

		return RequestBy(user, m_Modes.Find(name), GetGame().GetWorld().GetWorldTime());
	}

	//------------------------------------------------------------------------------------------------
	// \return synchronized world time in seconds, used to resume animation timing on JIP
	protected float GetSyncTime()
	{
		World world = GetGame().GetWorld();
		if (!world) return 0;
		return world.GetWorldTime() * 0.001;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplSave(ScriptBitWriter writer)
	{
		if (m_CurrentModeIndex < 0) m_CurrentModeIndex = 0;

		writer.WriteInt(1); // version
		writer.WriteIntRange(m_CurrentModeIndex, 0, LF_SirenMode.N_MODES);

		LF_LightAnimation anim = null;
		if (m_CurrentMode) anim = m_CurrentMode.GetAnimation();
		if (anim) writer.WriteFloat(anim.GetTimeInLoop());
		else writer.WriteFloat(0);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		int version;
		reader.ReadInt(version);
		if (version != 1) return false;

		float timeSlice;
		reader.ReadIntRange(m_CurrentModeIndex, 0, LF_SirenMode.N_MODES);
		SetMode();
		reader.ReadFloat(timeSlice);

		LF_LightAnimation anim = null;
		if (m_CurrentMode) anim = m_CurrentMode.GetAnimation();
		if (anim) anim.Tick(timeSlice + GetSyncTime());
		return true;
	}
}

/**
Contains all the information about the siren mode. Animation, sounds and knob procedural animation signals
**/
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_Name", "%1")]
class LF_SirenMode
{
	// Available modes to the user
	static const ref ParamEnumArray MODE_NAMES =
	{
		new ParamEnum("default", "0"),
		new ParamEnum("Mode1", "1"),
		new ParamEnum("Mode2", "2"),
		new ParamEnum("Mode3", "3"),
		new ParamEnum("Mode4", "4"),
		new ParamEnum("Mode5", "5"),
		new ParamEnum("Mode6", "6"),
		new ParamEnum("Mode7", "7")
	};

	// Number of modes available, used to reduce network traffic.
	static const int N_MODES = MODE_NAMES.Count();
	// Available sounds
	static const ref ParamEnumArray SIREN_SOUNDS =
	{
		new ParamEnum("Silent", "0"),
		new ParamEnum("DefaultHorn", "1"),
		new ParamEnum("SlowSiren", "2"),
		new ParamEnum("FastSiren", "3"),
		new ParamEnum("FastestSiren", "4"),
		new ParamEnum("WarningSiren", "5")
	};

	[Attribute("default", uiwidget: UIWidgets.ComboBox, enums: MODE_NAMES)]
	protected string m_Name;

	[Attribute("default", uiwidget: UIWidgets.ComboBox, enums: MODE_NAMES)]
	protected string m_NextModeSingleClick;

	[Attribute("default", uiwidget: UIWidgets.ComboBox, enums: MODE_NAMES)]
	protected string m_NextModeDoubleClick;

	[Attribute()]
	protected ref LF_LightAnimation m_Animation;

	[Attribute(desc: "Sound that should be played when the horn is pressed.", uiwidget: UIWidgets.ComboBox, enums: SIREN_SOUNDS)]
	protected string m_SoundWhenPressed;

	[Attribute(desc: "Sound that should be played when horn is released", uiwidget: UIWidgets.ComboBox, enums: SIREN_SOUNDS)]
	protected string m_SoundWhenReleased;

	[Attribute(desc: "Controls the knob position for each mode. Each pair contains the name of the procedural animation signal and it's value.")]
	protected ref array<ref SignalValuePair> m_KnobSignals;

	//------------------------------------------------------------------------------------------------
	string GetName()
	{
		return m_Name;
	}

	//------------------------------------------------------------------------------------------------
	string GetDoubleClickMode()
	{
		return m_NextModeDoubleClick;
	}

	//------------------------------------------------------------------------------------------------
	string GetSingleClickMode()
	{
		return m_NextModeSingleClick;
	}

	//------------------------------------------------------------------------------------------------
	string GetSirenActive()
	{
		return m_SoundWhenPressed;
	}

	//------------------------------------------------------------------------------------------------
	string GetSirenInactive()
	{
		return m_SoundWhenReleased;
	}

	//------------------------------------------------------------------------------------------------
	LF_LightAnimation GetAnimation()
	{
		return m_Animation;
	}

	//------------------------------------------------------------------------------------------------
	/**
	\brief Inserts a light into the animation. Called when a light registers itself
	\param light - light to be inserted
	**/
	void InsertLight(LF_SirenLightComponent light)
	{
		if (m_Animation) m_Animation.InsertLight(light);
	}

	//------------------------------------------------------------------------------------------------
	/**
	\brief Get the signal value pairs of the knob procedural animation attached to this mode
	\return signal value pairs
	**/
	array<ref SignalValuePair> GetKnobSignals()
	{
		return m_KnobSignals;
	}
}

/**
Holds name and value of a procedural animation signal.
**/
[BaseContainerProps()]
class SignalValuePair
{
	[Attribute(desc: "Name of the signal to send to the procedural animation.")]
	protected string m_Name;

	[Attribute(desc: "Value of the procedural animation signal specified above.")]
	protected float m_Value;

	//------------------------------------------------------------------------------------------------
	string GetName()
	{
		return m_Name;
	}

	//------------------------------------------------------------------------------------------------
	float GetValue()
	{
		return m_Value;
	}
}

/**
Holds all the modes that the user set. Primary function is to enable saving the modes into a single config prefab
**/
[BaseContainerProps(configRoot: true)]
class LF_SirenModes
{
	[Attribute()]
	protected ref array<ref LF_SirenMode> m_Modes;

	//------------------------------------------------------------------------------------------------
	array<ref LF_SirenMode> GetModes()
	{
		return m_Modes;
	}

	//------------------------------------------------------------------------------------------------
	/**
	\brief Insert the light into each mode. Called when a light registers itself
	\param light - the light to be inserted
	**/
	void InsertLight(LF_SirenLightComponent light)
	{
		foreach (LF_SirenMode mode : m_Modes)
		{
			mode.InsertLight(light);
		}
	}

	//------------------------------------------------------------------------------------------------
	/**
	\brief Gets a mode by index
	\param index - index of the mode
	\return the mode with specified index, null if index is invalid
	**/
	LF_SirenMode GetMode(int index)
	{
		if (index < 0 || index > m_Modes.Count() - 1) return null;
		return m_Modes[index];
	}

	//------------------------------------------------------------------------------------------------
	/**
	\brief Finds a mode by name
	\param name - name of the mode being searched
	\return the index of the found mode or -1 if not found
	**/
	int Find(string name)
	{
		int i = 0;
		while (i < m_Modes.Count() && m_Modes[i].GetName() != name)
		{
			i++;
		}
		if (i < m_Modes.Count()) return i;
		return -1;
	}
}
//------------------------------------------------------------------------------------------------
enum LF_ESirenRequest
{
	ACCEPT,
	INVALID,
	SAME,
	TOO_FAST
}

//------------------------------------------------------------------------------------------------
//! Pure rule for a siren toggle request, shared by the server handler and LOGIC tests.
class LF_SirenRules
{
	//! Shortest time between two accepted changes on one vehicle.
	static const int MIN_TOGGLE_MS = 300;

	//------------------------------------------------------------------------------------------------
	//! Duty gate: the occupant must have an account, be on duty, and belong to the vehicle's service.
	//! A civilian service (no service set) admits nobody.
	static bool DutyAllows(LF_Faction service, bool hasAccount, LF_Faction faction, bool onDuty)
	{
		return hasAccount && onDuty && service != LF_Faction.CIVILIAN && faction == service;
	}

	//------------------------------------------------------------------------------------------------
	static LF_ESirenRequest Judge(int mode, int modeCount, int current, int nowMs, int lastChangeMs, bool hasChanged)
	{
		if (mode < 0 || mode >= modeCount)
			return LF_ESirenRequest.INVALID;

		if (mode == current)
			return LF_ESirenRequest.SAME;

		if (hasChanged && nowMs - lastChangeMs < MIN_TOGGLE_MS)
			return LF_ESirenRequest.TOO_FAST;

		return LF_ESirenRequest.ACCEPT;
	}
}
