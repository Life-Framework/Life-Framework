//------------------------------------------------------------------------------------------------
//! The alarm at a crime site (M1-20). LF_Dispatch spawns a replicated LF_SiteAlarm prefab at the
//! site and deletes it when the alarm time runs out; replication streaming hands the entity to
//! players near the site only. Every machine with a renderer plays the looped siren from the
//! entity and, when the prefab says so, flashes a light beside it. The level is the prefab: the
//! loud variant (nobody on duty) uses the vanilla loud siren project, a longer hear range, and a
//! brighter light. Deleting the entity stops the sound (SCR_SoundManagerModule drops a source
//! whose owner is gone) and the light (a child of the entity).
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Police", description: "Looped siren and flashing light at a crime site.")]
class LF_SiteAlarmComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_SiteAlarmComponent : ScriptComponent
{
	static const string FEATURE = "Dispatch";

	[Attribute("0", UIWidgets.CheckBox, "The loud level: nobody was on duty when the crime started")]
	protected bool m_bLoud;

	[Attribute("{852528B9C262A607}Sounds/Structures/Military/Sirens/Structures_Siren.acp", UIWidgets.ResourceNamePicker, "Sound project of the looped siren", "acp")]
	protected ResourceName m_sSoundProject;

	[Attribute("SOUND_SIREN_LP", UIWidgets.EditBox, "Looped sound event in the sound project")]
	protected string m_sSoundEvent;

	[Attribute("Siren", UIWidgets.EditBox, "Signal the siren project reads; set to Signal Value before play. Empty sets none")]
	protected string m_sSignalName;

	[Attribute("1", UIWidgets.Auto, "Value of the signal above")]
	protected float m_fSignalValue;

	[Attribute("150", UIWidgets.Auto, "Metres within which a player counts as hearing this alarm (harness state, logs)")]
	protected float m_fHearRange;

	[Attribute("1", UIWidgets.CheckBox, "Flash a light at the site")]
	protected bool m_bFlash;

	[Attribute("1 0.25 0.1", UIWidgets.ColorPicker, "Flash light colour")]
	protected vector m_vLightColor;

	[Attribute("12", UIWidgets.Auto, "Flash light radius in metres")]
	protected float m_fLightRadius;

	[Attribute("8", UIWidgets.Auto, "Flash light intensity (LV) when lit")]
	protected float m_fLightLV;

	[Attribute("0 2.2 0", UIWidgets.Coords, "Flash light offset from the site")]
	protected vector m_vLightOffset;

	[Attribute("500", UIWidgets.Auto, "Flash half-period in milliseconds: lit this long, dark this long")]
	protected int m_iFlashMs;

	//! Alarms alive on this machine, for the harness state and the near-site lookup.
	protected static ref array<LF_SiteAlarmComponent> s_aActive;

	protected ref SCR_AudioSource m_Audio;
	protected LightEntity m_Light;
	protected bool m_bLit;
	protected bool m_bRendering;
	protected bool m_bSoundBroken;

	//------------------------------------------------------------------------------------------------
	bool IsLoud()
	{
		return m_bLoud;
	}

	//------------------------------------------------------------------------------------------------
	float GetHearRange()
	{
		return m_fHearRange;
	}

	//------------------------------------------------------------------------------------------------
	//! True once this machine started the siren (a renderer that was in audible range).
	bool IsSounding()
	{
		return m_Audio != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The loudest alarm alive on this machine whose hear range covers position: "loud",
	//! "normal", or "none". What the harness reports as heard.
	static string HeardAt(vector position)
	{
		if (!s_aActive)
			return "none";

		string heard = "none";
		foreach (LF_SiteAlarmComponent alarm : s_aActive)
		{
			if (!alarm || !alarm.GetOwner())
				continue;

			if (vector.Distance(alarm.GetOwner().GetOrigin(), position) > alarm.GetHearRange())
				continue;

			if (alarm.IsLoud())
				return "loud";

			heard = "normal";
		}

		return heard;
	}

	//------------------------------------------------------------------------------------------------
	static int ActiveCount()
	{
		if (!s_aActive)
			return 0;

		return s_aActive.Count();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!s_aActive)
			s_aActive = {};

		s_aActive.Insert(this);

		// A dedicated server hears and renders nothing; it only owns the lifetime.
		if (RplSession.Mode() == RplMode.Dedicated || System.IsConsoleApp())
			return;

		m_bRendering = true;
		if (m_bFlash)
			CreateLight(owner);

		// The first try can come before the listener is placed; Tick retries the sound while the
		// alarm lives, so a player who walks into range later still hears it.
		GetGame().GetCallqueue().CallLater(Tick, m_iFlashMs, true);
		LF_Debug.Info(FEATURE, string.Format("site alarm up at %1: loud=%2 flash=%3", owner.GetOrigin(), m_bLoud, m_bFlash));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aActive)
			s_aActive.RemoveItem(this);

		if (m_bRendering)
		{
			GetGame().GetCallqueue().Remove(Tick);
			if (m_Audio)
			{
				m_Audio.Terminate();
				m_Audio = null;
			}

			if (m_Light)
				delete m_Light;

			LF_Debug.Info(FEATURE, string.Format("site alarm down at %1: loud=%2", owner.GetOrigin(), m_bLoud));
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (!m_Audio)
			StartSound(owner);

		if (!m_Light)
			return;

		m_bLit = !m_bLit;
		float lv = 0;
		if (m_bLit)
			lv = m_fLightLV;

		m_Light.SetColor(Color.FromVector(m_vLightColor), lv);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartSound(IEntity owner)
	{
		if (m_bSoundBroken)
			return;

		if (m_sSoundProject.IsEmpty() || m_sSoundEvent.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "site alarm has no sound project or event; the alarm is silent");
			m_bSoundBroken = true;
			return;
		}

		SCR_SoundManagerModule sound = SCR_SoundManagerModule.GetInstance(owner.GetWorld());
		if (!sound)
			return;

		SCR_AudioSourceConfiguration config = new SCR_AudioSourceConfiguration();
		config.m_sSoundProject = m_sSoundProject;
		config.m_sSoundEventName = m_sSoundEvent;
		config.m_eFlags = EAudioSourceConfigurationFlag.Static;

		// Null while the listener is out of the project's audible range; Tick tries again.
		SCR_AudioSource audio = sound.CreateAudioSource(owner, config);
		if (!audio)
			return;

		if (!m_sSignalName.IsEmpty())
			audio.SetSignalValue(m_sSignalName, m_fSignalValue);

		sound.PlayAudioSource(audio);
		m_Audio = audio;
		LF_Debug.Info(FEATURE, string.Format("site alarm sounding: %1 %2 loud=%3", m_sSoundProject, m_sSoundEvent, m_bLoud));
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateLight(IEntity owner)
	{
		m_Light = LightEntity.CreateLight(LightType.POINT, LightFlags.CASTSHADOW, m_fLightRadius, Color.FromVector(m_vLightColor), m_fLightLV, m_vLightOffset);
		if (!m_Light)
		{
			LF_Debug.Error(FEATURE, "site alarm light was not created");
			return;
		}

		m_Light.SetLensFlareType(LightLensFlareType.Disabled);
		owner.AddChild(m_Light, -1);
		m_bLit = true;
	}
}
