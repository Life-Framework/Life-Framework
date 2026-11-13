//------------------------------------------------------------------------------------------------
//! A looped background sound at a place: a radio playing, a furnace roaring, hens on a farm. Put
//! it on the prop that makes the sound, or on an empty LF_AmbientEmitter prefab for the sound of an
//! area. Everything here is local to a machine that renders: LF_Atmosphere decides twice a second
//! which emitters are near enough, allowed (condition, hours) and within the voice cap, and calls
//! Play and Stop. A dedicated server does nothing with it.
//!
//! The distance curve is in the sound project (tools\cli asset ambience writes inner_m and
//! outer_m into it); m_fRadius is where this machine starts the voice and should sit just inside
//! the project's outer range. The voice goes through SCR_SoundManagerModule, which measures the
//! interior at the source, so a source inside a building is muffled for a listener outside.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Atmosphere", description: "Looped positional background sound with a radius, hours, a condition and a share of the voice cap.")]
class LF_AmbientEmitterComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_AmbientEmitterComponent : ScriptComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Sound project with the looped event", "acp")]
	protected ResourceName m_sSoundProject;

	[Attribute("SOUND_LF_AMBIENT_LP", UIWidgets.EditBox, "Looped sound event in the project")]
	protected string m_sSoundEvent;

	[Attribute("20", UIWidgets.Auto, "Metres from the listener inside which the sound starts. Keep it just inside the project's outer range")]
	protected float m_fRadius;

	[Attribute("3", UIWidgets.Auto, "Metres past the radius a playing sound keeps playing, so the edge does not flutter")]
	protected float m_fStopMargin;

	[Attribute("0", UIWidgets.Auto, "Higher wins a voice when more emitters are in range than LF_Atmosphere's cap")]
	protected int m_iPriority;

	[Attribute("", UIWidgets.EditBox, "Emitters with the same group share one voice: the nearest plays. Empty is no group")]
	protected string m_sGroup;

	[Attribute("0", UIWidgets.Auto, "Sounds from this hour of the day (0 to 24). From equal to To is always")]
	protected float m_fHourFrom;

	[Attribute("0", UIWidgets.Auto, "Sounds until this hour of the day. A window may wrap past midnight")]
	protected float m_fHourTo;

	[Attribute("0 0 0", UIWidgets.Coords, "Where the sound comes from, relative to the entity", params: "inf inf purpose=coords space=entity")]
	protected vector m_vOffset;

	[Attribute("", UIWidgets.Object, "When it may sound. None is always")]
	protected ref LF_AmbientCondition m_Condition;

	//! Emitters alive on this machine (none on a dedicated server). LF_Atmosphere reads it each tick.
	protected static ref array<LF_AmbientEmitterComponent> s_aAll = {};

	protected ref LF_AmbientCandidate m_Candidate = new LF_AmbientCandidate();
	protected ref SCR_AudioSource m_Audio;
	protected bool m_bRegistered;
	protected bool m_bBroken;
	protected bool m_bVoiceSeen;
	protected int m_iPlayTicks;
	protected int m_iRefusedTicks;
	protected bool m_bRefusalSaid;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// A dedicated server hears nothing.
		if (RplSession.Mode() == RplMode.Dedicated || System.IsConsoleApp())
			return;

		if (m_sSoundProject.IsEmpty() || m_sSoundEvent.IsEmpty())
		{
			LF_Debug.Error(LF_Atmosphere.FEATURE, string.Format("%1 has no sound project or event; it stays silent", Label()));
			return;
		}

		if (m_Condition)
			m_Condition.Init(owner);

		m_Candidate.m_fRadius = m_fRadius;
		m_Candidate.m_fMargin = m_fStopMargin;
		m_Candidate.m_iPriority = m_iPriority;
		m_Candidate.m_sGroup = m_sGroup;
		s_aAll.Insert(this);
		m_bRegistered = true;
		LF_Atmosphere.OnEmittersChanged(s_aAll.Count());
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_bRegistered)
		{
			Stop("deleted");
			s_aAll.RemoveItem(this);
			LF_Atmosphere.OnEmittersChanged(s_aAll.Count());
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! The entity's name; for an unnamed entity (a building in a layer) the sound it carries.
	string Label()
	{
		IEntity owner = GetOwner();
		if (owner && !owner.GetName().IsEmpty())
			return owner.GetName();

		string project = FilePath.StripPath(m_sSoundProject);
		if (project.IsEmpty())
			return LF_Atmosphere.LabelOf(owner);

		return FilePath.StripExtension(project);
	}

	//------------------------------------------------------------------------------------------------
	static array<LF_AmbientEmitterComponent> GetAll()
	{
		return s_aAll;
	}

	//------------------------------------------------------------------------------------------------
	vector GetSoundPosition()
	{
		IEntity owner = GetOwner();
		if (m_vOffset == vector.Zero)
			return owner.GetOrigin();

		return owner.CoordToParent(m_vOffset);
	}

	//------------------------------------------------------------------------------------------------
	bool IsPlaying()
	{
		return m_Audio != null;
	}

	//------------------------------------------------------------------------------------------------
	float GetRadius()
	{
		return m_fRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! The emitter's row for this tick. LF_Atmosphere passes the listener distance and the hour.
	LF_AmbientCandidate Refresh(int id, float distance, float hour)
	{
		IEntity owner = GetOwner();
		bool allowed = !m_bBroken && LF_AtmosphereMix.InHours(hour, m_fHourFrom, m_fHourTo);
		if (allowed && m_Condition)
			allowed = m_Condition.IsMet(owner);

		m_Candidate.m_iId = id;
		m_Candidate.m_fDistance = distance;
		m_Candidate.m_bAllowed = allowed;
		m_Candidate.m_bPlaying = m_Audio != null;
		return m_Candidate;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the voice. False when the engine gave none this tick (the listener is past the
	//! project's audible range, or the sound system is not up yet); the mix asks again next tick.
	bool Play(float distance)
	{
		if (m_Audio)
			return true;

		IEntity owner = GetOwner();
		SCR_SoundManagerModule sound = SCR_SoundManagerModule.GetInstance(owner.GetWorld());
		if (!sound)
		{
			Refused(distance, "this machine has no sound manager");
			return false;
		}

		SCR_AudioSourceConfiguration config = new SCR_AudioSourceConfiguration();
		config.m_sSoundProject = m_sSoundProject;
		config.m_sSoundEventName = m_sSoundEvent;
		config.m_vOffset = m_vOffset;
		config.m_eFlags = EAudioSourceConfigurationFlag.Static;

		SCR_AudioSource audio = sound.CreateAudioSource(owner, config);
		if (!audio)
		{
			Refused(distance, "the engine calls the event inaudible here");
			return false;
		}

		sound.PlayAudioSource(audio);
		m_Audio = audio;
		m_iPlayTicks = 0;
		m_iRefusedTicks = 0;
		m_bVoiceSeen = false;
		LF_Atmosphere.CountStart();
		LF_Debug.Info(LF_Atmosphere.FEATURE, string.Format("start %1 at %2 m (radius %3) %4", Label(), distance.ToString(-1, 1), m_fRadius, FilePath.StripPath(m_sSoundProject)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The mix chose this emitter and the engine gave no voice. One tick of that is normal (the
	//! listener is not placed yet); five seconds of it is said once, so a sound that never plays is
	//! not a silent no-op: sound is off on this machine (-noSound), or m_fRadius reaches past the
	//! project's outer range.
	protected void Refused(float distance, string why)
	{
		m_iRefusedTicks++;
		if (m_bRefusalSaid || m_iRefusedTicks * LF_Atmosphere.TICK_MS < 5000)
			return;

		m_bRefusalSaid = true;
		LF_Debug.Warn(LF_Atmosphere.FEATURE, string.Format("%1 is in range at %2 m (radius %3) and got no voice for 5 s: %4", Label(), distance.ToString(-1, 1), m_fRadius, why));
	}

	//------------------------------------------------------------------------------------------------
	void Stop(string why)
	{
		if (!m_Audio)
			return;

		m_Audio.Terminate();
		m_Audio = null;
		LF_Atmosphere.CountStop();
		LF_Debug.Info(LF_Atmosphere.FEATURE, string.Format("stop %1 (%2)", Label(), why));
	}

	//------------------------------------------------------------------------------------------------
	//! Called each tick while playing. The voice starts a moment after Play (the interior is
	//! measured first), so this is where the engine's own answer is read: a valid handle that has
	//! not finished is a loop the engine is mixing. No voice after four seconds is a broken project
	//! or event; the emitter says so once and stays silent.
	void CheckVoice()
	{
		if (!m_Audio || m_bVoiceSeen)
			return;

		m_iPlayTicks++;
		bool valid = m_Audio.m_AudioHandle != AudioHandle.Invalid;
		if (valid && !AudioSystem.IsSoundPlayed(m_Audio.m_AudioHandle))
		{
			m_bVoiceSeen = true;
			LF_Debug.Info(LF_Atmosphere.FEATURE, string.Format("voice up %1 (engine handle valid, looping)", Label()));
			return;
		}

		if (m_iPlayTicks * LF_Atmosphere.TICK_MS < 4000)
			return;

		LF_Debug.Error(LF_Atmosphere.FEATURE, string.Format("%1: the engine started no voice for %2 %3 (handle valid=%4); it stays silent", Label(), m_sSoundProject, m_sSoundEvent, valid));
		m_bBroken = true;
		Stop("no voice");
	}
}
