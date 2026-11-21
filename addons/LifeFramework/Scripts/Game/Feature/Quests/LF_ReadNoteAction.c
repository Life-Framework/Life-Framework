//------------------------------------------------------------------------------------------------
//! "Read" on a story note lying in the world (a letter, a slip). Shows the note's text to the
//! reader as a long toast. The words are two localization keys set on the prefab's action, so a
//! pack writes its own letters and the core names no place or person (#3793).
//! Runs on the server: the toast goes through the notification manager to the reader only.
class LF_ReadNoteAction : ScriptedUserAction
{
	static const string FEATURE = "Quests";
	//! A toast long enough to read a short letter once.
	static const float DEFAULT_SECONDS = 25;	// lf-fixed: fallback when a prefab sets no duration; the prefab attribute is the tunable

	[Attribute("", UIWidgets.EditBox, "Toast title (a localization key): who the note is from")]
	protected string m_sTitleKey;

	[Attribute("", UIWidgets.EditBox, "Toast text (a localization key): the note itself")]
	protected string m_sTextKey;

	[Attribute("25", UIWidgets.EditBox, "Seconds the toast stays up")]
	protected float m_fSeconds;

	//------------------------------------------------------------------------------------------------
	//! The key the toast shows, or empty when the prefab sets none.
	string GetTextKey()
	{
		return m_sTextKey;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return !m_sTextKey.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (m_sTextKey.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "Read note performed on a prefab with no text key; nothing shown");
			return;
		}

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (playerId <= 0)
			return;

		float seconds = m_fSeconds;
		if (seconds <= 0)
			seconds = DEFAULT_SECONDS;

		LF_NotificationManagerComponent.NotifyPlayer(playerId, m_sTitleKey, m_sTextKey, seconds);
		LF_Debug.Info(FEATURE, string.Format("note read by player %1: %2", playerId, m_sTextKey));
	}
}
