[ComponentEditorProps(category: "LifeFramework/Mining", description: "A small warm glint on an item while it lies loose, so a dropped piece is seen in the grass")]
class LF_LooseGlintComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A dropped ore piece is a small dark stone in tall grass (first-2h review B8: "the nuggets drop into
//! the grass" and the miner could not find them). While the item lies loose, every rendering peer shows a
//! small warm light on it for a while; it goes out when the item is picked up or the time is up, and comes
//! back for the same time when the item is dropped again. A dedicated server shows nothing.
class LF_LooseGlintComponent : ScriptComponent
{
	[Attribute("1.1", UIWidgets.Auto, "Glint radius (metres)", category: "Glint")]
	protected float m_fRadius;

	[Attribute("1 0.55 0.25", UIWidgets.EditBox, "Glint colour (red green blue, 0 to 1)", category: "Glint")]
	protected vector m_vColor;

	[Attribute("3", UIWidgets.Auto, "Glint brightness (LV)", category: "Glint")]
	protected float m_fLV;

	[Attribute("90", UIWidgets.Auto, "Seconds a loose item glints before the light goes out", category: "Glint")]
	protected float m_fSeconds;

	protected LightEntity m_Light;
	protected InventoryItemComponent m_Item;

	//------------------------------------------------------------------------------------------------
	//! Pure: an item glints while it is loose (in no inventory slot) and the rendering peer can show it.
	static bool ShouldGlint(bool inSlot, bool renders)
	{
		return renders && !inSlot;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (System.IsConsoleApp())
			return;

		m_Item = InventoryItemComponent.Cast(owner.FindComponent(InventoryItemComponent));
		if (m_Item)
			m_Item.m_OnParentSlotChangedInvoker.Insert(OnSlotChanged);

		// The slot is known once the entity is fully made (an item spawned into a bag has one by then).
		GetGame().GetCallqueue().CallLater(Refresh, 100, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSlotChanged(InventoryStorageSlot oldSlot, InventoryStorageSlot newSlot)
	{
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	protected void Refresh()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		bool inSlot = m_Item && m_Item.GetParentSlot() != null;
		bool renders = !System.IsConsoleApp() && RplSession.Mode() != RplMode.Dedicated;
		if (!ShouldGlint(inSlot, renders))
		{
			LightOff();
			return;
		}

		if (m_Light)
			return;

		m_Light = LightEntity.CreateLight(LightType.POINT, 0, m_fRadius, Color.FromVector(m_vColor), m_fLV, "0 0.12 0");
		if (!m_Light)
		{
			LF_Debug.Error("Mining", "loose glint light was not created on " + LF_Utils.GetPrefabName(owner));
			return;
		}

		m_Light.SetLensFlareType(LightLensFlareType.Disabled);
		owner.AddChild(m_Light, -1);
		GetGame().GetCallqueue().Remove(LightOff);
		GetGame().GetCallqueue().CallLater(LightOff, m_fSeconds * 1000, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void LightOff()
	{
		if (m_Light)
			delete m_Light;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		ScriptCallQueue queue;
		if (GetGame())
			queue = GetGame().GetCallqueue();

		if (queue)
		{
			queue.Remove(Refresh);
			queue.Remove(LightOff);
		}

		if (m_Item)
			m_Item.m_OnParentSlotChangedInvoker.Remove(OnSlotChanged);

		LightOff();
		super.OnDelete(owner);
	}
}
