// LF_UISound.c — a sheet's own sounds (key click, accept, refuse) through the vanilla UI sound
// entity. Event names come from config (the opening action's attributes), so a server can swap
// them. The world cue (cash changing hands) is played on the character by the server.
class LF_UISound
{
	//------------------------------------------------------------------------------------------------
	//! Plays a UI sound event on this machine. Empty plays nothing. No-op on a dedicated server.
	static void Play(string eventName)
	{
		if (eventName.IsEmpty() || System.IsConsoleApp())
			return;

		SCR_UISoundEntity.SoundEvent(eventName);
	}
}
