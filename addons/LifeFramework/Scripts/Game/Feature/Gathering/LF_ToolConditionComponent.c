//------------------------------------------------------------------------------------------------
//! The condition of one gathering tool, carried on the tool itself (#4821). Before, condition lived
//! in a per-player store keyed by tool kind, so a new fine pickaxe started at the old worn one's
//! condition and every tier wore at the standard rate. Now each pickaxe, axe and rod holds its own
//! tier (a prefab attribute: the pack's worn, standard and fine prefabs set it) and its own condition
//! (server state, replicated so the holder's bench prompt can read it).
//!
//! The condition travels with the item: through inventory moves, drops and trades it is the same
//! entity. It is saved two ways: on the ground and in world storage by LF_ToolConditionComponentSerializer
//! (bound in Configs/Systems/Persistence/LifeFramework.conf), and in a carried or stored snapshot by
//! the "toolcondition" save section (LF_ToolConditionSavePayload), parallel to the snapshot's items.
[ComponentEditorProps(category: "LifeFramework/Feature/Gathering", description: "Tier and condition of one gathering tool (#4821).")]
class LF_ToolConditionComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ToolConditionComponent : ScriptComponent
{
	static const string FEATURE = "ToolWear";

	[Attribute("0", UIWidgets.ComboBox, "Tool kind: which trade wears it and which bench action works on it", "", ParamEnumArray.FromEnum(LF_EToolBenchKind))]
	protected LF_EToolBenchKind m_eKind;

	[Attribute("1", UIWidgets.EditBox, "Tier: 0 worn, 1 standard, 2 fine. Sets the wear per use (Configs/Gathering/LF_ToolTiers.conf)", "0 2 1")]
	protected int m_iTier;

	[RplProp()]
	protected float m_fCondition = LF_ToolCondition.FULL;

	//! Test hook: the tool a test character "holds" (EntityIDs, never the entity).
	protected static EntityID s_HeldForTestsBy = EntityID.INVALID;
	protected static EntityID s_HeldForTestsTool = EntityID.INVALID;

	//------------------------------------------------------------------------------------------------
	static LF_ToolConditionComponent Of(IEntity item)
	{
		if (!item)
			return null;

		return LF_ToolConditionComponent.Cast(item.FindComponent(LF_ToolConditionComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! The kind id ("pickaxe", "axe", "rod") of a bench kind.
	static string KindId(LF_EToolBenchKind kind)
	{
		if (kind == LF_EToolBenchKind.AXE)
			return LF_ToolBench.KIND_AXE;
		if (kind == LF_EToolBenchKind.ROD)
			return LF_ToolBench.KIND_ROD;
		return LF_ToolBench.KIND_PICKAXE;
	}

	//------------------------------------------------------------------------------------------------
	string GetKind()
	{
		return KindId(m_eKind);
	}

	//------------------------------------------------------------------------------------------------
	int GetTier()
	{
		return LF_ToolCondition.ClampTier(m_iTier);
	}

	//------------------------------------------------------------------------------------------------
	float GetCondition()
	{
		return m_fCondition;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sets the condition, clamped to 0..FULL, and replicates it. A load and the bench use it.
	void SetCondition(float condition)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Warn(FEATURE, "tool condition set off the server, ignored");
			return;
		}

		float clamped = condition;
		if (clamped < 0)
			clamped = 0;
		if (clamped > LF_ToolCondition.FULL)
			clamped = LF_ToolCondition.FULL;
		if (clamped == m_fCondition)
			return;

		m_fCondition = clamped;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one use that costs `scale` times this tier's rate (1 is the full rate; a trade perk
	//! lowers it, #4408). Never below 0. \return The warning key for the holder, or "".
	string Wear(float scale)
	{
		float before = m_fCondition;
		float left = before - LF_ToolCondition.WearPerUse(GetTier()) * scale;
		if (left < 0)
			left = 0;

		SetCondition(left);
		return LF_ToolWear.WarnKey(before, m_fCondition);
	}

	//------------------------------------------------------------------------------------------------
	//! The tool of this kind the character holds in the hands (right hand, left-hand gadget, or a hand
	//! tool such as the rod), or null. A tool in a pocket or the pack does not count.
	static LF_ToolConditionComponent Held(IEntity character, string kind)
	{
		if (!character || kind.IsEmpty())
			return null;

		if (s_HeldForTestsBy != EntityID.INVALID && character.GetID() == s_HeldForTestsBy)
			return Matching(GetGame().GetWorld().FindEntityByID(s_HeldForTestsTool), kind);

		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(character);
		if (controller)
		{
			LF_ToolConditionComponent right = Matching(controller.GetRightHandItem(), kind);
			if (right)
				return right;

			LF_ToolConditionComponent left = Matching(controller.GetAttachedGadgetAtLeftHandSlot(), kind);
			if (left)
				return left;
		}

		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.Cast(character.FindComponent(SCR_GadgetManagerComponent));
		if (gadgets)
		{
			LF_ToolConditionComponent gadget = Matching(gadgets.GetHeldGadget(), kind);
			if (gadget)
				return gadget;
		}

		if (kind == LF_ToolBench.KIND_ROD)
			return Matching(LF_HandTool.HeldTool(character, LF_ToolBench.KIND_ROD), kind);

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The tool of this kind that did the work: the one in the hands, else the first one carried (a
	//! gather node that accepts a tool from the inventory). Null when the character has none.
	static LF_ToolConditionComponent InUse(IEntity character, string kind)
	{
		LF_ToolConditionComponent held = Held(character, kind);
		if (held)
			return held;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!inventory)
			return null;

		array<IEntity> items = {};
		inventory.GetItems(items);
		foreach (IEntity item : items)
		{
			LF_ToolConditionComponent tool = Matching(item, kind);
			if (tool)
				return tool;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ToolConditionComponent Matching(IEntity item, string kind)
	{
		LF_ToolConditionComponent tool = Of(item);
		if (tool && tool.GetKind() == kind)
			return tool;

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Test hook: `character` counts as holding `tool` (null clears it).
	static void SetHeldForTests(IEntity character, IEntity tool)
	{
		if (!character || !tool)
		{
			s_HeldForTestsBy = EntityID.INVALID;
			s_HeldForTestsTool = EntityID.INVALID;
			return;
		}

		s_HeldForTestsBy = character.GetID();
		s_HeldForTestsTool = tool.GetID();
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_HeldForTestsBy = EntityID.INVALID;
		s_HeldForTestsTool = EntityID.INVALID;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the test hook (LF_StaticReset finds it by reflection).
class LF_ToolConditionComponent_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ToolConditionComponent";
	}

	override void Apply()
	{
		LF_ToolConditionComponent.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Saves a tool's condition with the item when the item is saved on its own (on the ground, in world
//! storage). Bound in Configs/Systems/Persistence/LifeFramework.conf. Version first, then the condition
//! only when the tool is not at full.
class LF_ToolConditionComponentSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	override static typename GetTargetType()
	{
		return LF_ToolConditionComponent;
	}

	//------------------------------------------------------------------------------------------------
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		LF_ToolConditionComponent tool = LF_ToolConditionComponent.Cast(component);
		if (!tool)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);
		if (tool.GetCondition() >= LF_ToolCondition.FULL)
			return ESerializeResult.OK;

		float condition = tool.GetCondition();
		context.Write(condition);
		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		LF_ToolConditionComponent tool = LF_ToolConditionComponent.Cast(component);
		if (!tool)
			return false;

		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		float condition;
		if (!context.Read(condition))
			return true;

		tool.SetCondition(condition);
		return true;
	}
}
