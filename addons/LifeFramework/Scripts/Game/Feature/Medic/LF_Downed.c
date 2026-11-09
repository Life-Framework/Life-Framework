//------------------------------------------------------------------------------------------------
//! The downed state (#255, first step of the Medic epic #146). Lethal damage to a player does
//! not kill them: the server leaves them alive at a sliver of health, in vanilla unconsciousness,
//! and publishes LF_OnPlayerDowned. The real death (and with it the murder hook and the hospital
//! wake) happens later, through Finish, which the bleed-out timer (#256) calls; a revive (#259)
//! calls Clear.
//!
//! Server-authoritative. The flag lives on the character (SCR_ChimeraCharacter.LF_IsDowned, an
//! RplProp, so a late joiner reads it with the entity snapshot) and keeps the character
//! unconscious through SCR_CharacterDamageManagerComponent.ShouldBeUnconscious: resilience
//! regeneration cannot wake a downed player. The unconscious life state itself is vanilla's and
//! replicates by the engine. A downed player takes no further damage; that is what makes the
//! death a single, attributable event.
//!
//! Entry: SCR_CharacterHealthHitZone.ComputeEffectiveDamage / OnDamage at the bottom of this file.
//------------------------------------------------------------------------------------------------
class LF_Downed
{
	static const string FEATURE = "Medic";

	//! Decisions of Decide.
	static const int PASS = 0;
	static const int DOWN = 1;
	static const int ABSORB = 2;

	//! Smallest damage a lethal hit still deals (health points).
	static const float MIN_LETHAL_CUT = 0.01;

