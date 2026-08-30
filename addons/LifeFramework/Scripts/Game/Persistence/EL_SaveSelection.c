//------------------------------------------------------------------------------------------------
//! Plain record of a save point's type mask and playthrough, so the autosave
//! overwrite selection can be tested headlessly without constructing the native
//! SaveGame class.
class EL_SaveInfo
{
	//! ESaveGameType as int. It is a bitmask, so a combined type carries more than one flag.
	int m_iType;
	int m_iPlaythrough;

	//------------------------------------------------------------------------------------------------
	static EL_SaveInfo Create(int type, int playthrough)
	{
		EL_SaveInfo info();
		info.m_iType = type;
		info.m_iPlaythrough = playthrough;
		return info;
	}
}

//------------------------------------------------------------------------------------------------
//! Decides which existing AUTO save an autosave should overwrite.
//!
//! Two lessons the base game paid for, encoded here so a test owns them
//! (SCR_SaveSessionToolbarAction.c:56-67):
//!  1. ESaveGameType is a BITMASK, not an ordinal. An equality test matches a
//!     single-flag save and silently matches nothing for a combined one, so the
//!     membership test must be `(type & AUTO) != 0`.
//!  2. GetSaves() returns every save for the mission across all playthroughs.
//!     Overwriting the latest AUTO without checking whose playthrough it
//!     belongs to would let a fresh campaign scribble over the previous one.
class EL_SaveSelection
{
	//------------------------------------------------------------------------------------------------
	//! \return Index of the newest AUTO save for the given playthrough, or -1
	//!         when none exists (caller should create a new save instead).
	static int FindLatestAutoSaveIndex(array<ref EL_SaveInfo> saves, int currentPlaythrough)
	{
		if (!saves)
			return -1;

		for (int i = saves.Count() - 1; i >= 0; i--)
		{
			EL_SaveInfo save = saves[i];
			if (!save)
				continue;

if ((save.m_iType & ESaveGameType.AUTO) == 0)
			continue;

			if (save.m_iPlaythrough != currentPlaythrough)
				continue;

			return i;
		}

		return -1;
	}
}