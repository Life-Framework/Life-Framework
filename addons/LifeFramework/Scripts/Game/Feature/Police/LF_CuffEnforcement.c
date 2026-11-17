//------------------------------------------------------------------------------------------------
//! Cuffs enforced on the server (#2417). The owner's controller disables weapon controls on the
//! cuffed player's own machine (LF_Restraint), but a modified client ignores that. The server
//! therefore refuses, for a character whose replicated cuffed flag is set:
//! - damage it instigates (a shot, a punch, a dart): swallowed in HijackDamageHandling on every
//!   damage manager, characters included, before the stun hook can use it;
//! - a driver's seat: a cuffed character who reaches one is put out again, and a driver who is
//!   cuffed is put out at once (a cargo seat stays allowed: officers transport prisoners);
//! - robbery (till, held till, vehicle, weapon, vault), lockpicking and the player interaction
//!   menu, at each one's server entry point, through Refuse().
//! The client blocks stay for the honest player's UX; these are the authority.
//------------------------------------------------------------------------------------------------
class LF_CuffEnforcement
{
	static const string FEATURE = "Police";

	//! What the last refusal was for, so a WORLD test can tell a cuff refusal from any other.
	protected static string s_sLastRefused;
	//! Cuffed drivers to put out next frame (the call queue cannot carry an EntityID).
	protected static ref array<EntityID> s_aPutOut;
	protected static ref array<int> s_aTries;
	//! A driver still seated after an ask is asked again, this often, this many times.
	protected static const int RETRY_MS = 1000; // lf-fixed: an anti-cheat retry, not a gameplay tunable
	protected static const int MAX_TRIES = 5; // lf-fixed: an anti-cheat retry, not a gameplay tunable

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sLastRefused = string.Empty;
		s_aPutOut = null;
		s_aTries = null;
	}

	//------------------------------------------------------------------------------------------------
	static string LastRefused()
	{
		return s_sLastRefused;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: true when `user` is cuffed, so `what` is refused. Logs it and tells the player.
	static bool Refuse(IEntity user, string what)
	{
		if (!LF_Restraint.IsRestrained(user))
			return false;

		s_sLastRefused = what;
		LF_Debug.Warn(FEATURE, string.Format("refused %1 from cuffed %2", what, LF_Utils.GetPlayerUID(user)));
		int playerId = LF_PlayerInteraction.PlayerIdOf(user);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Restrain_Title", "#LF-Restrain_NotWhileCuffed", 3.0, LF_ENotificationType.WARNING);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: is a hit refused? A cuffed instigator hurts nobody but itself (a fall stays a fall), and
	//! damage over time already running (bleeding from an earlier shot) keeps running.
	static bool RefusesHit(bool instigatorRestrained, bool selfDamage, bool overTime)
	{
		return instigatorRestrained && !selfDamage && !overTime;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the damage hook. True swallows the hit.
	static bool RefuseHit(IEntity victim, notnull BaseDamageContext damageContext)
	{
		if (!Replication.IsServer() || !damageContext.instigator)
			return false;

		IEntity shooter = damageContext.instigator.GetInstigatorEntity();
		if (!shooter)
			return false;

		bool overTime = DotDamageEffect.Cast(damageContext.damageEffect) != null;
		bool self = victim && shooter == victim;
		if (!RefusesHit(LF_Restraint.IsRestrained(shooter), self, overTime))
			return false;

		s_sLastRefused = "damage";
		LF_Debug.Warn(FEATURE, string.Format("refused %1 damage from cuffed %2", damageContext.damageValue, LF_Utils.GetPlayerUID(shooter)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: may this character sit in this seat?
	static bool RefusesSeat(bool restrained, bool piloting)
	{
		return restrained && piloting;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a cuffed character in a driver's seat is put out of it. \return True when it was refused.
	static bool RefuseSeat(IEntity character, BaseCompartmentSlot slot)
	{
		if (!Replication.IsServer() || !character || !slot)
			return false;

		if (!RefusesSeat(LF_Restraint.IsRestrained(character), PilotCompartmentSlot.Cast(slot) != null))
			return false;

		s_sLastRefused = "driver seat";
		LF_Debug.Warn(FEATURE, string.Format("cuffed %1 is in a driver's seat: put out", LF_Utils.GetPlayerUID(character)));
		int playerId = LF_PlayerInteraction.PlayerIdOf(character);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Restrain_Title", "#LF-Restrain_NotWhileCuffed", 3.0, LF_ENotificationType.WARNING);

		// Not from inside the seat event that brought them in: next frame.
		Queue(character.GetID(), 0);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: called when the cuffs land. A cuffed driver leaves the seat.
	static void OnCuffed(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return;

		CompartmentAccessComponent access = chimera.GetCompartmentAccessComponent();
		if (access)
			RefuseSeat(character, access.GetCompartment());
	}

	//------------------------------------------------------------------------------------------------
	protected static void Queue(EntityID characterId, int tries)
	{
		if (!s_aPutOut)
			s_aPutOut = {};

		if (!s_aTries)
			s_aTries = {};

		if (s_aPutOut.Contains(characterId))
			return;

		if (s_aPutOut.IsEmpty() && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(PutOutPending, RETRY_MS, false);

		s_aPutOut.Insert(characterId);
		s_aTries.Insert(tries);
	}

	//------------------------------------------------------------------------------------------------
	protected static void PutOutPending()
	{
		if (!s_aPutOut || !s_aTries)
			return;

		array<EntityID> pending = {};
		array<int> tries = {};
		pending.Copy(s_aPutOut);
		tries.Copy(s_aTries);
		s_aPutOut.Clear();
		s_aTries.Clear();
		foreach (int i, EntityID characterId : pending)
		{
			if (!PutOut(characterId))
				continue;

			if (tries[i] + 1 < MAX_TRIES)
				Queue(characterId, tries[i] + 1);
			else
				LF_Debug.Error(FEATURE, "a cuffed driver was asked out of the seat 5 times and is still there: the owning client is not moving them");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Asks once. eturn True while the character is still cuffed in a driver's seat (try again).
	protected static bool PutOut(EntityID characterId)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(GetGame().GetWorld().FindEntityByID(characterId));
		if (!character || !LF_Restraint.IsRestrained(character))
			return false;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (!access || !PilotCompartmentSlot.Cast(access.GetCompartment()))
			return false;

		// Beside the vehicle, the way LF_Jail dismounts a prisoner: a body this machine owns gets
		// out here; a body a remote client owns is moved by that client through the vanilla Game
		// Master path, which asks the owner (a server-side get-out does not move it).
		vector transform[4];
		character.GetWorldTransform(transform);
		IEntity vehicle = access.GetCompartment().GetVehicle();
		if (vehicle)
			transform[3] = vehicle.GetOrigin() + vehicle.GetTransformAxis(0).Normalized() * -2.5;

		RplComponent rpl = RplComponent.Cast(character.FindComponent(RplComponent));
		if (!rpl || rpl.IsOwner())
		{
			access.GetOutVehicle_NoDoor(transform, false, true);
		}
		else
		{
			SCR_EditableCharacterComponent editable = SCR_EditableCharacterComponent.Cast(character.FindComponent(SCR_EditableCharacterComponent));
			if (!editable)
			{
				LF_Debug.Error(FEATURE, "a cuffed driver cannot be put out: no SCR_EditableCharacterComponent to ask the owner");
				return false;
			}

			editable.SetTransform(transform);
		}

		LF_Debug.Info(FEATURE, string.Format("asked cuffed %1 out of the driver's seat", LF_Utils.GetPlayerUID(character)));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Characters: their vanilla HijackDamageHandling does not call super, so the hook is here too.
modded class SCR_CharacterDamageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
	{
		if (LF_CuffEnforcement.RefuseHit(GetOwner(), damageContext))
			return true;

		return super.HijackDamageHandling(damageContext);
	}
}

//------------------------------------------------------------------------------------------------
//! Vehicles, props and everything else whose damage manager reaches the base hook.
modded class SCR_DamageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
	{
		if (LF_CuffEnforcement.RefuseHit(GetOwner(), damageContext))
			return true;

		return super.HijackDamageHandling(damageContext);
	}
}

//------------------------------------------------------------------------------------------------
//! The seat event fires on the server for every character, whichever machine asked to get in.
modded class SCR_CompartmentAccessComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void OnCompartmentEntered(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
	{
		super.OnCompartmentEntered(targetEntity, manager, mgrID, slotID, move);
		if (Replication.IsServer())
			LF_CuffEnforcement.RefuseSeat(GetOwner(), GetCompartment());
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_CuffEnforcement's statics (registered at script load; Core/LF_StaticReset.c).
class LF_CuffEnforcement_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_CuffEnforcement_StaticResetHook());

	override string Owner()
	{
		return "LF_CuffEnforcement";
	}

	override void Apply()
	{
		LF_CuffEnforcement.ResetStatics();
	}
}
