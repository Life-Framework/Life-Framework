//------------------------------------------------------------------------------------------------
//! The hold-up (#244, epic #145 Moderation): a "Hold up" entry in the player interaction menu
//! (`holdup` role slot), offered with a weapon in hand. It raises a hand (vanilla gesture), plays
//! a cue at both characters, and publishes LF_OnHoldUp so the incident log records the intent
//! (actor, target, position, zone). It changes no damage rule: talk first, and the log shows who did.
//!
//! The server decides: range and cooldown come from Configs/Moderation/LF_Incident.conf
//! (m_fHoldUpRange, m_iHoldUpCooldownSeconds). The menu itself reaches LF_PlayerInteraction.REACH,
//! so a larger range changes nothing until the menu reaches further.
//------------------------------------------------------------------------------------------------
enum LF_EHoldUp
{
	OK,
	OUT_OF_RANGE,
	COOLDOWN,
	NO_WEAPON
}

//------------------------------------------------------------------------------------------------
class LF_HoldUp
{
	static const string FEATURE = "HoldUp";
	static const string SLOT = "holdup";

	protected static ref map<string, int> s_mLast;
	//! Target uid -> unix seconds of the last hold-up aimed at them (LF_Restraint reads it).
	protected static ref map<string, int> s_mHeldUpAt;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mLast = null;
		s_mHeldUpAt = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: when targetUid was last held up, or -1 for never.
	static int HeldUpAt(string targetUid)
	{
		if (!s_mHeldUpAt || !s_mHeldUpAt.Contains(targetUid))
			return -1;

		return s_mHeldUpAt.Get(targetUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: may this hold-up happen? last is the actor's previous hold-up time, or -1 for none.
	static LF_EHoldUp Decide(float distance, float range, bool armed, int now, int last, int cooldownSeconds)
	{
		if (!armed)
			return LF_EHoldUp.NO_WEAPON;

		if (distance > range)
			return LF_EHoldUp.OUT_OF_RANGE;

		if (last >= 0 && now - last < cooldownSeconds)
			return LF_EHoldUp.COOLDOWN;

		return LF_EHoldUp.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: decide, log, and on OK publish the intent and start the actor's cooldown.
	static LF_EHoldUp Attempt(string actorUid, string targetUid, vector position, float distance, bool armed, int now)
	{
		if (!Replication.IsServer())
			return LF_EHoldUp.OUT_OF_RANGE;

		if (!s_mLast)
			s_mLast = new map<string, int>();

		int last = -1;
		if (s_mLast.Contains(actorUid))
			last = s_mLast.Get(actorUid);

		LF_EHoldUp result = Decide(distance, LF_IncidentConfig.HoldUpRange(), armed, now, last, LF_IncidentConfig.HoldUpCooldownSeconds());
		if (result != LF_EHoldUp.OK)
		{
			LF_Debug.Info(FEATURE, string.Format("hold-up refused: %1 -> %2 (%3, %4 m)", actorUid, targetUid, typename.EnumToString(LF_EHoldUp, result), distance));
			return result;
		}

		s_mLast.Set(actorUid, now);
		if (!s_mHeldUpAt)
			s_mHeldUpAt = new map<string, int>();

		s_mHeldUpAt.Set(targetUid, now);
		LF_Debug.Info(FEATURE, string.Format("hold-up: %1 -> %2 at %3", actorUid, targetUid, position));
		LF_EventBus.Publish(LF_OnHoldUp.Create(actorUid, targetUid, position));
		return result;
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_EHoldUp result)
	{
		if (result == LF_EHoldUp.COOLDOWN)
			return "#LF-HoldUp_Cooldown";

		if (result == LF_EHoldUp.NO_WEAPON)
			return "#LF-HoldUp_NoWeapon";

		return "#LF-HoldUp_TooFar";
	}

	//------------------------------------------------------------------------------------------------
	//! A weapon is in the character's hands.
	static bool IsArmed(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return false;

		CharacterControllerComponent controller = chimera.GetCharacterController();
		if (!controller)
			return false;

		BaseWeaponManagerComponent weapons = controller.GetWeaponManagerComponent();
		if (!weapons)
			return false;

		return weapons.GetCurrentWeapon() != null;
	}
}

//------------------------------------------------------------------------------------------------
class LF_HoldUpRole : LF_PlayerRoleHandler
{
	//! Vanilla halt gesture: a raised open hand (Configs/Commanding/Gestures).
	static const int GESTURE_ID = 2;
	static const string SOUND_CUE = "SOUND_RADIO_SIGNAL_2";

	//------------------------------------------------------------------------------------------------
	//! Every machine, once per world (LF_GameModeRoleplay.OnGameStart). Idempotent.
	static void Register()
	{
		if (LF_HoldUpRole.Cast(LF_PlayerInteraction.GetRole(LF_HoldUp.SLOT)))
			return;

		LF_PlayerInteraction.RegisterRole(LF_HoldUp.SLOT, new LF_HoldUpRole());
	}

	//------------------------------------------------------------------------------------------------
	override string GetActionName()
	{
		return "#LF-HoldUp_Action";
	}

	//------------------------------------------------------------------------------------------------
	override bool CanOffer(IEntity user, IEntity target)
	{
		return LF_HoldUp.IsArmed(user);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(IEntity user, IEntity target)
	{
		string actorUid = LF_Utils.GetPlayerUID(user);
		string targetUid = LF_Utils.GetPlayerUID(target);
		if (actorUid.IsEmpty() || targetUid.IsEmpty())
		{
			LF_Debug.Error(LF_HoldUp.FEATURE, "hold-up refused: a player has no account id");
			return;
		}

		float distance = vector.Distance(user.GetOrigin(), target.GetOrigin());
		LF_EHoldUp result = LF_HoldUp.Attempt(actorUid, targetUid, user.GetOrigin(), distance, LF_HoldUp.IsArmed(user), System.GetUnixTime());
		if (result != LF_EHoldUp.OK)
		{
			LF_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(user), "#LF-HoldUp_Title", LF_HoldUp.RefusalKey(result), 3.0, LF_ENotificationType.WARNING);
			return;
		}

		LF_PlayerInteraction.PlayGesture(user, GESTURE_ID);
		LF_PlayerInteraction.PlayHandoverSound(user, SOUND_CUE);
		LF_PlayerInteraction.PlayHandoverSound(target, SOUND_CUE);
		LF_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(target), "#LF-HoldUp_Title", "#LF-HoldUp_HeldUp", 4.0, LF_ENotificationType.WARNING);
	}
}
