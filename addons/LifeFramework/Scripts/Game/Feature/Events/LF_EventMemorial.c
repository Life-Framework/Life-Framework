//------------------------------------------------------------------------------------------------
//! A memorial rite (#3806): a pack .conf with this root class becomes an operation a phase names
//! (LF_EventModuleLoader). While the phase runs, at the scene's placed `event_target` marker:
//!  - the first run opens the rite once per phase run: the church bells ring (a one-shot RPC the
//!    clients play on the vanilla bell of the nearest church, LF_MemorialBells) and the town's radios
//!    within a ring of the marker go quiet for a while (the replicated LF_AmbientSwitchComponent
//!    of each radio whose sound slot the config names), then come back on; the speaker's recorded
//!    line (a voiced line of a dialogue conf, #3827) plays at the marker at the same moment the
//!    people present get the caption (LF_MemorialVoice), and the rite is busy for its length;
//!  - every run, each player standing within the radius for the first time in this phase run gets
//!    the event toast and the speaker's line, and one standing award (LF_Reputation.Award) once per
//!    character per phase run, so a late arrival is counted and a player who stays is not paid twice.
//! Data only: the marker id, the texts (localisation keys), the slots and the award row are the
//! pack's; the core knows no place, no name and no sound. Server only.
[BaseContainerProps()]
class LF_EventMemorial
{
	[Attribute("", UIWidgets.EditBox, "Scene id whose placed event_target marker is the memorial: the radius, the radio ring and the bells are measured from it")]
	string m_sSceneId;

	[Attribute("30", UIWidgets.Auto, "Metres around the marker a player counts as present: they get the toast, the line and the standing", params: "1 300")]
	float m_fRadius;

	[Attribute("", UIWidgets.EditBox, "Toast title shown to players present (a localisation key or plain text)")]
	string m_sToastTitle;

	[Attribute("", UIWidgets.EditBox, "Toast text shown to players present (a localisation key or plain text)")]
	string m_sToastText;

	[Attribute("8", UIWidgets.Auto, "Seconds the toast stays up", params: "1 60")]
	float m_fToastSeconds;

	[Attribute("", UIWidgets.EditBox, "Who speaks the line (a localisation key or plain text): the priest")]
	string m_sSpeakerName;

	[Attribute("", UIWidgets.EditBox, "What the speaker says to players present (a localisation key or plain text). Empty: no line")]
	string m_sSpeakerLine;

	[Attribute("12", UIWidgets.Auto, "Seconds the line stays up", params: "1 60")]
	float m_fLineSeconds;

	[Attribute("", UIWidgets.Auto, "Sound slots (Configs/SoundSlots) of the radios that go quiet. Empty: no radio is touched")]
	ref array<string> m_aRadioSlots;

	[Attribute("120", UIWidgets.Auto, "Metres around the marker inside which a radio goes quiet", params: "1 1000")]
	float m_fRadioRadius;

	[Attribute("60", UIWidgets.Auto, "Seconds the radios stay quiet; the ones that were playing come back after", params: "0 600")]
	float m_fSilenceSeconds;

	[Attribute("12", UIWidgets.Auto, "Strokes of the church bell; 0 rings nothing", params: "0 40")]
	int m_iBellStrokes;

	[Attribute("90", UIWidgets.Auto, "Metres around the marker the church bell is looked for (clients take the nearest vanilla bell inside it)", params: "1 500")]
	float m_fBellSearchRadius;

	[Attribute("", UIWidgets.EditBox, "Standing award row id (Configs/Reputation) given once per character per phase run. Empty: no award")]
	string m_sAwardAction;

	[Attribute("", UIWidgets.ResourceNamePicker, "Dialogue .conf (Configs/Dialogue) holding the speaker's recorded line; played at the memorial when the rite opens, with the caption. Empty: no voice", "conf")]
	ResourceName m_sVoiceDialogue;

	[Attribute("", UIWidgets.EditBox, "Id of the voiced line in that dialogue; its sound event, audio project and length are the clip")]
	string m_sVoiceLine;

	[Attribute("1.65", UIWidgets.Auto, "Metres above the marker the clip plays from (the speaker's head)", params: "0 10")]
	float m_fVoiceHeight;

