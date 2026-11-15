//------------------------------------------------------------------------------------------------
//! The stun (#341, the police taser): a hit from a stun weapon downs a player for a few seconds
//! and deals no damage. It is a knockout (LF_Knockout): the downed flag holds the character in
//! vanilla unconsciousness, the owner ragdolls the body until the time is over, and vanilla's
//! wake-up animation stands it up. Nothing bleeds out, the player cannot give up, and the server
//! wakes them when the time is over.
//!
//! Server-authoritative. A weapon is a stun weapon when its prefab carries LF_StunWeaponComponent;
//! the hit is recognised by the weapon in the shooter's hands, so any magazine fired from it stuns
//! and its darts never wound a character.
//!
//! Entry: SCR_CharacterDamageManagerComponent.HijackDamageHandling at the bottom of this file.
//------------------------------------------------------------------------------------------------
class LF_Stun
{
	static const string FEATURE = "Police";

	//! Decisions of Decide.
	static const int PASS = 0;
	static const int STUN = 1;
	static const int ABSORB = 2;

	protected static int s_iStunCount;
	protected static int s_iAbsorbedCount;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iStunCount = 0;
		s_iAbsorbedCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure rule for one hit on a character.
	//! \param stunWeapon The shooter holds a stun weapon and the hit is a projectile's.
	//! \param distance Metres between shooter and victim.
	//! \param range The weapon's reach in metres.
	//! \param canDown The victim can be put in the downed state now.
	//! \return PASS (vanilla handles the hit), STUN (discard the damage and stun), ABSORB (discard only).
	static int Decide(bool stunWeapon, float distance, float range, bool canDown)
	{
		if (!stunWeapon)
			return PASS;

		if (distance > range || !canDown)
			return ABSORB;

		return STUN;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character's damage manager is about to handle a hit.
	//! \return True when the hit came from a stun weapon: its damage is discarded.
	static bool TryHit(notnull SCR_CharacterDamageManagerComponent damageManager, notnull BaseDamageContext damageContext)
	{
		if (!Replication.IsServer() || damageContext.damageType != EDamageType.KINETIC || !damageContext.instigator)
			return false;

		IEntity shooter = damageContext.instigator.GetInstigatorEntity();
		SCR_ChimeraCharacter victim = SCR_ChimeraCharacter.Cast(damageManager.GetOwner());
		if (!shooter || !victim || shooter == victim)
			return false;

		LF_StunWeaponComponent weapon = HeldStunWeapon(shooter);
		if (!weapon)
			return false;

		float distance = vector.Distance(shooter.GetOrigin(), victim.GetOrigin());
		int decision = Decide(true, distance, weapon.GetRange(), CanDown(victim, damageManager));
		if (decision == STUN && Apply(victim, weapon.GetStunSeconds(), LF_Utils.GetPlayerUID(shooter)))
			return true;

		s_iAbsorbedCount++;
		LF_Debug.Info(FEATURE, string.Format("stun dart on %1 had no effect (%2 m, reach %3 m)", Uid(victim), distance, weapon.GetRange()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stun a character for `seconds`: a knockout (LF_Knockout) that ragdolls the body, holds
	//! it down and stands it up with the vanilla wake-up. Refused for a character that is dead,
	//! downed, already unconscious, not a player's, or cannot go unconscious.
	static bool Apply(notnull SCR_ChimeraCharacter victim, float seconds, string byUid = "")
	{
		if (!LF_Knockout.Begin(victim, seconds, byUid, "stun"))
			return false;

		s_iStunCount++;
		LF_Utils.NotifyEntity(victim, "#LF-Police_Stunned", "", 4.0, Math.Ceil(seconds).ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the stun's time is over (LF_Knockout.Wake).
	static void Wake(SCR_ChimeraCharacter victim, int token)
	{
		LF_Knockout.Wake(victim, token);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: end a stun now (LF_Knockout.End).
	static bool End(notnull SCR_ChimeraCharacter victim)
	{
		return LF_Knockout.End(victim);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsStunned(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		return character.LF_IsStunned();
	}

	//------------------------------------------------------------------------------------------------
	static int GetStunCount()
	{
		return s_iStunCount;
	}

	//------------------------------------------------------------------------------------------------
	static int GetAbsorbedCount()
	{
		return s_iAbsorbedCount;
	}

	//------------------------------------------------------------------------------------------------
	//! The knockout gate: on in config, a player's character, alive, conscious, not downed, and the
	//! game mode permits unconsciousness.
	protected static bool CanDown(notnull SCR_ChimeraCharacter victim, notnull SCR_CharacterDamageManagerComponent damageManager)
	{
		return LF_Knockout.CanKnockOut(victim);
	}

	//------------------------------------------------------------------------------------------------
	//! The stun weapon in a character's hands, or null.
	static LF_StunWeaponComponent HeldStunWeapon(notnull IEntity shooter)
	{
		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(shooter.FindComponent(BaseWeaponManagerComponent));
		if (!weapons)
			return null;

		BaseWeaponComponent current = weapons.GetCurrentWeapon();
		if (!current || !current.GetOwner())
			return null;

		return LF_StunWeaponComponent.Cast(current.GetOwner().FindComponent(LF_StunWeaponComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected static string Uid(IEntity character)
	{
		string uid = LF_Utils.GetPlayerUID(character);
		if (uid.IsEmpty())
			return "npc";

		return uid;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Police", description: "Marks a weapon as a stun weapon: its hits down a player for a few seconds and deal no damage.")]
class LF_StunWeaponComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_StunWeaponComponent : ScriptComponent
{
	[Attribute("8", UIWidgets.Auto, "Seconds a hit player stays down. Zero or less: the weapon stuns nobody", params: "0 120")]
	protected float m_fStunSeconds;

	[Attribute("10", UIWidgets.Auto, "Reach in metres; a hit from further away does nothing", params: "0 100")]
	protected float m_fRange;

	//------------------------------------------------------------------------------------------------
	float GetStunSeconds()
	{
		return m_fStunSeconds;
	}

	//------------------------------------------------------------------------------------------------
	float GetRange()
	{
		return m_fRange;
	}
}

//------------------------------------------------------------------------------------------------
//! The taser cartridge fits only the taser, and a pistol magazine does not fit it.
class LF_MagazineWellTaserClass {}
LF_MagazineWellTaserClass LF_MagazineWellTaserSource;

class LF_MagazineWellTaser : BaseMagazineWell
{
}

//------------------------------------------------------------------------------------------------
modded class SCR_CharacterDamageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	//! A stun weapon's hit never reaches the hit zones.
	override bool HijackDamageHandling(notnull BaseDamageContext damageContext)
	{
		if (LF_Stun.TryHit(this, damageContext))
			return true;

		return super.HijackDamageHandling(damageContext);
	}
}