	protected static int s_iLethalBypass;
	protected static bool s_bTestAllEligible;
	protected static int s_iDownedCount;
	protected static int s_iAbsorbedCount;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iLethalBypass = 0;
		s_bTestAllEligible = false;
		s_iDownedCount = 0;
		s_iAbsorbedCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure rule for one hit on the Health hit zone.
	//! \param enabled Config: the downed state is on.
	//! \param eligible The character is a player's (NPCs die as in vanilla).
	//! \param alreadyDowned The character is already downed.
	//! \param unconsciousnessPermitted The game mode allows vanilla unconsciousness.
	//! \param lethalBypass A caller asked for a real kill (Finish, the dev die command).
	//! \param effectiveDamage Damage the hit zone would take.
	//! \param health The hit zone's current health.
	//! \return PASS (vanilla handles the hit), DOWN (down the player), ABSORB (discard the hit).
	static int Decide(bool enabled, bool eligible, bool alreadyDowned, bool unconsciousnessPermitted, bool lethalBypass, float effectiveDamage, float health)
	{
		if (lethalBypass || !enabled || !eligible)
			return PASS;

		if (effectiveDamage <= 0)
			return PASS;

		if (alreadyDowned)
			return ABSORB;

		if (!unconsciousnessPermitted)
			return PASS;

		if (effectiveDamage < health)
			return PASS;

		return DOWN;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the damage a Health hit zone is about to take. Lethal damage to a player is cut to
	//! what leaves the configured sliver of health, so the character never reaches zero and
	//! vanilla never kills it; damage to a downed player is cut to nothing. Nothing is changed
	//! here but the number (the engine may ask speculatively); a lethal cut is remembered on the
	//! zone and OnHealthDamaged, which follows the real damage, downs the player.
	//! eturn The damage the zone takes.
	static float CapHealthDamage(notnull SCR_CharacterHealthHitZone zone, notnull BaseDamageContext damageContext, float damage)
	{
		zone.LF_SetLethalPending(false);
		if (!Replication.IsServer())
			return damage;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(zone.GetOwner());
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(zone.GetHitZoneContainer());
		if (!character || !damageManager)
			return damage;

		int decision = Decide(Enabled(), IsEligible(character), character.LF_IsDowned(), damageManager.GetPermitUnconsciousness(), s_iLethalBypass > 0, damage, zone.GetHealth());
		if (decision == PASS)
			return damage;

		if (decision == ABSORB)
		{
			s_iAbsorbedCount++;
			LF_Debug.Info(FEATURE, string.Format("hit on downed %1 absorbed (%2)", PlayerUid(character), damage));
			return 0;
		}

		zone.LF_SetLethalPending(true);
		// Never less than a sliver: zero damage would not reach OnHealthDamaged (a revived player
		// still at the floor, hit again, would stay up).
		float room = zone.GetHealth() - LF_MedicConfig.Get().m_fDownedHealthScaled * zone.GetMaxHealth();
		return Math.Max(MIN_LETHAL_CUT, Math.Min(damage, room));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: damage was dealt to a Health hit zone. Downs the player when it was the lethal cut.
	static void OnHealthDamaged(notnull SCR_CharacterHealthHitZone zone, notnull BaseDamageContext damageContext)
	{
		if (!zone.LF_IsLethalPending())
			return;

		zone.LF_SetLethalPending(false);
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(zone.GetOwner());
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(zone.GetHitZoneContainer());
		if (character && damageManager)
			Down(character, damageManager, zone, damageContext.instigator);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: put a character in the downed state. The lethal hit that caused it is discarded by
	//! the caller. Nothing changes on a character that is already downed.
	//! \param instigator Who dealt the hit; may be null. Its player id becomes the downer.
	static bool Down(notnull SCR_ChimeraCharacter character, notnull SCR_CharacterDamageManagerComponent damageManager, notnull HitZone health, Instigator instigator)
	{
		if (!Replication.IsServer() || character.LF_IsDowned())
			return false;

		string downerUid;
		if (instigator)
		{
			int downerId = instigator.GetInstigatorPlayerID();
			if (downerId > 0)
				downerUid = LF_Utils.GetPlayerUID(downerId);
		}

		character.LF_SetDowned(true, downerUid);
		damageManager.UpdateConsciousness();

		s_iDownedCount++;
		string victimUid = PlayerUid(character);
		LF_Debug.Info(FEATURE, string.Format("player %1 is downed (by %2), health %3", victimUid, downerUid, health.GetHealthScaled()));
		LF_EventBus.Publish(LF_OnPlayerDowned.Create(victimUid, downerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: end the downed state and let the character wake (a revive, #259). The character
	//! keeps the sliver of health it was left with.
	static bool Clear(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!Replication.IsServer() || !character || !character.LF_IsDowned())
			return false;

		character.LF_SetDowned(false, "");
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(character.FindComponent(SCR_CharacterDamageManagerComponent));
		if (damageManager)
			damageManager.UpdateConsciousness();

		LF_Debug.Info(FEATURE, string.Format("player %1 is no longer downed", PlayerUid(character)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: kill a character for real, downed or not (bleed-out, give-up, the dev die command).
	//! The kill reaches the vanilla death path, so the murder hook and the hospital wake run.
	static bool Finish(IEntity entity, notnull Instigator instigator)
	{
		if (!entity || !Replication.IsServer())
			return false;

		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(entity.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damageManager)
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (character && character.LF_IsDowned())
			character.LF_SetDowned(false, "");

		s_iLethalBypass++;
		damageManager.Kill(instigator);
		s_iLethalBypass--;
		LF_Debug.Info(FEATURE, string.Format("player %1 killed for real", PlayerUid(entity)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A player's body may be searched or robbed. False only while the body is downed and the
	//! config forbids it; the owner always reaches their own inventory.
	static bool AllowsAccess(IEntity owner, InventoryStorageManagerComponent instigatorManager)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(owner);
		if (!character || !character.LF_IsDowned())
			return true;

		if (instigatorManager && instigatorManager.GetOwner() == owner)
			return true;

		if (LF_MedicConfig.Get().m_bDownedCanBeSearched)
			return true;

		LF_Debug.Info(FEATURE, string.Format("search of downed %1 refused by config", PlayerUid(owner)));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsDowned(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		return character.LF_IsDowned();
	}

	//------------------------------------------------------------------------------------------------
	static bool Enabled()
	{
		return LF_MedicConfig.Get().m_bDownedEnabled;
	}

	//------------------------------------------------------------------------------------------------
	//! Players are eligible; so is everything while a test has opted in.
	static bool IsEligible(IEntity character)
	{
		if (s_bTestAllEligible)
			return true;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || !character)
			return false;

		return playerManager.GetPlayerIdFromControlledEntity(character) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: spawned test characters have no player behind them.
	static void SetTestAllEligible(bool all)
	{
		s_bTestAllEligible = all;
	}

	//------------------------------------------------------------------------------------------------
	static int GetDownedCount()
	{
		return s_iDownedCount;
	}

	//------------------------------------------------------------------------------------------------
	static int GetAbsorbedCount()
	{
		return s_iAbsorbedCount;
	}

	//------------------------------------------------------------------------------------------------
	//! The uid for logs and events: the player's, else "npc" for a body with no player.
	protected static string PlayerUid(IEntity character)
	{
		string uid = LF_Utils.GetPlayerUID(character);
		if (uid.IsEmpty())
			return "npc";

		return uid;
	}
}

//------------------------------------------------------------------------------------------------
//! The downed flag on the character, replicated so every client (and a late joiner) reads it.
modded class SCR_ChimeraCharacter
{
	[RplProp()]
	protected bool m_bLF_Downed;

	//! Server only: who dealt the hit that downed this character ("" when no player did).
	protected string m_sLF_DownerUid;

	//------------------------------------------------------------------------------------------------
	void LF_SetDowned(bool downed, string downerUid)
	{
		if (!Replication.IsServer())
			return;

		m_sLF_DownerUid = downerUid;
		if (downed == m_bLF_Downed)
			return;

		m_bLF_Downed = downed;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	bool LF_IsDowned()
	{
		return m_bLF_Downed;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player who downed this character, for the murder attribution when they die (#256).
	string LF_GetDownerUid()
	{
		return m_sLF_DownerUid;
	}
}

//------------------------------------------------------------------------------------------------
//! Every route to a character's death ends at its Health hit zone: a bullet or a fall through
//! the limb pass rules, Kill, bleeding out. That is where lethal damage is cut.
modded class SCR_CharacterHealthHitZone
{
	protected bool m_bLF_LethalPending;

	//------------------------------------------------------------------------------------------------
	void LF_SetLethalPending(bool pending)
	{
		m_bLF_LethalPending = pending;
	}

	//------------------------------------------------------------------------------------------------
	bool LF_IsLethalPending()
	{
		return m_bLF_LethalPending;
	}

	//------------------------------------------------------------------------------------------------
	override float ComputeEffectiveDamage(notnull BaseDamageContext damageContext, bool isDOT)
	{
		return LF_Downed.CapHealthDamage(this, damageContext, super.ComputeEffectiveDamage(damageContext, isDOT));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDamage(notnull BaseDamageContext damageContext)
	{
		super.OnDamage(damageContext);
		LF_Downed.OnHealthDamaged(this, damageContext);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_CharacterDamageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	//! A downed character stays unconscious whatever resilience regeneration does.
	override protected bool ShouldBeUnconscious()
	{
		if (LF_Downed.IsDowned(GetOwner()))
			return true;

		return super.ShouldBeUnconscious();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the next consciousness update leaves the character unconscious. Tests read it:
	//! the controller applies the life state a frame later.
	bool LF_WantsUnconscious()
	{
		return ShouldBeUnconscious();
	}
}

//------------------------------------------------------------------------------------------------
//! A downed body is searched through the vanilla inventory, whose body access validation
//! always allows an unconscious character. Config can close it.
modded class SCR_InventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override protected bool HandleAccessValidation(InventoryStorageManagerComponent instigatorManager, IEntity item, EInventoryManagerOperation operation)
	{
		if (!LF_Downed.AllowsAccess(GetOwner(), instigatorManager))
			return false;

		return super.HandleAccessValidation(instigatorManager, item, operation);
	}
}