	//------------------------------------------------------------------------------------------------
	//! \return an empty string when usable, else why not.
	string Validate()
	{
		if (m_sSceneId.IsEmpty())
			return "no scene id";

		if (m_sVoiceDialogue.IsEmpty() != m_sVoiceLine.IsEmpty())
			return "a voice needs both a dialogue conf and a line id";

		if (m_fRadius <= 0)
			return "radius must be above 0";

		if (m_aRadioSlots && !m_aRadioSlots.IsEmpty() && (m_fRadioRadius <= 0 || m_fSilenceSeconds <= 0))
			return "radio slots need a radio radius and a silence time above 0";

		if (m_iBellStrokes < 0)
			return "bell strokes below 0";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! One player standing at the memorial, as the operation reads it. Tests feed these in place of the
//! connected players.
class LF_EventMemorialPerson
{
	int m_iPlayerId;
	string m_sUid;
	vector m_vPosition;

	//------------------------------------------------------------------------------------------------
	static LF_EventMemorialPerson Make(int playerId, string uid, vector position)
	{
		LF_EventMemorialPerson person = new LF_EventMemorialPerson();
		person.m_iPlayerId = playerId;
		person.m_sUid = uid;
		person.m_vPosition = position;
		return person;
	}
}

//------------------------------------------------------------------------------------------------
//! What one run of the memorial operation did, plain data for the log and the tests.
enum LF_EMemorialOutcome
{
	NOT_SERVER,		//!< Not the server: nothing happens on a client.
	NO_SITE,		//!< The scene has no placed event_target: nowhere to hold the rite (logged).
	OPENED,			//!< First run of this phase run: bells and radios done, the people present served.
	SERVED,			//!< A later run of this phase run: only people who had not been served yet.
	QUIET			//!< A later run and nobody new stands there.
}

//------------------------------------------------------------------------------------------------
//! The operation an LF_EventMemorial config registers as. It is never Busy: everything happens
//! inside the one Run, the radios come back by the call queue. Server only.
class LF_EventMemorialOperation : LF_EventOperation
{
	static const string FEATURE = "Events";

	string m_sModuleId;
	ref LF_EventMemorial m_Config;
	//! Tests: the place to use instead of the scene's marker.
	bool m_bSiteOverride;
	vector m_vSiteOverride;
	//! Tests: these people stand there; null means the connected players.
	ref array<ref LF_EventMemorialPerson> m_aTestPeople;
	//! Tests: no RPC to clients, no toast (there is no player controller to send to).
	bool m_bQuiet;
	//! The last run's outcome, for the tests.
	LF_EMemorialOutcome m_eLast;
	//! How many people the last run served, how many standing awards moved points.
	int m_iLastServed;
	int m_iLastAwarded;
	//! Radios switched off by the last opening, not back on yet (tests read it).
	ref array<EntityID> m_aSilenced = {};
	//! Tests: the speaker's dialogue to use instead of loading m_Config.m_sVoiceDialogue.
	ref LF_DialogueConfig m_VoiceTest;
	//! The clip the last opening asked the clients to play (tests read it): event, project, seconds, where.
	string m_sLastClipEvent;
	string m_sLastClipProject;
	float m_fLastClipSeconds;
	vector m_vLastClipAt;
	//! How many clips the operation has started.
	int m_iClips;
	//! Tick count at which the clip started speaking ends; the rite is busy until then.
	protected int m_iClipEndsMs;

	protected bool m_bRan;
	protected float m_fRanPhaseAt;
	protected ref set<string> m_aServed = new set<string>();

	//! Query scratch: the radios a sphere scan found, and where it looked.
	protected ref array<IEntity> m_aMatches;
	protected vector m_vScanCenter;

	//------------------------------------------------------------------------------------------------
	void LF_EventMemorialOperation(string moduleId, LF_EventMemorial config)
	{
		m_sModuleId = moduleId;
		m_Config = config;
		m_eLast = LF_EMemorialOutcome.NOT_SERVER;
	}

	//------------------------------------------------------------------------------------------------
	void ~LF_EventMemorialOperation()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(RestoreRadios);
	}

	//------------------------------------------------------------------------------------------------
	//! The rite waits for the clip: the phase does not advance while the speaker is still talking.
	override bool Busy()
	{
		return m_iClipEndsMs != 0 && System.GetTickCount() < m_iClipEndsMs;
	}

	//------------------------------------------------------------------------------------------------
	override bool Run(string eventId, string phaseName, LF_EventContext ctx)
	{
		LF_EMemorialOutcome outcome = Execute(eventId, phaseName, ctx);
		m_eLast = outcome;
		return outcome == LF_EMemorialOutcome.OPENED || outcome == LF_EMemorialOutcome.SERVED;
	}

	//------------------------------------------------------------------------------------------------
	LF_EMemorialOutcome Execute(string eventId, string phaseName, LF_EventContext ctx)
	{
		m_iLastServed = 0;
		m_iLastAwarded = 0;
		if (!Replication.IsServer() || !m_Config)
			return LF_EMemorialOutcome.NOT_SERVER;

		vector site;
		if (!ResolveSite(site))
		{
			LF_Debug.Warn(FEATURE, string.Format("event %1 phase %2: module %3 has no placed event_target for scene '%4'; no rite", eventId, phaseName, m_sModuleId, m_Config.m_sSceneId));
			return LF_EMemorialOutcome.NO_SITE;
		}

		// A new phase run (its start time moved) starts a new rite: everybody counts again.
		bool opening = true;
		if (ctx && m_bRan && ctx.m_fPhaseStartedAt == m_fRanPhaseAt)
			opening = false;

		if (opening)
		{
			m_bRan = ctx != null;
			if (ctx)
				m_fRanPhaseAt = ctx.m_fPhaseStartedAt;

			m_aServed.Clear();
			Open(eventId, phaseName, site);
		}

		int served = ServePresent(eventId, phaseName, site);
		if (opening)
			return LF_EMemorialOutcome.OPENED;

		if (served > 0)
			return LF_EMemorialOutcome.SERVED;

		return LF_EMemorialOutcome.QUIET;
	}

	//------------------------------------------------------------------------------------------------
	//! The part of the rite that happens once per phase run: bells, then the radios.
	protected void Open(string eventId, string phaseName, vector site)
	{
		LF_Debug.Info(FEATURE, string.Format("event %1 phase %2: module %3 opens the rite at scene '%4'", eventId, phaseName, m_sModuleId, m_Config.m_sSceneId));
		if (m_Config.m_iBellStrokes > 0 && !m_bQuiet)
			LF_MemorialBells.Announce(site, m_Config.m_fBellSearchRadius, m_Config.m_iBellStrokes);

		SilenceRadios(site);

		// The clip starts with the caption: ServePresent runs right after in the same run.
		StartVoice(site);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks every client to play the speaker's recorded line from the memorial, and holds the rite for
	//! its length. No voice configured, or a line that does not resolve (logged): the caption is all.
	protected void StartVoice(vector site)
	{
		m_iClipEndsMs = 0;
		m_sLastClipEvent = "";
		m_sLastClipProject = "";
		m_fLastClipSeconds = 0;
		if (m_Config.m_sVoiceDialogue.IsEmpty() || m_Config.m_sVoiceLine.IsEmpty())
			return;

		LF_DialogueConfig dialogue = m_VoiceTest;
		if (!dialogue)
			dialogue = LF_DialogueConfig.Load(m_Config.m_sVoiceDialogue);

		if (!dialogue)
		{
			LF_Debug.Error(FEATURE, string.Format("memorial %1: voice dialogue %2 does not load; caption only", m_sModuleId, m_Config.m_sVoiceDialogue));
			return;
		}

		LF_DialogueLine line = LF_DialogueFlow.FindLine(dialogue, m_Config.m_sVoiceLine);
		if (!line || line.IsTextOnly() || line.m_sEvent.IsEmpty() || dialogue.m_sSoundProject.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("memorial %1: voice line '%2' of %3 has no recording; caption only", m_sModuleId, m_Config.m_sVoiceLine, dialogue.m_sId));
			return;
		}

		m_sLastClipEvent = line.m_sEvent;
		m_sLastClipProject = dialogue.m_sSoundProject;
		m_fLastClipSeconds = line.m_fSeconds;
		m_vLastClipAt = site + Vector(0, m_Config.m_fVoiceHeight, 0);
		m_iClips++;
		int clipMs = Math.Round(line.m_fSeconds * 1000);
		m_iClipEndsMs = System.GetTickCount() + clipMs;
		if (m_iClipEndsMs == 0)
			m_iClipEndsMs = 1;

		LF_Debug.Info(FEATURE, string.Format("memorial %1: voice %2 (%3 s) starts with the caption", m_sModuleId, line.m_sEvent, line.m_fSeconds));
		if (!m_bQuiet)
			LF_MemorialVoice.Announce(m_vLastClipAt, m_sLastClipProject, m_sLastClipEvent);
	}

	//------------------------------------------------------------------------------------------------
	//! Switches off every playing radio of the configured slots within the ring and schedules them back.
	//! A radio already off stays off afterwards: nothing is switched on that this did not switch off.
	protected void SilenceRadios(vector site)
	{
		// A second opening before the first one's radios came back puts them back first.
		RestoreRadios();
		if (!m_Config.m_aRadioSlots || m_Config.m_aRadioSlots.IsEmpty())
			return;

		if (!GetGame() || !GetGame().GetWorld())
		{
			LF_Debug.Error(FEATURE, "memorial radios skipped: no world");
			return;
		}

		m_aMatches = {};
		GetGame().GetWorld().QueryEntitiesBySphere(site, m_Config.m_fRadioRadius, CollectRadio, null, EQueryEntitiesFlags.ALL);
		foreach (IEntity radio : m_aMatches)
		{
			LF_AmbientSwitchComponent power = LF_AmbientSwitchComponent.Cast(radio.FindComponent(LF_AmbientSwitchComponent));
			if (power && power.IsOn() && power.SetOn(false))
				m_aSilenced.Insert(radio.GetID());
		}

		m_aMatches = null;
		LF_Debug.Info(FEATURE, string.Format("memorial: %1 radio(s) quiet for %2 s", m_aSilenced.Count(), m_Config.m_fSilenceSeconds));
		if (!m_aSilenced.IsEmpty())
			GetGame().GetCallqueue().CallLater(RestoreRadios, m_Config.m_fSilenceSeconds * 1000, false);
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectRadio(IEntity entity)
	{
		if (!entity)
			return true;

		LF_AmbientEmitterComponent emitter = LF_AmbientEmitterComponent.Cast(entity.FindComponent(LF_AmbientEmitterComponent));
		if (!emitter || !entity.FindComponent(LF_AmbientSwitchComponent))
			return true;

		if (m_Config.m_aRadioSlots.Find(emitter.GetSoundSlot()) != -1 && vector.Distance(entity.GetOrigin(), m_vScanCenter) <= m_Config.m_fRadioRadius)
			m_aMatches.Insert(entity);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the radios this operation switched off back on. Safe to call with none.
	void RestoreRadios()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(RestoreRadios);

		if (m_aSilenced.IsEmpty())
			return;

		int back;
		foreach (EntityID id : m_aSilenced)
		{
			IEntity radio;
			if (GetGame() && GetGame().GetWorld())
				radio = GetGame().GetWorld().FindEntityByID(id);

			if (!radio)
				continue;

			LF_AmbientSwitchComponent power = LF_AmbientSwitchComponent.Cast(radio.FindComponent(LF_AmbientSwitchComponent));
			if (power && power.SetOn(true))
				back++;
		}

		LF_Debug.Info(FEATURE, string.Format("memorial: %1 radio(s) back on", back));
		m_aSilenced.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Toast, line and standing for each person within the radius not served yet in this phase run.
	//! \return how many were served.
	protected int ServePresent(string eventId, string phaseName, vector site)
	{
		array<ref LF_EventMemorialPerson> people = {};
		ReadPeople(people);

		int served;
		foreach (LF_EventMemorialPerson person : people)
		{
			if (person.m_sUid.IsEmpty() || m_aServed.Contains(person.m_sUid))
				continue;

			if (vector.Distance(person.m_vPosition, site) > m_Config.m_fRadius)
				continue;

			m_aServed.Insert(person.m_sUid);
			served++;
			Tell(person);
			if (!m_Config.m_sAwardAction.IsEmpty() && LF_Reputation.Award(person.m_sUid, m_Config.m_sAwardAction) != 0)
				m_iLastAwarded++;
		}

		m_iLastServed = served;
		if (served > 0)
			LF_Debug.Info(FEATURE, string.Format("event %1 phase %2: module %3 served %4 at the memorial, %5 standing award(s)", eventId, phaseName, m_sModuleId, served, m_iLastAwarded));
		return served;
	}

	//------------------------------------------------------------------------------------------------
	protected void Tell(LF_EventMemorialPerson person)
	{
		if (m_bQuiet)
			return;

		if (!m_Config.m_sToastText.IsEmpty())
			LF_NotificationManagerComponent.NotifyPlayer(person.m_iPlayerId, m_Config.m_sToastTitle, m_Config.m_sToastText, m_Config.m_fToastSeconds);

		if (!m_Config.m_sSpeakerLine.IsEmpty())
			LF_NotificationManagerComponent.NotifyPlayer(person.m_iPlayerId, m_Config.m_sSpeakerName, m_Config.m_sSpeakerLine, m_Config.m_fLineSeconds);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReadPeople(notnull array<ref LF_EventMemorialPerson> into)
	{
		if (m_aTestPeople)
		{
			foreach (LF_EventMemorialPerson fixture : m_aTestPeople)
				into.Insert(fixture);
			return;
		}

		if (!GetGame())
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity body = playerManager.GetPlayerControlledEntity(playerId);
			if (!body)
				continue;

			into.Insert(LF_EventMemorialPerson.Make(playerId, LF_Utils.GetPlayerUID(playerId), body.GetOrigin()));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool ResolveSite(out vector site)
	{
		if (m_bSiteOverride)
		{
			site = m_vSiteOverride;
			m_vScanCenter = site;
			return true;
		}

		if (!LF_Places.FindOrigin(LF_EventWaypointComponent.KIND_TARGET, m_Config.m_sSceneId, site))
			return false;

		m_vScanCenter = site;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The church bells of the memorial on the clients (#3806). The server announces a position, a search
//! radius and a number of strokes; each machine that renders takes the nearest vanilla bell
//! (SCR_BellSoundComponent) inside the radius and rings it by hand, alternating the two vanilla strokes
//! and ending on the vanilla last stroke, one every 850 ms as the vanilla hour bell does. No bell in
//! reach: one warning, no sound. Nothing is decided here; a dedicated server never plays.
class LF_MemorialBells
{
	static const string FEATURE = "Events";
	static const int STROKE_MS = 850; // lf-fixed: the vanilla hour bell's own stroke interval (SCR_BellSoundComponent.TRIGGER_INTERVAL)

	protected static ref LF_MemorialBells s_Ringing;

	protected EntityID m_BellId = EntityID.INVALID;
	protected int m_iLeft;
	protected vector m_vCenter;
	protected float m_fRadius;
	protected ref array<IEntity> m_aFound;

	//------------------------------------------------------------------------------------------------
	//! Server: tell every client to ring, and ring here when this machine renders (a listen host).
	static void Announce(vector at, float searchRadius, int strokes)
	{
		if (!Replication.IsServer())
			return;

		LF_GameModeRoleplay mode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (!mode)
		{
			LF_Debug.Error(FEATURE, "memorial bells skipped: no roleplay game mode to announce on");
			return;
		}

		mode.LF_AnnounceMemorialBells(at, searchRadius, strokes);
	}

	//------------------------------------------------------------------------------------------------
	//! This machine: finds the nearest bell and starts ringing it.
	static void Ring(vector at, float searchRadius, int strokes)
	{
		if (RplSession.Mode() == RplMode.Dedicated || System.IsConsoleApp() || strokes <= 0)
			return;

		LF_MemorialBells ringer = new LF_MemorialBells();
		ringer.m_vCenter = at;
		ringer.m_fRadius = searchRadius;
		ringer.m_iLeft = strokes;
		IEntity bell = ringer.FindBell();
		if (!bell)
		{
			LF_Debug.Warn(FEATURE, string.Format("memorial bells: no church bell within %1 m of the memorial; silent", searchRadius));
			return;
		}

		ringer.m_BellId = bell.GetID();
		if (s_Ringing)
			GetGame().GetCallqueue().Remove(s_Ringing.Stroke);

		s_Ringing = ringer;
		GetGame().GetCallqueue().CallLater(s_Ringing.Stroke, STROKE_MS, true);
		LF_Debug.Info(FEATURE, string.Format("memorial bells: ringing %1 strokes", strokes));
	}

	//------------------------------------------------------------------------------------------------
	//! One stroke: the vanilla bell event for this count, the last one the vanilla end stroke.
	void Stroke()
	{
		IEntity bell;
		if (GetGame() && GetGame().GetWorld())
			bell = GetGame().GetWorld().FindEntityByID(m_BellId);

		SoundComponent sound;
		if (bell)
			sound = SoundComponent.Cast(bell.FindComponent(SoundComponent));

		if (sound)
		{
			string eventName = SCR_SoundEvent.SOUND_BELL_A;
			if (m_iLeft == 1)
				eventName = SCR_SoundEvent.SOUND_BELL_END;
			else if (m_iLeft % 2 == 1)
				eventName = SCR_SoundEvent.SOUND_BELL_B;

			sound.SoundEvent(eventName);
		}

		m_iLeft--;
		if (!sound || m_iLeft <= 0)
		{
			GetGame().GetCallqueue().Remove(Stroke);
			if (s_Ringing == this)
				s_Ringing = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest entity with a vanilla bell component inside the radius, or null.
	protected IEntity FindBell()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return null;

		m_aFound = {};
		GetGame().GetWorld().QueryEntitiesBySphere(m_vCenter, m_fRadius, CollectBell, null, EQueryEntitiesFlags.ALL);
		IEntity best;
		float bestDistance = m_fRadius + 1;
		foreach (IEntity candidate : m_aFound)
		{
			float distance = vector.Distance(candidate.GetOrigin(), m_vCenter);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = candidate;
			}
		}

		m_aFound = null;
		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectBell(IEntity entity)
	{
		if (entity && entity.FindComponent(SCR_BellSoundComponent))
			m_aFound.Insert(entity);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Ringing && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(s_Ringing.Stroke);

		s_Ringing = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MemorialBells' statics (LF_StaticReset finds it by reflection).
class LF_MemorialBells_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MemorialBells";
	}

	override void Apply()
	{
		LF_MemorialBells.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The speaker's recorded line of the memorial on the clients (#3827). The server announces a
//! position (the speaker's head at the memorial), an audio project and a sound event; each machine
//! that renders plays it there as a 3D sound, so a player near the church hears the priest and one
//! far away hears nothing (the project's own range). Nothing is decided here; a dedicated server
//! never plays.
class LF_MemorialVoice
{
	static const string FEATURE = "Events";
	protected static int s_iPlayed;

	//------------------------------------------------------------------------------------------------
	//! Server: tell every client to play the clip, and play it here when this machine renders (a listen host).
	static void Announce(vector at, string project, string soundEvent)
	{
		if (!Replication.IsServer())
			return;

		LF_GameModeRoleplay mode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (!mode)
		{
			LF_Debug.Error(FEATURE, "memorial voice skipped: no roleplay game mode to announce on");
			return;
		}

		mode.LF_AnnounceMemorialVoice(at, project, soundEvent);
	}

	//------------------------------------------------------------------------------------------------
	//! This machine: plays the clip at `at`.
	static void Play(vector at, string project, string soundEvent)
	{
		if (RplSession.Mode() == RplMode.Dedicated || System.IsConsoleApp() || project.IsEmpty() || soundEvent.IsEmpty())
			return;

		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = at;
		AudioHandle handle = AudioSystem.PlayEvent(project, soundEvent, mat);
		if (handle == AudioHandle.Invalid)
		{
			LF_Debug.Warn(FEATURE, string.Format("memorial voice %1 did not start (out of earshot here, or %2 failed to load)", soundEvent, project));
			return;
		}

		s_iPlayed++;
		LF_Debug.Info(FEATURE, string.Format("memorial voice %1 plays at the memorial", soundEvent));
	}

	//------------------------------------------------------------------------------------------------
	//! Clips started on this machine (the harness and a session proof read it).
	static int PlayedCount()
	{
		return s_iPlayed;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iPlayed = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MemorialVoice's statics (LF_StaticReset finds it by reflection).
class LF_MemorialVoice_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MemorialVoice";
	}

	override void Apply()
	{
		LF_MemorialVoice.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Carries the memorial bells from the server to the clients on the game mode, like the event
//! announcement (LF_EventShell.c), without editing LF_GameModeRoleplay.c (M4 rule).
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	//! Server: every client rings the church bell near `at`. A listen-server host rings it directly.
	void LF_AnnounceMemorialBells(vector at, float searchRadius, int strokes)
	{
		if (!Replication.IsServer())
			return;

		Rpc(RpcDo_LF_MemorialBells, at, searchRadius, strokes);
		if (RplSession.Mode() != RplMode.Dedicated)
			RpcDo_LF_MemorialBells(at, searchRadius, strokes);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_MemorialBells(vector at, float searchRadius, int strokes)
	{
		LF_MemorialBells.Ring(at, searchRadius, strokes);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every client plays the speaker's clip at `at`. A listen-server host plays it directly.
	void LF_AnnounceMemorialVoice(vector at, string project, string soundEvent)
	{
		if (!Replication.IsServer())
			return;

		Rpc(RpcDo_LF_MemorialVoice, at, project, soundEvent);
		if (RplSession.Mode() != RplMode.Dedicated)
			RpcDo_LF_MemorialVoice(at, project, soundEvent);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_MemorialVoice(vector at, string project, string soundEvent)
	{
		LF_MemorialVoice.Play(at, project, soundEvent);
	}
}
