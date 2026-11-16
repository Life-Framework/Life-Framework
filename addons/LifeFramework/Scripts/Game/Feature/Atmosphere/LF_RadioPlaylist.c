//------------------------------------------------------------------------------------------------
//! Radio playlists (#1969): a radio rotates through a list of tracks instead of looping one. The
//! core keeps only the mechanism; the lists are pack data (an LF_RadioPlaylistConfig overlay the
//! mission header names, a pack's Configs/Atmosphere/). A list is keyed by the sound
//! slot of the emitter it plays on (LF_AmbientEmitterComponent.m_sSoundSlot), so a prop is not
//! touched: a slot with a list rotates, a slot without one plays its single project as before.
//!
//! One track is one looped sound project (SOUND_LF_AMBIENT_LP) that plays for m_fSeconds, then the
//! next playable track starts. A track whose project does not load is skipped with one
//! LF_Debug.Error; a list with no playable track leaves the radio silent. A list may carry hours
//! (local solar time, a window may wrap past midnight): outside them the radio is silent.
[BaseContainerProps()]
class LF_RadioTrack
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Sound project with the looped event SOUND_LF_AMBIENT_LP", "acp")]
	ResourceName m_Project;

	[Attribute("60", UIWidgets.Auto, "Seconds the track plays before the next one starts: about the length of its sample")]
	float m_fSeconds;

	//! 0 not checked yet, 1 playable, 2 missing. Transient, never saved.
	protected int m_iState;
	protected bool m_bMissingSaid;

	//------------------------------------------------------------------------------------------------
	//! Tests and tools set the state without loading anything.
	void SetPlayable(bool playable)
	{
		if (playable)
			m_iState = 1;
		else
			m_iState = 2;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPlayable()
	{
		if (m_iState == 0)
		{
			m_iState = 2;
			if (!m_Project.IsEmpty())
			{
				Resource resource = Resource.Load(m_Project);
				if (resource && resource.IsValid())
					m_iState = 1;
			}
		}

		return m_iState == 1;
	}

	//------------------------------------------------------------------------------------------------
	//! True the first time it is asked after the track was found missing, so the error logs once.
	bool TakeMissingNote()
	{
		if (m_bMissingSaid)
			return false;

		m_bMissingSaid = true;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_RadioPlaylist
{
	[Attribute("", UIWidgets.EditBox, "The sound slot of the radio this list plays on (atmo_store_radio)")]
	string m_sSlot;

	[Attribute("0", UIWidgets.Auto, "The list plays from this hour of the day (0 to 24). From equal to To is always")]
	float m_fHourFrom;

	[Attribute("0", UIWidgets.Auto, "The list plays until this hour. A window may wrap past midnight")]
	float m_fHourTo;

	[Attribute("", UIWidgets.Object, "The tracks, in the order they play; after the last the first plays again")]
	ref array<ref LF_RadioTrack> m_aTracks;

	//------------------------------------------------------------------------------------------------
	bool InHours(float hour)
	{
		return LF_AtmosphereMix.InHours(hour, m_fHourFrom, m_fHourTo);
	}

	//------------------------------------------------------------------------------------------------
	int PlayableCount()
	{
		int count;
		if (!m_aTracks)
			return 0;

		foreach (LF_RadioTrack track : m_aTracks)
		{
			if (track && track.IsPlayable())
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! The index of the track that plays after `after` (-1: the first start): the next playable one
	//! in order, wrapping past the end. A missing track is skipped with one error. -1 when no track
	//! plays; with a single playable track that track follows itself.
	int Next(int after)
	{
		if (!m_aTracks || m_aTracks.IsEmpty())
			return -1;

		int count = m_aTracks.Count();
		for (int step = 1; step <= count; step++)
		{
			int index = (after + step) % count;
			if (index < 0)
				index += count;

			LF_RadioTrack track = m_aTracks[index];
			if (track && track.IsPlayable())
				return index;

			if (track && track.TakeMissingNote())
				LF_Debug.Error(LF_Atmosphere.FEATURE, string.Format("radio playlist '%1': track %2 (%3) does not load; skipped", m_sSlot, index, track.m_Project));
		}

		return -1;
	}
}

//------------------------------------------------------------------------------------------------
//! The root of a pack's playlist overlay file.
[BaseContainerProps()]
class LF_RadioPlaylistConfig
{
	[Attribute("", UIWidgets.Object, "Playlists, one per radio sound slot")]
	ref array<ref LF_RadioPlaylist> m_aPlaylists;
}

//------------------------------------------------------------------------------------------------
//! What an emitter asks: LF_RadioPlaylists.Find("atmo_store_radio"). Every pack overlay of class
//! LF_RadioPlaylistConfig is read once, on first use; with none, every Find is null and the radios
//! play their one project.
class LF_RadioPlaylists
{
	protected static ref map<string, ref LF_RadioPlaylist> s_mBySlot;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mBySlot = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers a list (tests; a later list for the same slot replaces the earlier one).
	static void Add(notnull LF_RadioPlaylist list)
	{
		if (!s_mBySlot)
			s_mBySlot = new map<string, ref LF_RadioPlaylist>();

		s_mBySlot.Set(list.m_sSlot, list);
	}

	//------------------------------------------------------------------------------------------------
	static LF_RadioPlaylist Find(string slot)
	{
		if (slot.IsEmpty())
			return null;

		if (!s_mBySlot)
			Load();

		LF_RadioPlaylist list;
		if (s_mBySlot.Find(slot, list))
			return list;

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Load()
	{
		s_mBySlot = new map<string, ref LF_RadioPlaylist>();
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(ResourceName.Empty, LF_RadioPlaylistConfig, "radio playlists", resources, names);
		foreach (int i, Resource resource : resources)
		{
			LF_RadioPlaylistConfig config = LF_RadioPlaylistConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
			if (!config || !config.m_aPlaylists)
				continue;

			foreach (LF_RadioPlaylist list : config.m_aPlaylists)
			{
				if (!list || list.m_sSlot.IsEmpty())
				{
					LF_Debug.Error(LF_Atmosphere.FEATURE, string.Format("radio playlist without a slot in %1; skipped", names[i]));
					continue;
				}

				s_mBySlot.Set(list.m_sSlot, list);
				LF_Debug.Info(LF_Atmosphere.FEATURE, string.Format("radio playlist '%1': %2 tracks (%3)", list.m_sSlot, list.m_aTracks.Count(), names[i]));
			}
		}
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_RadioPlaylists' statics (registered at script load; Core/LF_StaticReset.c).
class LF_RadioPlaylists_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_RadioPlaylists_StaticResetHook());

	override string Owner()
	{
		return "LF_RadioPlaylists";
	}

	override void Apply()
	{
		LF_RadioPlaylists.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The emitter side: an emitter whose sound slot has a playlist plays the list's current track,
//! and moves to the next when the track's time is up or the voice stops for any reason (a radio
//! out of earshot carries on). Out of the list's hours it is not allowed, so it stays silent.
modded class LF_AmbientEmitterComponent
{
	protected ref LF_RadioPlaylist m_Playlist;
	protected int m_iTrack = -1;
	protected int m_iTrackTicks;
	protected bool m_bPlaylistSilentSaid;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!m_bRegistered || !m_sSoundProject.IsEmpty())
			return;

		m_Playlist = LF_RadioPlaylists.Find(m_sSoundSlot);
	}

	//------------------------------------------------------------------------------------------------
	override LF_AmbientCandidate Refresh(int id, float distance, float hour)
	{
		LF_AmbientCandidate candidate = super.Refresh(id, distance, hour);
		if (m_Playlist && candidate.m_bAllowed && !m_Playlist.InHours(hour))
			candidate.m_bAllowed = false;

		return candidate;
	}

	//------------------------------------------------------------------------------------------------
	override bool Play(float distance)
	{
		if (!m_Audio && m_Playlist)
		{
			if (m_iTrack < 0)
				m_iTrack = m_Playlist.Next(-1);

			if (m_iTrack < 0)
			{
				if (!m_bPlaylistSilentSaid)
				{
					m_bPlaylistSilentSaid = true;
					LF_Debug.Error(LF_Atmosphere.FEATURE, string.Format("%1: radio playlist '%2' has no playable track; it stays silent", Label(), m_sSoundSlot));
				}

				return false;
			}

			m_sProject = m_Playlist.m_aTracks[m_iTrack].m_Project;
			m_iTrackTicks = 0;
		}

		return super.Play(distance);
	}

	//------------------------------------------------------------------------------------------------
	override void CheckVoice()
	{
		super.CheckVoice();
		if (!m_Audio || !m_Playlist || m_iTrack < 0)
			return;

		m_iTrackTicks++;
		float seconds = m_Playlist.m_aTracks[m_iTrack].m_fSeconds;
		if (m_iTrackTicks * LF_Atmosphere.TICK_MS < seconds * 1000)
			return;

		// One playable track keeps looping; the list only rotates when there is a next one.
		if (m_Playlist.PlayableCount() > 1)
			Stop("track over");
		else
			m_iTrackTicks = 0;
	}

	//------------------------------------------------------------------------------------------------
	override void Stop(string why)
	{
		bool wasPlaying = m_Audio != null;
		super.Stop(why);
		if (wasPlaying && m_Playlist && m_iTrack >= 0)
			m_iTrack = m_Playlist.Next(m_iTrack);
	}
}
