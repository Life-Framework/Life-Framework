//------------------------------------------------------------------------------------------------
//! What may eat, drink or throw food (one place, used by the effect, the throw and the tests).
//! A refusal is a localization key, "" when the character may.
//------------------------------------------------------------------------------------------------
class LF_ConsumeRules
{
	static const string FEATURE = "Survival";

	//! Items consumed this session, newest last; an item entity counts once (the engine can report
	//! one finished use more than once). Capped, an entity id is only reused after it is long gone.
	static const int SPENT_KEEP = 64; // lf-fixed: ring size of the once-per-item ledger
	protected static ref array<EntityID> s_aSpent;

	//------------------------------------------------------------------------------------------------
	//! "" when `character` may eat, drink or throw now; else the key that says why not. Dead, downed
	//! or knocked out, cuffed, seated in a vehicle (the driver too), swimming.
	static string RefusalFor(IEntity character)
	{
		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(character);
		if (!chimera)
			return "#LF-Consume_Refused_Busy";

		CharacterControllerComponent controller = chimera.GetCharacterController();
		if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE)
			return "#LF-Consume_Refused_Down";

		if (LF_Downed.IsDowned(chimera))
			return "#LF-Consume_Refused_Down";

		if (LF_Restraint.IsRestrained(chimera))
			return "#LF-Consume_Refused_Cuffed";

		CompartmentAccessComponent access = chimera.GetCompartmentAccessComponent();
		if (access && access.IsInCompartment())
			return "#LF-Consume_Refused_Vehicle";

