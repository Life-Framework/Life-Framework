//------------------------------------------------------------------------------------------------
//! The booking desk's sounds (#4431, police L9): when a suspect is booked the sergeant's typewriter
//! clacks out the sheet and the ink pad stamp lands on it. The core asks for the `booking_typewriter`
//! and `booking_stamp` sound slots; the flagship pack fills them (Sounds/Police/Booking_*.acp). A slot
//! with no sound stays silent and says so once.
//!
//! The server announces the booking through the booking officer's character (a replicated entity every
//! nearby machine streams); each machine plays both sounds from that character, the stamp a moment
//! after the typewriter. No sound is played on a dedicated server.
class LF_BookingSounds
{
	static const string FEATURE = "BookingSounds";
	static const string SLOT_TYPEWRITER = "booking_typewriter";
	static const string SLOT_STAMP = "booking_stamp";
	static const string EVENT_TYPEWRITER = "SOUND_LF_BOOKING_TYPEWRITER";
	static const string EVENT_STAMP = "SOUND_LF_BOOKING_STAMP";
	//! The stamp lands after the typewriter burst has finished.
	static const int STAMP_DELAY_MS = 2600; // lf-fixed: audio pacing, a typewriter burst is about 3 s

	protected static int s_iAnnounced;
	protected static bool s_bWarnedSilent;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iAnnounced = 0;
		s_bWarnedSilent = false;
	}

	//------------------------------------------------------------------------------------------------
	//! How many booking announcements this world has made (a test reads it).
	static int Announced()
	{
		return s_iAnnounced;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a suspect was booked. Returns how many of the two slots have a sound to play (0 to 2).
	static int OnBooked(IEntity officer)
	{
		int ready;
		if (!LF_SoundSlots.Get(SLOT_TYPEWRITER).IsEmpty())
			ready++;
		if (!LF_SoundSlots.Get(SLOT_STAMP).IsEmpty())
			ready++;

		s_iAnnounced++;
		LF_Debug.Info(FEATURE, string.Format("booking sounds: typewriter then stamp, %1 of 2 slots filled", ready));
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(officer);
		if (character)
			character.LF_BroadcastBookingSounds();
		else
			LF_Debug.Warn(FEATURE, "booking sounds skipped: the booking officer is not a character");

		return ready;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the typewriter now, the stamp after it, both from the officer's character.
	static void PlayFrom(IEntity officer)
	{
		if (!officer)
			return;

		PlaySlot(officer, SLOT_TYPEWRITER, EVENT_TYPEWRITER);
		GetGame().GetCallqueue().CallLater(PlaySlot, STAMP_DELAY_MS, false, officer, SLOT_STAMP, EVENT_STAMP);
	}

	//------------------------------------------------------------------------------------------------
	protected static void PlaySlot(IEntity officer, string slot, string soundEvent)
	{
		if (!officer)
			return;

		ResourceName project = LF_SoundSlots.Get(slot);
		if (project.IsEmpty())
		{
			if (!s_bWarnedSilent)
				LF_Debug.Warn(FEATURE, string.Format("no sound for the %1 slot yet: the booking is silent", slot));

			s_bWarnedSilent = true;
			return;
		}

		vector mat[4];
		officer.GetWorldTransform(mat);
		mat[3] = officer.GetOrigin() + "0 1 0";
		if (AudioSystem.PlayEvent(project, soundEvent, mat) == AudioHandle.Invalid)
			LF_Debug.Error(FEATURE, string.Format("booking sound %1 did not start in %2", soundEvent, project));
	}
}

//------------------------------------------------------------------------------------------------
class LF_BookingSounds_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BookingSounds";
	}

	override void Apply()
	{
		LF_BookingSounds.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Server: every machine near the booking officer plays the typewriter and the stamp. A listen host
	//! plays it itself, because a Broadcast never runs on the server.
	void LF_BroadcastBookingSounds()
	{
		if (!Replication.IsServer())
			return;

		Rpc(RpcDo_LF_BookingSounds);
		RpcDo_LF_BookingSounds();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_BookingSounds()
	{
		// A dedicated server renders and hears nothing.
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		LF_BookingSounds.PlayFrom(this);
	}
}
