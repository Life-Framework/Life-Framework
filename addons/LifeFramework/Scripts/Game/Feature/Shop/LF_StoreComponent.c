//------------------------------------------------------------------------------------------------
//! Run state of one product line: the stock and the display copies standing on the shelf.
class LF_StoreLineState : Managed
{
	ResourceName m_Prefab;
	int m_iStock;
	int m_iMax;
	int m_iRestock;
	ref array<vector> m_aSlots = {};
	//! Yaw of each slot relative to the store, LF_StoreRules.NO_YAW for an unturned copy.
	ref array<float> m_aYaws = {};
	//! Copies per folded pile (LF_StoreLine.m_iStack); 0 or 1 is no piles.
	int m_iStack;
	//! The shelf entity at each slot, EntityID.INVALID for an empty slot. Same length as m_aSlots.
	ref array<EntityID> m_aShelf = {};

	//------------------------------------------------------------------------------------------------
	int Occupied()
	{
		int count;
		foreach (EntityID id : m_aShelf)
		{
			if (id != EntityID.INVALID)
				count++;
		}

		return count;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Shop", description: "Makes the shop a physical store: goods on the shelves, a till, a boundary, stock that restocks.")]
class LF_StoreComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A physical store (#202), beside the price-list LF_ShopComponent on the same entity. The stock of
//! each line is ONE number held here. The shelf shows min(stock, slots) real world items; a unit
//! leaves the stock exactly once, either because a player took the item off the shelf (this
//! component notices it and tags the item unpaid, LF_Stores) or because the price list sold it
//! (LF_ShopComponent.Buy reserves it first, and the shelf copy that is now surplus is deleted).
//! Every operation looks for taken items first, so the two paths can never both count one unit.
//!
//! The shelf refills only on the restock interval, from the back room, so a taken item stays gone
//! for a while where the player can see it. Server only; a client sees the replicated world items.
class LF_StoreComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Stable key, unique in the world. The saved stock is matched by it")]
	protected string m_sStoreKey;

	[Attribute("", UIWidgets.Object, "The goods on sale. Each line is stock, a restock rate and the shelf points")]
	protected ref array<ref LF_StoreLine> m_aLines;

	[Attribute("0 0 0", UIWidgets.Coords, "Centre of the store boundary, local to this entity")]
	protected vector m_vBoundaryCenter;

	[Attribute("6 0 4", UIWidgets.Coords, "Half size of the store boundary (x and z), along this entity's axes. Leaving it with unpaid goods is shoplifting")]
	protected vector m_vBoundaryHalf;

	[Attribute("120", UIWidgets.Auto, "Seconds between restocks. 0 never restocks")]
	protected int m_iRestockSeconds;

	[Attribute("300", UIWidgets.Auto, "Seconds a shopper whose unpaid goods came back after a restart or a logout has to come back and pay. After that, carrying them outside the store is shoplifting")]
	protected int m_iRestoredGraceSeconds;

	//! Live stores by key. Lives here so the component's own OnDelete removes it.
	protected static ref map<string, LF_StoreComponent> s_mLive;

	//! A restored world item within this many metres (flat) of a shelf point is the shelf's own copy.
	protected static const float STRAY_RADIUS = 0.15;
	//! ...and within this height of it: under the 0.36 m between two gondola boards (#1143).
	protected static const float STRAY_HEIGHT = 0.2;
	//! Seconds after a fill or a load when the shelf looks once more for copies the engine restored late.
	protected static const int LATE_RECONCILE_MS = 4000;
	//! The spawn trace runs from this far above a shelf point to SNAP_BELOW under it. Under the 0.36 m
	//! between two boards of a vanilla gondola (tools/buildings/pipeline/fixtures.json).
	static const float SNAP_ABOVE = 0.15;
	static const float SNAP_BELOW = 0.5;
	//! Loose items or shoppers the spawn trace looks past before it gives up.
	protected static const int SNAP_TRIES = 4;
	protected static ref array<EntityID> s_aStrays;
	protected static ResourceName s_StrayPrefab;
	protected static vector s_vStrayPoint;
	protected static float s_fStrayHeight;

	protected ref array<ref LF_StoreLineState> m_aState = {};
	protected float m_fLastRestockMs;
	protected bool m_bFilled;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		BuildState();
		LF_Stores.Register(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_mLive && s_mLive.Get(m_sStoreKey) == this)
			s_mLive.Remove(m_sStoreKey);

		if (Replication.IsServer())
		{
			LF_Stores.Unregister(this);
			RemoveAllShelfItems();
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	static map<string, LF_StoreComponent> Live()
	{
		if (!s_mLive)
			s_mLive = new map<string, LF_StoreComponent>();

		return s_mLive;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetLive()
	{
		s_mLive = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildState()
	{
		m_aState.Clear();
		if (!m_aLines)
			return;

		foreach (LF_StoreLine line : m_aLines)
		{
			if (!line || line.m_ItemPrefab.IsEmpty())
			{
				LF_Debug.Error(LF_Stores.FEATURE, string.Format("store %1 has a line with no item; skipped", m_sStoreKey));
				continue;
			}

			LF_StoreLineState state = new LF_StoreLineState();
			state.m_Prefab = line.m_ItemPrefab;
			state.m_iMax = Math.Max(line.m_iMaxStock, 0);
			state.m_iStock = LF_StoreRules.StartStock(line.m_iStartStock, state.m_iMax);
			state.m_iRestock = line.m_iRestockAmount;
			state.m_iStack = line.m_iStack;
			FillSlots(state, line.m_sSlots);
			m_aState.Insert(state);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void FillSlots(LF_StoreLineState state, string slots)
	{
		LF_StoreRules.ParseSlots(slots, state.m_aSlots);
		LF_StoreRules.ParseSlotYaws(slots, state.m_aYaws);
		state.m_aShelf.Clear();
		foreach (vector slot : state.m_aSlots)
		{
			state.m_aShelf.Insert(EntityID.INVALID);
		}
	}

	//------------------------------------------------------------------------------------------------
	string GetStoreKey()
	{
		return m_sStoreKey;
	}

	//------------------------------------------------------------------------------------------------
	int GetRestoredGraceSeconds()
	{
		return Math.Max(m_iRestoredGraceSeconds, 0);
	}

	//------------------------------------------------------------------------------------------------
	LF_ShopComponent GetShop()
	{
		return LF_Component<LF_ShopComponent>.Find(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! Test and setup seam: a store built in script has no attributes to fill.
	void Configure(string storeKey, vector boundaryCenter, vector boundaryHalf, int restockSeconds)
	{
		m_sStoreKey = storeKey;
		m_vBoundaryCenter = boundaryCenter;
		m_vBoundaryHalf = boundaryHalf;
		m_iRestockSeconds = restockSeconds;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_StoreLineState FindLine(ResourceName prefab)
	{
		foreach (LF_StoreLineState state : m_aState)
		{
			if (LF_Utils.SamePrefab(state.m_Prefab, prefab))
				return state;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the store stocks this prefab on a shelf line (a limited item).
	bool HasLine(ResourceName prefab)
	{
		return FindLine(prefab) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Units in stock (shelf and back room), or -1 when the store has no line for the prefab.
	int GetStock(ResourceName prefab)
	{
		LF_StoreLineState line = FindLine(prefab);
		if (!line)
			return -1;

		return line.m_iStock;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the entity is a display copy standing at a shelf slot of this store.
	bool IsShelfItem(IEntity item)
	{
		if (!item)
			return false;

		EntityID id = item.GetID();
		foreach (LF_StoreLineState line : m_aState)
		{
			if (line.m_aShelf.Find(id) != -1)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Display copies standing on the shelf now, -1 when the store has no line for the prefab.
	int GetShelfCount(ResourceName prefab)
	{
		LF_StoreLineState line = FindLine(prefab);
		if (!line)
			return -1;

		return line.Occupied();
	}

	//------------------------------------------------------------------------------------------------
	int GetLineCount()
	{
		return m_aState.Count();
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetLinePrefab(int index)
	{
		if (index < 0 || index >= m_aState.Count())
			return ResourceName.Empty;

		return m_aState[index].m_Prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The entity standing at a slot of a line, or null. For tests and the harness.
	IEntity GetShelfItem(ResourceName prefab, int slot)
	{
		LF_StoreLineState line = FindLine(prefab);
		if (!line || slot < 0 || slot >= line.m_aShelf.Count() || line.m_aShelf[slot] == EntityID.INVALID)
			return null;

		return GetGame().GetWorld().FindEntityByID(line.m_aShelf[slot]);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a world point is inside the store boundary (flat).
	bool Contains(vector worldPos)
	{
		vector local = GetOwner().CoordToLocal(worldPos) - m_vBoundaryCenter;
		return LF_StoreRules.InsideBox(local, m_vBoundaryHalf);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the first fill of the shelves, once the world has settled.
	void FirstFill()
	{
		if (m_bFilled)
			return;

		m_bFilled = true;
		Sync(true);
		GetGame().GetCallqueue().CallLater(LateReconcile, LATE_RECONCILE_MS, false);
		LF_Debug.Info(LF_Stores.FEATURE, string.Format("store %1 stocked: %2", m_sStoreKey, Describe()));
	}

	//------------------------------------------------------------------------------------------------
	//! One line of text per store: "apple 6/6 shelf 4, water ..." for logs and the harness.
	string Describe()
	{
		string text;
		foreach (LF_StoreLineState line : m_aState)
		{
			if (!text.IsEmpty())
				text += ", ";

			string name = line.m_Prefab;
			int slash = name.LastIndexOf("/");
			if (slash >= 0)
				name = name.Substring(slash + 1, name.Length() - slash - 1);

			text += string.Format("%1 %2/%3 shelf %4", name, line.m_iStock, line.m_iMax, line.Occupied());
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: finds shelf items a player took (in an inventory, or moved off the shelf) or that were
	//! deleted. Each one leaves the stock once; a taken one is tagged unpaid. Runs before every stock
	//! change so a pickup and a price-list sale can never count the same unit.
	//! \return Items found taken or gone.
	int DetectTaken()
	{
		int found;
		LF_ShopComponent shop = GetShop();
		foreach (LF_StoreLineState line : m_aState)
		{
			for (int i = 0; i < line.m_aShelf.Count(); i++)
			{
				if (line.m_aShelf[i] == EntityID.INVALID)
					continue;

				IEntity item = GetGame().GetWorld().FindEntityByID(line.m_aShelf[i]);
				if (!item)
				{
					line.m_aShelf[i] = EntityID.INVALID;
					line.m_iStock = Math.Max(line.m_iStock - 1, 0);
					found++;
					LF_Debug.Warn(LF_Stores.FEATURE, string.Format("store %1: a shelf %2 was deleted; stock now %3", m_sStoreKey, line.m_Prefab, line.m_iStock));
					continue;
				}

				IEntity root = LF_InventoryUtils.GetStorageHierachyRoot(item);
				bool moved = root != item || vector.Distance(item.GetOrigin(), SlotWorld(line, i)) > LF_StoreRules.TAKEN_DISTANCE;
				if (!moved)
					continue;

				line.m_aShelf[i] = EntityID.INVALID;
				line.m_iStock = Math.Max(line.m_iStock - 1, 0);
				found++;

				int price;
				if (shop)
					price = shop.GetBuyPrice(line.m_Prefab);

				LF_Stores.Tag(this, item, line.m_Prefab, price);
			}
		}

		if (found > 0)
			CompactStacks();

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the copy of a slot stands. A lone slot, or the bottom of a pile, rests on the surface
	//! under its point; a copy higher in a pile keeps its written height above the bottom one, so the
	//! whole pile stands on the board its bottom copy found.
	protected vector SpawnPoint(LF_StoreLineState line, int index)
	{
		int base = LF_StoreRules.StackBase(index, line.m_iStack);
		vector bottom = SnapToSurface(SlotWorld(line, base));
		return bottom + (SlotWorld(line, index) - SlotWorld(line, base)) + "0 0.02 0";
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnShelfCopy(LF_StoreLineState line, int index)
	{
		vector angles = "0 0 0";
		if (index < line.m_aYaws.Count() && line.m_aYaws[index] != LF_StoreRules.NO_YAW)
			angles = Vector(GetOwner().GetYawPitchRoll()[0] + line.m_aYaws[index], 0, 0);

		return LF_Utils.SpawnEntityPrefab(line.m_Prefab, SpawnPoint(line, index), angles);
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the gap a copy taken from the middle of a pile left: folded clothes do not float. Each
	//! copy above the gap is put back one place lower (spawned there, the old one deleted). The stock
	//! is untouched.
	protected void CompactStacks()
	{
		foreach (LF_StoreLineState line : m_aState)
		{
			if (line.m_iStack <= 1)
				continue;

			array<bool> occupied = {};
			foreach (EntityID id : line.m_aShelf)
			{
				occupied.Insert(id != EntityID.INVALID);
			}

			array<int> from = {};
			array<int> to = {};
			LF_StoreRules.StackMoves(occupied, line.m_iStack, from, to);
			for (int m = 0; m < from.Count(); m++)
			{
				IEntity old = GetGame().GetWorld().FindEntityByID(line.m_aShelf[from[m]]);
				IEntity moved = SpawnShelfCopy(line, to[m]);
				if (!moved)
				{
					LF_Debug.Error(LF_Stores.FEATURE, string.Format("store %1: a pile copy of %2 did not respawn lower; left where it was", m_sStoreKey, line.m_Prefab));
					break;
				}

				line.m_aShelf[to[m]] = moved.GetID();
				line.m_aShelf[from[m]] = EntityID.INVALID;
				if (old)
					SCR_EntityHelper.DeleteEntityAndChildren(old);
			}

			if (!from.IsEmpty())
				LF_Debug.Info(LF_Stores.FEATURE, string.Format("store %1: a %2 pile closed up, %3 copies moved down", m_sStoreKey, line.m_Prefab, from.Count()));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected vector SlotWorld(LF_StoreLineState line, int index)
	{
		return GetOwner().CoordToParent(line.m_aSlots[index]);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether a price-list sale of units can be filled. Counts shelf items taken by hand
	//! first. True for an item the store has no line for.
	bool CanSupply(ResourceName prefab, int units)
	{
		LF_StoreLineState line = FindLine(prefab);
		if (!line)
			return true;

		DetectTaken();
		return line.m_iStock >= units;
	}

	//------------------------------------------------------------------------------------------------
	protected vector SnapToSurface(vector point)
	{
		return SnapToSurfaceAt(point);
	}

	//------------------------------------------------------------------------------------------------
	//! A shelf point is a hint: a short trace straight down finds what the goods stand on (the counter,
	//! a gondola board, a cooler deck) so the item rests on it whatever height the point was written
	//! at. Any static surface counts: a building store's fixtures come from its furniture prefab, and
	//! prefab children are not runtime child entities of the building, so "the store or its children"
	//! cannot be named (probed on the supermarket, #1143). Loose items and characters are passed
	//! through, so a copy never stacks on another or on a shopper. The trace starts just above the
	//! point: on a gondola it finds the board under the point, not the one above it. Falls back to the
	//! point itself when nothing is under it.
	static vector SnapToSurfaceAt(vector point)
	{
		array<IEntity> skip = {};
		for (int attempt = 0; attempt < SNAP_TRIES; attempt++)
		{
			TraceParam trace = new TraceParam();
			trace.Start = point + Vector(0, SNAP_ABOVE, 0);
			trace.End = point - Vector(0, SNAP_BELOW, 0);
			trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
			trace.LayerMask = -1;
			if (!skip.IsEmpty())
				trace.ExcludeArray = skip;

			float fraction = GetGame().GetWorld().TraceMove(trace, null);
			if (fraction >= 1)
				return point;

			IEntity hit = trace.TraceEnt;
			if (hit && (ChimeraCharacter.Cast(hit) || hit.FindComponent(InventoryItemComponent)))
			{
				skip.Insert(hit);
				continue;
			}

			return trace.Start + (trace.End - trace.Start) * fraction;
		}

		return point;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sets aside units for a price-list sale. Looks for taken items first, then takes the
	//! units out of the stock and deletes the display copies that are now surplus.
	//! \return False when the store holds fewer than units of a line it stocks. True for an item it has
	//! no line for (the price list sells that without limit).
	bool Reserve(ResourceName prefab, int units)
	{
		LF_StoreLineState line = FindLine(prefab);
		if (!line)
			return true;

		DetectTaken();
		if (line.m_iStock < units)
		{
			LF_Debug.Info(LF_Stores.FEATURE, string.Format("store %1: %2 refused, stock %3 is below %4", m_sStoreKey, prefab, line.m_iStock, units));
			return false;
		}

		line.m_iStock -= units;
		Sync(false);
		LF_Debug.Info(LF_Stores.FEATURE, string.Format("store %1: price-list sale of %2 x%3, stock now %4", m_sStoreKey, prefab, units, line.m_iStock));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gives back units reserved for a sale that did not complete. The shelf copy that was
	//! deleted stays gone until the next restock.
	void Unreserve(ResourceName prefab, int units)
	{
		LF_StoreLineState line = FindLine(prefab);
		if (!line || units <= 0)
			return;

		line.m_iStock = Math.Min(line.m_iStock + units, line.m_iMax);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: restocks every line by its amount and refills the shelves from the back room.
	void RestockNow()
	{
		DetectTaken();
		foreach (LF_StoreLineState line : m_aState)
			line.m_iStock = LF_StoreRules.Restocked(line.m_iStock, line.m_iMax, line.m_iRestock);

		Sync(true);
		LF_Debug.Info(LF_Stores.FEATURE, string.Format("store %1 restocked: %2", m_sStoreKey, Describe()));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the restock clock, called from the store tick.
	void RestockIfDue(float nowMs)
	{
		if (m_fLastRestockMs <= 0)
		{
			m_fLastRestockMs = nowMs;
			return;
		}

		if (!LF_StoreRules.RestockDue(nowMs, m_fLastRestockMs, m_iRestockSeconds))
			return;

		m_fLastRestockMs = nowMs;
		RestockNow();
	}

	//------------------------------------------------------------------------------------------------
	//! Makes the shelf match the stock: surplus display copies are deleted, and with allowSpawn the
	//! empty slots are filled from the stock.
	protected void Sync(bool allowSpawn)
	{
		if (allowSpawn)
			AdoptStrays();

		CompactStacks();

		foreach (LF_StoreLineState line : m_aState)
		{
			int want = LF_StoreRules.ShelfCount(line.m_iStock, line.m_aSlots.Count());
			int have = line.Occupied();
			for (int i = line.m_aShelf.Count() - 1; i >= 0 && have > want; i--)
			{
				if (line.m_aShelf[i] == EntityID.INVALID)
					continue;

				IEntity surplus = GetGame().GetWorld().FindEntityByID(line.m_aShelf[i]);
				line.m_aShelf[i] = EntityID.INVALID;
				have--;
				if (surplus)
					SCR_EntityHelper.DeleteEntityAndChildren(surplus);
			}

			if (!allowSpawn)
				continue;

			for (int s = 0; s < line.m_aShelf.Count() && have < want; s++)
			{
				if (line.m_aShelf[s] != EntityID.INVALID)
					continue;

				IEntity item = SpawnShelfCopy(line, s);
				if (!item)
				{
					LF_Debug.Error(LF_Stores.FEATURE, string.Format("store %1: shelf item did not spawn: %2", m_sStoreKey, line.m_Prefab));
					break;
				}

				line.m_aShelf[s] = item.GetID();
				have++;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A shelf item is a plain world item with a quantity component, so the engine's own persistence
	//! saves it and brings it back after a restart, at the same spot, beside the copies this store
	//! spawns from its saved stock. Any loose copy of a line's item sitting on a shelf point is therefore
	//! the shelf's own: it is adopted into an empty slot, or deleted when the slot already has one.
	protected void AdoptStrays()
	{
		foreach (LF_StoreLineState line : m_aState)
		{
			for (int i = 0; i < line.m_aShelf.Count(); i++)
			{
				vector point = SlotWorld(line, i);
				s_fStrayHeight = STRAY_HEIGHT;
				if (line.m_iStack > 1)
				{
					// The copies of a pile are a few centimetres apart: each slot owns only the copy at
					// its own height, or the bottom one would adopt the whole pile and delete the rest.
					point = SpawnPoint(line, i);
					s_fStrayHeight = Math.Min(STRAY_HEIGHT, StackStep(line, i) * 0.5);
				}

				if (!s_aStrays)
					s_aStrays = new array<EntityID>();

				s_aStrays.Clear();
				s_StrayPrefab = line.m_Prefab;
				s_vStrayPoint = point;
				GetGame().GetWorld().QueryEntitiesBySphere(point, 0.6, CollectStray, null, EQueryEntitiesFlags.ALL);
				foreach (EntityID id : s_aStrays)
				{
					IEntity stray = GetGame().GetWorld().FindEntityByID(id);
					if (!stray)
						continue;

					if (line.m_aShelf[i] == EntityID.INVALID && LF_StoreRules.ShelfCount(line.m_iStock, line.m_aSlots.Count()) > line.Occupied())
					{
						line.m_aShelf[i] = id;
						continue;
					}

					LF_Debug.Info(LF_Stores.FEATURE, string.Format("store %1: a restored copy of %2 sat on a shelf point already filled; deleted", m_sStoreKey, line.m_Prefab));
					SCR_EntityHelper.DeleteEntityAndChildren(stray);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The height between a pile slot and its nearest neighbour in the same pile; twice
	//! STRAY_HEIGHT for a pile of one.
	protected float StackStep(LF_StoreLineState line, int index)
	{
		float step = STRAY_HEIGHT * 2;
		int base = LF_StoreRules.StackBase(index, line.m_iStack);
		int end = Math.Min(base + line.m_iStack, line.m_aSlots.Count());
		for (int k = base; k < end; k++)
		{
			if (k == index)
				continue;

			float rise = Math.AbsFloat(line.m_aSlots[k][1] - line.m_aSlots[index][1]);
			if (rise > 0.001)
				step = Math.Min(step, rise);
		}

		return step;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectStray(IEntity entity)
	{
		if (!entity || !LF_Utils.SamePrefab(LF_Utils.GetPrefabName(entity), s_StrayPrefab))
			return true;

		if (LF_InventoryUtils.GetStorageHierachyRoot(entity) != entity || LF_Stores.HoldsApart(entity))
			return true;

		// Flat distance plus a height limit: on a gondola the slot on the board above is a few
		// centimetres away flat, and its copy must not be taken for this slot's (#1143).
		vector flat = entity.GetOrigin() - s_vStrayPoint;
		float rise = Math.AbsFloat(flat[1]);
		flat[1] = 0;
		if (flat.Length() <= STRAY_RADIUS && rise <= s_fStrayHeight)
			s_aStrays.Insert(entity.GetID());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one more look a few seconds after a fill or a load, for copies the engine restored late.
	void LateReconcile()
	{
		AdoptStrays();
		Sync(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes the display copies still standing on the shelf (the store is going away).
	protected void RemoveAllShelfItems()
	{
		foreach (LF_StoreLineState line : m_aState)
		{
			for (int i = 0; i < line.m_aShelf.Count(); i++)
			{
				if (line.m_aShelf[i] == EntityID.INVALID)
					continue;

				IEntity item = GetGame().GetWorld().FindEntityByID(line.m_aShelf[i]);
				line.m_aShelf[i] = EntityID.INVALID;
				if (item && LF_InventoryUtils.GetStorageHierachyRoot(item) == item)
					SCR_EntityHelper.DeleteEntityAndChildren(item);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: puts every display copy back the way a fresh fill would, with the given stock.
	void ForceStock(ResourceName prefab, int stock)
	{
		LF_StoreLineState line = FindLine(prefab);
		if (!line)
			return;

		line.m_iStock = LF_StoreRules.ClampStock(stock, line.m_iMax);
		Sync(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: adds a full line at run time (a store built in script has no attributes) and fills it.
	void AddTestLine(ResourceName prefab, int maxStock, int stock, string slots, int stack)
	{
		if (prefab.IsEmpty() || FindLine(prefab))
			return;

		LF_StoreLineState state = new LF_StoreLineState();
		state.m_Prefab = prefab;
		state.m_iMax = Math.Max(maxStock, 0);
		state.m_iStock = LF_StoreRules.ClampStock(stock, state.m_iMax);
		state.m_iStack = stack;
		FillSlots(state, slots);
		m_aState.Insert(state);
		Sync(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: drops a line added by AddTestLine and deletes its display copies.
	void RemoveTestLine(ResourceName prefab)
	{
		LF_StoreLineState line = FindLine(prefab);
		if (!line)
			return;

		foreach (EntityID id : line.m_aShelf)
		{
			IEntity item = GetGame().GetWorld().FindEntityByID(id);
			if (item && LF_InventoryUtils.GetStorageHierachyRoot(item) == item)
				SCR_EntityHelper.DeleteEntityAndChildren(item);
		}

		m_aState.RemoveItem(line);
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: the store forgets which entity stands at each slot of a line and leaves the copies
	//! in the world, the way a restart finds the copies the engine restored.
	void ForgetShelf(ResourceName prefab)
	{
		LF_StoreLineState line = FindLine(prefab);
		if (!line)
			return;

		for (int i = 0; i < line.m_aShelf.Count(); i++)
		{
			line.m_aShelf[i] = EntityID.INVALID;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: this store's stock, one row per line.
	void Export(notnull array<ref LF_StoreStockRow> rows)
	{
		if (m_sStoreKey.IsEmpty())
			return;

		DetectTaken();
		foreach (LF_StoreLineState line : m_aState)
		{
			LF_StoreStockRow row = new LF_StoreStockRow();
			row.m_sStoreKey = m_sStoreKey;
			row.m_sPrefab = line.m_Prefab;
			row.m_iStock = line.m_iStock;
			rows.Insert(row);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: puts a saved stock back and makes the shelf match it. A row for a line the
	//! store no longer has is dropped. Idempotent: the shelf converges, it never doubles.
	//! \return True when the row matched a line.
	bool ImportRow(LF_StoreStockRow row)
	{
		if (!row)
			return false;

		LF_StoreLineState line = FindLine(row.m_sPrefab);
		if (!line)
		{
			LF_Debug.Warn(LF_Stores.FEATURE, string.Format("store %1: saved stock of %2 dropped, the store no longer stocks it", m_sStoreKey, row.m_sPrefab));
			return false;
		}

		DetectTaken();
		line.m_iStock = LF_StoreRules.ClampStock(row.m_iStock, line.m_iMax);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: call after the rows of a load are imported.
	void AfterImport()
	{
		if (!m_bFilled)
			return;

		Sync(true);
		GetGame().GetCallqueue().CallLater(LateReconcile, LATE_RECONCILE_MS, false);
		LF_Debug.Info(LF_Stores.FEATURE, string.Format("store %1 restored: %2", m_sStoreKey, Describe()));
	}
}