		CharacterMovementComponent movement = CharacterMovementComponent.Cast(chimera.FindComponent(CharacterMovementComponent));
		if (movement && movement.IsSwimming())
			return "#LF-Consume_Refused_Water";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take this item's one consumption. False when it was already consumed.
	static bool Claim(IEntity item)
	{
		if (!item)
			return true;

		if (!s_aSpent)
			s_aSpent = new array<EntityID>();

		EntityID id = item.GetID();
		if (s_aSpent.Find(id) != -1)
			return false;

		s_aSpent.Insert(id);
		if (s_aSpent.Count() > SPENT_KEEP)
			s_aSpent.RemoveOrdered(0);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A stack gave up one unit and lives on: its next unit may be consumed.
	static void Release(IEntity item)
	{
		if (!item || !s_aSpent)
			return;

		int at = s_aSpent.Find(item.GetID());
		if (at != -1)
			s_aSpent.RemoveOrdered(at);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aSpent = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Local machine: a short toast for a refused eat, drink or throw.
	static void Tell(string key)
	{
		if (key.IsEmpty())
			return;

		LF_NotificationManagerComponent manager = LF_NotificationManagerComponent.GetInstance();
		if (!manager)
			return;

		manager.ShowNotificationToast("#LF-Consume_Title", key, 3, LF_ENotificationType.WARNING);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ConsumeRules' statics (LF_StaticReset finds it by reflection).
class LF_ConsumeRules_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ConsumeRules";
	}

	override void Apply()
	{
		LF_ConsumeRules.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The shared base of every food and drink effect (`LF_ConsumableEffectSurvival`,
//! `LF_ConsumableEffectImpair`). Vanilla's effect base has no use animation and no use rule, so an
//! item carrying a bare effect gave no Use action and no animation. This one adds both:
//!   * CanApplyEffect: the use is refused unless LF_ConsumeRules allows it (checked when the
//!     player starts, and again on the server when the animation completes);
//!   * the use animation is the vanilla item action (`CMD_Item_Action`, the one the canteen
//!     plays), with the hand pose and graph from the prefab's ItemAnimationAttributes;
//!   * ApplyEffect runs on the server once the animation completes, once per item, and calls
//!     `ApplyNeeds` for what the item actually does (hunger, thirst, impairment).
//! The shared settings (use mask, inventory use, duration, animation) live on the base food and
//! drink prefabs, so a new item inherits all of it and sets only its numbers.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_ConsumableEffectEat : SCR_ConsumableEffectBase
{
	//! The localization key of the last refusal on this machine (the toast reads it).
	protected static string s_sLastRefusal;

	//------------------------------------------------------------------------------------------------
	//! Defaults for every food and drink; the base prefabs also say them, so a prefab that sets
	//! neither still gets an inventory Use and a short use animation.
	void LF_ConsumableEffectEat()
	{
		m_bAllowInventoryUseWithoutHitZone = true;
		m_fApplyToSelfDuration = 4;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanApplyEffect(notnull IEntity target, notnull IEntity user, out SCR_EConsumableFailReason failReason)
	{
		failReason = SCR_EConsumableFailReason.NONE;
		if (target != user)
		{
			failReason = SCR_EConsumableFailReason.NOT_APPLICABLE;
			return false;
		}

		s_sLastRefusal = LF_ConsumeRules.RefusalFor(user);
		if (!s_sLastRefusal.IsEmpty())
		{
			failReason = SCR_EConsumableFailReason.NOT_APPLICABLE;
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnApplyEffectRejected(SCR_EConsumableFailReason failReason)
	{
		LF_ConsumeRules.Tell(s_sLastRefusal);
		LF_Debug.Info(LF_ConsumeRules.FEATURE, "use refused: " + s_sLastRefusal);
	}

	//------------------------------------------------------------------------------------------------
	override bool ActivateEffect(IEntity target, IEntity user, IEntity item, ItemUseParameters animParams = null)
	{
		ItemUseParameters useParams = animParams;
		if (!useParams && target && item)
			useParams = GetAnimationParameters(item, target);

		if (!useParams)
		{
			LF_Debug.Warn(LF_ConsumeRules.FEATURE, "use not started: no animation parameters");
			return false;
		}

		bool started = super.ActivateEffect(target, user, item, useParams);
		LF_Debug.Info(LF_ConsumeRules.FEATURE, string.Format("use %1 for %2 (command %3, %4 s)", started, LF_Utils.GetPrefabName(item), useParams.GetCommandID(), useParams.GetMaxAnimLength()));
		return started;
	}

	//------------------------------------------------------------------------------------------------
	//! The vanilla item-action command (the canteen's): the player graph plays the item's hand pose.
	override bool UpdateAnimationCommands(IEntity user)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		CharacterAnimationComponent animation = character.GetAnimationComponent();
		if (!animation)
			return false;

		m_iPlayerApplyToSelfCmdId = animation.BindCommand("CMD_Item_Action");
		m_iPlayerApplyToOtherCmdId = m_iPlayerApplyToSelfCmdId;
		if (m_iPlayerApplyToSelfCmdId < 0)
		{
			LF_Debug.Error(LF_ConsumeRules.FEATURE, "CMD_Item_Action is not in this character's animation graph: eating and drinking play no animation");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, when the animation completes (a test calls it directly).
	override void ApplyEffect(notnull IEntity target, notnull IEntity user, IEntity item, ItemUseParameters animParams)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Info(LF_ConsumeRules.FEATURE, "use complete on a client; the server applies it");
			return;
		}

		string why = LF_ConsumeRules.RefusalFor(target);
		if (!why.IsEmpty())
		{
			LF_Debug.Warn(LF_ConsumeRules.FEATURE, "consume refused on the server: " + why);
			return;
		}

		if (!LF_ConsumeRules.Claim(item))
		{
			LF_Debug.Warn(LF_ConsumeRules.FEATURE, "consume ignored: that item was already consumed");
			return;
		}

		if (item)
		{
			InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
			if (itemComponent)
				itemComponent.RequestUserLock(user, false);
		}

		ApplyNeeds(target, user, item);
	}

	//------------------------------------------------------------------------------------------------
	//! What this item does to the character once consumed. Server only.
	protected void ApplyNeeds(notnull IEntity target, notnull IEntity user, IEntity item);
}

//------------------------------------------------------------------------------------------------
//! A stack of food is eaten one unit at a time; the vanilla use deletes the item, which would eat the
//! whole stack. Also reads the held item's use for the throw input.
modded class SCR_ConsumableItemComponent
{
	protected bool m_bLF_ThrowListening;
	protected EntityID m_LF_OwnerId;

	//------------------------------------------------------------------------------------------------
	override void ApplyItemEffect(IEntity target, IEntity user, ItemUseParameters animParams, IEntity item, bool deleteItem = true)
	{
		LF_QuantityComponent quantity;
		if (deleteItem && item && Replication.IsServer() && LF_ConsumableEffectEat.Cast(GetConsumableEffect()))
			quantity = LF_QuantityComponent.Cast(item.FindComponent(LF_QuantityComponent));

		bool oneOfMany = quantity && quantity.GetQuantity() > 1;
		super.ApplyItemEffect(target, user, animParams, item, deleteItem && !oneOfMany);

		if (oneOfMany)
		{
			quantity.RemoveQuantity(1);
			LF_ConsumeRules.Release(item);
			LF_Debug.Info(LF_ConsumeRules.FEATURE, string.Format("one of a stack consumed, %1 left", quantity.GetQuantity()));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: launch this thrown item here and on every client (see LF_FoodThrow.Launch).
	void LF_LaunchEverywhere(vector from, vector velocity)
	{
		Rpc(RpcDo_LF_Launch, from, velocity);
		RpcDo_LF_Launch(from, velocity); // a Broadcast never runs on the server: the server's own copy launches here
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_Launch(vector from, vector velocity)
	{
		LF_FoodThrow.WakeAndLaunch(GetOwner(), from, velocity);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnApplyToCharacter(IEntity item, bool successful, ItemUseParameters animParams)
	{
		if (LF_ConsumableEffectEat.Cast(GetConsumableEffect()))
		{
			// The engine can clear the gadget's owner (the item left the hands when the use ended) before
			// it reports the use finished; vanilla then drops the effect without a word and the food is
			// gone for nothing. The owner recorded when the item was taken in hand stands in.
			if (successful && !m_CharacterOwner)
			{
				m_CharacterOwner = ChimeraCharacter.Cast(GetGame().GetWorld().FindEntityByID(m_LF_OwnerId));
				LF_Debug.Info(LF_ConsumeRules.FEATURE, string.Format("use finished with no owner on %1: restored=%2", LF_Utils.GetPrefabName(item), m_CharacterOwner != null));
			}
		}

		super.OnApplyToCharacter(item, successful, animParams);
	}

	//------------------------------------------------------------------------------------------------
	override void ModeSwitch(EGadgetMode mode, IEntity charOwner)
	{
		super.ModeSwitch(mode, charOwner);

		if (!LF_ConsumableEffectEat.Cast(GetConsumableEffect()))
			return;

		if (mode == EGadgetMode.IN_HAND && charOwner)
			m_LF_OwnerId = charOwner.GetID();

		bool local = charOwner && charOwner == SCR_PlayerController.GetLocalControlledEntity();
		LF_ListenForThrow(mode == EGadgetMode.IN_HAND && local);
	}

	//------------------------------------------------------------------------------------------------
	override void ModeClear(EGadgetMode mode)
	{
		super.ModeClear(mode);

		if (mode == EGadgetMode.IN_HAND)
			LF_ListenForThrow(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Local machine: the throw key counts only while this food is in the hands.
	protected void LF_ListenForThrow(bool on)
	{
		if (on == m_bLF_ThrowListening)
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		if (on)
			input.AddActionListener(LF_FoodThrow.ACTION, EActionTrigger.DOWN, LF_OnThrowInput);
		else
			input.RemoveActionListener(LF_FoodThrow.ACTION, EActionTrigger.DOWN, LF_OnThrowInput);

		m_bLF_ThrowListening = on;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnThrowInput()
	{
		LF_FoodThrow.Ask();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		LF_ListenForThrow(false);
		super.OnDelete(owner);
	}
}
