//------------------------------------------------------------------------------------------------
//! Harness `sound [SOUND_EVENT]`: what an agent can know about sound without ears. For the weapon
//! in the local character's hands it asks the weapon's sound component for each event the pistol
//! animation and muzzle fire: is the event in the weapon's sound projects (known). The event named
//! in the command is also played here, and `played` says whether the engine started a voice for it.
//! SOUND_RELOAD_BOLT_PULL is in the list as a control: the vanilla M9's handling project has it, a
//! weapon with its own slide-less project does not.
//! It also reports the stun buzz (LF_StunBuzz) on this client.
//------------------------------------------------------------------------------------------------
class LF_HarnessSound
{
	static const ref array<string> WEAPON_EVENTS = {"SOUND_SHOT", "SOUND_DRY", "SOUND_RELOAD_MAG_REMOVE", "SOUND_RELOAD_MAG_INSERT", "SOUND_RELOAD_BOLT_PULL"};

	//! The voice the last probe started, for the follow-up line a moment later.
	protected static SoundComponent s_Sound;
	protected static AudioHandle s_Handle = AudioHandle.Invalid;
	protected static string s_sEvent;
	static const string DIRECT = "DIRECT_";

	//------------------------------------------------------------------------------------------------
	static string Probe(string play = "")
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
			return "no_character";

		string detail = "weapon=none";
		IEntity weapon = HeldWeapon(character);
		if (weapon)
		{
			detail = "weapon=" + FilePath.StripPath(weapon.GetPrefabData().GetPrefabName());
			SoundComponent sound = SoundComponent.Cast(weapon.FindComponent(SoundComponent));
			if (!sound)
			{
				detail += " soundcomponent=0";
			}
			else
			{
				// DIRECT_<event>: the same event from the weapon's own project list, played through
				// AudioSystem at the weapon and not through its sound component. A difference between
				// the two is the component's signals, not the project.
				if (play.StartsWith(DIRECT))
				{
					string directEvent = play.Substring(DIRECT.Length(), play.Length() - DIRECT.Length());
					s_Handle = PlayDirect(weapon, directEvent);
					detail += string.Format(" direct:%1=%2", directEvent, s_Handle != AudioHandle.Invalid);
					if (s_Handle != AudioHandle.Invalid && GetGame().GetCallqueue())
					{
						s_Sound = sound;
						s_sEvent = play;
						GetGame().GetCallqueue().CallLater(Follow, 200, false);
					}

					play = "";
				}

				array<string> events = {};
				events.Copy(WEAPON_EVENTS);
				if (!play.IsEmpty() && !events.Contains(play))
					events.Insert(play);

				foreach (string eventName : events)
				{
					bool known = sound.GetEventIndex(eventName) >= 0;
					bool played;
					if (known && eventName == play)
					{
						s_Handle = sound.SoundEvent(eventName);
						played = s_Handle != AudioHandle.Invalid;
						if (played && GetGame().GetCallqueue())
						{
							s_Sound = sound;
							s_sEvent = eventName;
							GetGame().GetCallqueue().CallLater(Follow, 200, false);
						}
					}

					detail += string.Format(" %1=known:%2,played:%3", eventName, known, played);
				}
			}
		}

		bool audible = AudioSystem.IsAudible(LF_StunBuzz.PROJECT, LF_StunBuzz.EVENT, character.GetOrigin()) >= 0;
		detail += string.Format(" stun_loop_audible=%1 stun_buzz_playing=%2 started=%3 stopped=%4", audible, LF_StunBuzz.GetPlayingCount(), LF_StunBuzz.GetStartedCount(), LF_StunBuzz.GetStoppedCount());
		LF_Debug.Info("AgentHarness", "sound probe: " + detail);
		return detail;
	}

	//------------------------------------------------------------------------------------------------
	//! 200 ms into the probed voice: is it still sounding. A voice the engine started and then
	//! culled shows here and not in `played`.
	protected static void Follow()
	{
		if (!s_Sound)
			return;

		LF_Debug.Info("AgentHarness", string.Format("sound probe: %1 after 200 ms: sounding=%2", s_sEvent, AudioSystem.IsSoundPlayed(s_Handle)));
		s_Sound = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Play an event from the first of the weapon prefab's sound projects that has it.
	protected static AudioHandle PlayDirect(notnull IEntity weapon, string eventName)
	{
		Resource res = Resource.Load(weapon.GetPrefabData().GetPrefabName());
		if (!res || !res.IsValid())
			return AudioHandle.Invalid;

		IEntitySource source = res.GetResource().ToEntitySource();
		array<ResourceName> files = {};
		for (int i = 0, count = source.GetComponentCount(); i < count; i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (component.GetClassName() == "WeaponSoundComponent")
				component.Get("Filenames", files);
		}

		vector mat[4];
		weapon.GetWorldTransform(mat);
		foreach (ResourceName file : files)
		{
			AudioHandle handle = AudioSystem.PlayEvent(file, eventName, mat);
			if (handle != AudioHandle.Invalid)
				return handle;
		}

		return AudioHandle.Invalid;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity HeldWeapon(notnull IEntity character)
	{
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!weapons)
			return null;

		BaseWeaponComponent current = weapons.GetCurrentWeapon();
		if (!current)
			return null;

		return current.GetOwner();
	}
}
