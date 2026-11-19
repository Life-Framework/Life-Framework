//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Carry", description: "A load carried on the shoulder: caps the carrier's speed while it is in the hand-carry slot")]
class LF_ShoulderCarryComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The carry profile of a load (#3263): a log now, an ore sack or a crate later. While the item sits in a
//! carrier's hand-carry storage the carrier's dynamic speed is capped (`SetDynamicSpeed`; softwood log 0.7,
//! hardwood log 0.5, ore sack 0.75, crate 0.6) and a downed carrier drops it. The cap sits under jog speed,
//! so the carrier cannot sprint. The slot change fires on every machine, so the cap needs no RPC; the
//! character's own movement is simulated by its owner, who therefore holds the cap that matters.
class LF_ShoulderCarryComponent : ScriptComponent
{
	[Attribute("log", UIWidgets.EditBox, "What the load is (log, hardwood, ore, crate): for logs and the truck bed")]
	protected string m_sKind;

	[Attribute("0.7", UIWidgets.Slider, "Speed cap while carried, a fraction of jog speed", "0.1 1 0.05")]
	protected float m_fSpeedCap;

	protected EntityID m_Carrier = EntityID.INVALID;
	protected float m_fPrevSpeed = 1;

	//------------------------------------------------------------------------------------------------
	string GetKind()
	{
		return m_sKind;
	}

	//------------------------------------------------------------------------------------------------
	float GetSpeedCap()
	{
		return LF_ShoulderCarry.ClampCap(m_fSpeedCap);
	}

	//------------------------------------------------------------------------------------------------
	//! The carrier whose speed this load holds down, or null.
	IEntity GetCarrier()
	{
		if (m_Carrier == EntityID.INVALID)
			return null;

		return GetGame().GetWorld().FindEntityByID(m_Carrier);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		InventoryItemComponent item = LF_Component<InventoryItemComponent>.Find(owner);
		if (!item)
		{
			LF_Debug.Error(LF_ShoulderCarry.FEATURE, "shoulder-carry load has no InventoryItemComponent; no speed cap");
			return;
		}

		item.m_OnParentSlotChangedInvoker.Insert(OnSlotChanged);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		// A load deleted while carried (a truck bed takes it) must not leave its carrier slow.
		Lift();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! The character whose hand-carry storage holds `slot`, or null.
	protected static IEntity HandCarrier(InventoryStorageSlot slot)
	{
		if (!slot)
			return null;

		LF_HandInventoryStorageComponent hands = LF_HandInventoryStorageComponent.Cast(slot.GetStorage());
		if (!hands)
			return null;

		return hands.GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSlotChanged(InventoryStorageSlot oldSlot, InventoryStorageSlot newSlot)
	{
		IEntity carrier = HandCarrier(newSlot);
		if (!carrier)
		{
			Lift();
			return;
		}

		if (m_Carrier == carrier.GetID())
			return;

		Lift();
		Apply(carrier);
	}

	//------------------------------------------------------------------------------------------------
	protected void Apply(notnull IEntity carrier)
	{
		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(carrier);
		if (!controller)
		{
			LF_Debug.Error(LF_ShoulderCarry.FEATURE, "carrier has no CharacterControllerComponent; no speed cap");
			return;
		}

		m_Carrier = carrier.GetID();
		m_fPrevSpeed = controller.GetDynamicSpeed();
		controller.SetDynamicSpeed(GetSpeedCap());
		controller.SetShouldApplyDynamicSpeedOverride(true);
		SCR_CharacterControllerComponent scripted = SCR_CharacterControllerComponent.Cast(controller);
		if (scripted)
			scripted.m_OnLifeStateChanged.Insert(OnLifeStateChanged);
		LF_Debug.Info(LF_ShoulderCarry.FEATURE, string.Format("%1 on the shoulder: speed capped to %2", m_sKind, GetSpeedCap()));
	}

	//------------------------------------------------------------------------------------------------
	protected void Lift()
	{
		IEntity carrier = GetCarrier();
		m_Carrier = EntityID.INVALID;
		if (!carrier)
			return;

		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(carrier);
		if (!controller)
			return;

		SCR_CharacterControllerComponent scripted = SCR_CharacterControllerComponent.Cast(controller);
		if (scripted)
			scripted.m_OnLifeStateChanged.Remove(OnLifeStateChanged);
		controller.SetDynamicSpeed(m_fPrevSpeed);
		controller.SetShouldApplyDynamicSpeedOverride(false);
		LF_Debug.Info(LF_ShoulderCarry.FEATURE, string.Format("%1 off the shoulder: speed cap lifted", m_sKind));
	}

	//------------------------------------------------------------------------------------------------
	//! A downed, unconscious or dead carrier lets the load go.
	protected void OnLifeStateChanged(ECharacterLifeState previousLifeState, ECharacterLifeState newLifeState)
	{
		if (newLifeState == ECharacterLifeState.ALIVE)
			return;

		IEntity carrier = GetCarrier();
		if (!carrier)
			return;

		LF_Debug.Info(LF_ShoulderCarry.FEATURE, "carrier went down: the load is dropped");
		LF_ShoulderCarry.Drop(carrier);
		// The drop's slot change lifts the cap; this covers a drop the engine refused.
		Lift();
	}
}
