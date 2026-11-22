[ComponentEditorProps(category: "EveronLife/Feature/Crime", description: "A container left short on the books: opens with a key, empties in one take, refills on a timer (#4195).")]
class LF_StashContainerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A short container in the world (crime B3, #4195). The pack sets the key prefab the tally clerk hands
//! out and the stock (cartons, bottles). The rules live in LF_StashContainerStock; this component owns
//! one stock, runs the refill timer on the server and replicates only "empty or not" so a client's
//! action row can say why the container will not open.
class LF_StashContainerComponent : ScriptComponent
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Key item that opens the container (the clerk hands it out)", "et")]
	protected ResourceName m_sKeyPrefab;

	[Attribute(desc: "What the container holds when full")]
	protected ref array<ref LF_StashContainerEntry> m_aStock;

	[Attribute("2700", UIWidgets.EditBox, "Seconds from a take until the container is full again", "60 86400 1")]
	protected float m_fRefillSeconds;

	[RplProp()]
	protected bool m_bEmpty;

	protected ref LF_StashContainerStock m_Stock;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		m_Stock = new LF_StashContainerStock();
		m_Stock.SetKeyPrefab(m_sKeyPrefab);
		m_Stock.SetRefillSeconds(m_fRefillSeconds);
		m_Stock.AddEntries(m_aStock);

		if (!Replication.IsServer())
			return;

		if (m_sKeyPrefab.IsEmpty())
			LF_Debug.Error(LF_StashContainerStock.FEATURE, "container has no key prefab: it can never open");

		if (m_Stock.CountItems() == 0)
			LF_Debug.Error(LF_StashContainerStock.FEATURE, "container has no stock configured");
	}

	//------------------------------------------------------------------------------------------------
	LF_StashContainerStock GetStock()
	{
		return m_Stock;
	}

	//------------------------------------------------------------------------------------------------
	//! Replicated: true from a take until the refill. Clients read this; the server reads the timer.
	bool IsEmpty()
	{
		return m_bEmpty;
	}

	//------------------------------------------------------------------------------------------------
	static float NowSec()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		return world.GetWorldTime() / 1000.0;
	}

	//------------------------------------------------------------------------------------------------
	//! Why the player cannot open the container right now, as both sides see it.
	LF_EStashTake Check(IEntity player)
	{
		if (!m_Stock)
			return LF_EStashTake.NO_STOCK;

		if (Replication.IsServer())
			return m_Stock.Check(player, NowSec());

		// A client has no timer: the replicated flag stands in for it, the key is in its own inventory.
		LF_EStashTake check = m_Stock.Check(player, 0);
		if (check == LF_EStashTake.OK && m_bEmpty)
			return LF_EStashTake.EMPTY;

		return check;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: move the stock to the player and schedule the refill.
	int Take(IEntity player)
	{
		if (!Replication.IsServer() || !m_Stock)
			return 0;

		int moved = m_Stock.Take(player, NowSec());
		if (moved <= 0)
			return 0;

		m_bEmpty = true;
		Replication.BumpMe();

		GetGame().GetCallqueue().Remove(OnRefill);
		GetGame().GetCallqueue().CallLater(OnRefill, m_Stock.GetRefillSeconds() * 1000, false);
		return moved;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnRefill()
	{
		if (!m_bEmpty)
			return;

		m_bEmpty = false;
		Replication.BumpMe();
		LF_Debug.Info(LF_StashContainerStock.FEATURE, "container refilled");
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(OnRefill);
		super.OnDelete(owner);
	}
}
