//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Carry", description: "A vehicle's cargo bed: carried loads (logs now, ore sacks and crates later) are loaded at the tailgate, drawn as a stack and unloaded onto a shoulder")]
class LF_CargoBedComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The cargo bed of a vehicle (#3265). The server owns a stack of units (one kind index per unit, bottom
//! first; a hardwood log is two units). Load takes the carried item and counts it in, unload spawns the top
//! item onto the unloader's shoulder, a full bed refuses. The stack replicates as one short string through
//! an RPC per change and RplSave/RplLoad for a late joiner. Every machine draws it as local, physics-free
//! copies on bed slots (the LF_FarmPlotComponent visual pattern); nothing persists here (#3266 saves it).
class LF_CargoBedComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.EditBox, "Capacity in units (car 0, pickup or van 6, truck 12)")]
	protected int m_iCapacity;

	[Attribute("0 1.2 -3", UIWidgets.EditBox, "Tailgate point, local: where the load and unload actions are offered")]
	protected vector m_vTailgate;

	[Attribute("0 1.3 -1", UIWidgets.EditBox, "Centre of the bed floor, local: the stack is drawn around it")]
	protected vector m_vBedCentre;

	[Attribute("2", UIWidgets.EditBox, "Width of the loading area across the vehicle (metres): the logs lie along the bed, side by side across it")]
	protected float m_fBedWidth;

	[Attribute("4", UIWidgets.EditBox, "Units side by side across the bed in one layer")]
	protected int m_iPerLayer;

	[Attribute("0.25", UIWidgets.EditBox, "Height of one layer of the stack (metres)")]
	protected float m_fLayerHeight;

	//! One kind index per unit, bottom first. The server writes it; every machine holds a copy.
	protected ref array<int> m_aStack = new array<int>();

	//! The local copies drawn on the bed (local entities, no physics, no replication).
	protected ref array<EntityID> m_aVisualIds = new array<EntityID>();

	protected string m_sDrawn;

	//------------------------------------------------------------------------------------------------
	int GetCapacity()
	{
		return m_iCapacity;
	}

	//------------------------------------------------------------------------------------------------
	int GetUnits()
	{
		return m_aStack.Count();
	}

	//------------------------------------------------------------------------------------------------
	bool IsEmpty()
	{
		return m_aStack.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Units of one kind on the bed (a hardwood log counts 2).
	int CountOf(int kindIndex)
	{
		int count;
		foreach (int kind : m_aStack)
		{
			if (kind == kindIndex)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! The kind index on top, or -1 when empty.
	int TopKind()
	{
		if (m_aStack.IsEmpty())
			return -1;

		return m_aStack[m_aStack.Count() - 1];
	}

	//------------------------------------------------------------------------------------------------
	//! World position of the tailgate point.
	vector GetTailgateWorld()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return vector.Zero;

		return owner.CoordToParent(m_vTailgate);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		ClearVisuals(owner.GetWorld());
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: loads whatever `user` carries on the shoulder. Returns the refusal, NONE when it went in.
	LF_ECargoRefusal ServerLoad(IEntity user)
	{
		if (!Replication.IsServer())
			return LF_ECargoRefusal.NONE;

		IEntity item = LF_ShoulderCarry.Carried(user);
		if (!item)
			return Refuse(LF_ECargoRefusal.NO_LOAD);

		LF_ShoulderCarryComponent profile = LF_Component<LF_ShoulderCarryComponent>.Find(item);
		int kindIndex = LF_CargoBed.KindIndex(profile.GetKind());
		LF_ECargoRefusal refusal = DecideFor(user, kindIndex);
		if (refusal != LF_ECargoRefusal.NONE)
			return Refuse(refusal);

		// The item goes first: its own OnDelete lifts the carrier's speed cap.
		SCR_EntityHelper.DeleteEntityAndChildren(item);
		Push(kindIndex);
		LF_Debug.Info(LF_CargoBed.FEATURE, string.Format("loaded %1 (%2/%3 units)", profile.GetKind(), GetUnits(), m_iCapacity));
		return LF_ECargoRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: puts the top item on `user`'s shoulder. Returns the refusal, NONE when it went on.
	LF_ECargoRefusal ServerUnload(IEntity user)
	{
		if (!Replication.IsServer())
			return LF_ECargoRefusal.NONE;

		if (m_aStack.IsEmpty())
			return Refuse(LF_ECargoRefusal.EMPTY);

		if (IsLockedFor(user))
			return Refuse(LF_ECargoRefusal.LOCKED);

		if (user && LF_ShoulderCarry.Carried(user))
			return Refuse(LF_ECargoRefusal.NO_HANDS);

		int kindIndex = TopKind();
		array<ref LF_CargoKind> kinds = LF_CargoBed.Kinds();
		LF_CargoKind row = kinds[kindIndex];
		if (row.m_ItemPrefab.IsEmpty())
			return Refuse(LF_ECargoRefusal.NO_ITEM_PREFAB);

		vector at = GetTailgateWorld();
		if (user)
			at = user.GetOrigin();

		IEntity item = LF_Utils.SpawnEntityPrefab(row.m_ItemPrefab, at + "0 0.5 0");
		if (!item)
			return Refuse(LF_ECargoRefusal.NO_ITEM_PREFAB);

		if (user && !LF_ShoulderCarry.Hoist(user, item))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return Refuse(LF_ECargoRefusal.NO_HANDS);
		}

		Pop(row.m_iUnits);
		LF_Debug.Info(LF_CargoBed.FEATURE, string.Format("unloaded %1 (%2/%3 units)", row.m_sKind, GetUnits(), m_iCapacity));
		if (!user)
			SCR_EntityHelper.DeleteEntityAndChildren(item);

		return LF_ECargoRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: counts a load of `kind` in without an item (the harness, the tests). Same capacity rule.
	LF_ECargoRefusal ServerLoadKind(string kind)
	{
		if (!Replication.IsServer())
			return LF_ECargoRefusal.NONE;

		int kindIndex = LF_CargoBed.KindIndex(kind);
		LF_ECargoRefusal refusal = DecideFor(null, kindIndex);
		if (refusal != LF_ECargoRefusal.NONE)
			return Refuse(refusal);

		Push(kindIndex);
		return LF_ECargoRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: replaces the stack with a saved one (#3266) and tells every machine. Same text twice, same bed.
	void ServerRestore(string packed)
	{
		if (!Replication.IsServer())
			return;

		LoadText(packed);
		Sync();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a load of this kind index would go in now (user may be null: no reach check).
	LF_ECargoRefusal DecideFor(IEntity user, int kindIndex)
	{
		int units = LF_CargoBed.UnitsOf(kindIndex);
		LF_ECargoRefusal refusal = LF_CargoBed.DecideLoad(m_iCapacity, GetUnits(), units, kindIndex >= 0);
		if (refusal != LF_ECargoRefusal.NONE)
			return refusal;

		if (user && vector.Distance(user.GetOrigin(), GetTailgateWorld()) > LF_CargoBed.REACH)
			return LF_ECargoRefusal.TOO_FAR;

		if (IsLockedFor(user))
			return LF_ECargoRefusal.LOCKED;

		return LF_ECargoRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! True when a player (`user` set) meets a locked vehicle: the bed follows the trunk's lock, so nobody
	//! empties or fills a locked truck. A null user is a script call (harness, tests) and is not gated.
	bool IsLockedFor(IEntity user)
	{
		if (!user)
			return false;

		LF_VehicleStorageAccessControl access = LF_Component<LF_VehicleStorageAccessControl>.Find(GetOwner());
		return access && access.IsStorageLocked(user);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ECargoRefusal Refuse(LF_ECargoRefusal refusal)
	{
		LF_Debug.Warn(LF_CargoBed.FEATURE, "refused: " + LF_CargoBed.RefusalText(refusal));
		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	protected void Push(int kindIndex)
	{
		int units = LF_CargoBed.UnitsOf(kindIndex);
		for (int i = 0; i < units; i++)
			m_aStack.Insert(kindIndex);

		Sync();
	}

	//------------------------------------------------------------------------------------------------
	protected void Pop(int units)
	{
		for (int i = 0; i < units; i++)
		{
			if (!m_aStack.IsEmpty())
				m_aStack.Remove(m_aStack.Count() - 1);
		}

		Sync();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tells every machine the new stack and redraws here (a Broadcast never runs on the server).
	protected void Sync()
	{
		string packed = LF_CargoBed.Pack(m_aStack);
		Rpc(RpcDo_SetStack, packed);
		RpcDo_SetStack(packed);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetStack(string packed)
	{
		LoadText(packed);
	}

	//------------------------------------------------------------------------------------------------
	override bool RplSave(ScriptBitWriter writer)
	{
		writer.WriteString(SavedText());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		string packed;
		if (!reader.ReadString(packed))
			return false;

		LoadText(packed);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The stack as RplSave writes it (and a save would): one short string.
	string SavedText()
	{
		return LF_CargoBed.Pack(m_aStack);
	}

	//------------------------------------------------------------------------------------------------
	//! The inverse, as RplLoad reads it: replaces the stack, keeps it inside the capacity, redraws.
	void LoadText(string packed)
	{
		LF_CargoBed.Unpack(packed, m_aStack, MaxUnits());
		Redraw();
	}

	//------------------------------------------------------------------------------------------------
	protected int MaxUnits()
	{
		if (m_iCapacity < 1)
			return 1000;

		return m_iCapacity;
	}

	//------------------------------------------------------------------------------------------------
	//! Local point of unit slot `index` on the bed: the bottom centre of that log. Columns run across the
	//! bed (x), layers stack up (y); every log lies along the bed (z) through the bed centre.
	vector SlotPosition(int index)
	{
		int perLayer = m_iPerLayer;
		if (perLayer < 1)
			perLayer = 1;

		int layer = index / perLayer;
		int column = index - layer * perLayer;
		float step = m_fBedWidth / perLayer;
		float x = (column - (perLayer - 1) * 0.5) * step;
		return m_vBedCentre + Vector(x, layer * m_fLayerHeight, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! The local transform that lays a mesh with these bounds along the bed, its bottom centre on `slot`.
	//! A log mesh has its pivot at one end (WoodLog_01_C runs 0 to -4 m on z), so the bounds place it, not
	//! the pivot; a mesh longer on x than on z is turned a quarter first. `wobble` is a small yaw (degrees).
	static void SlotTransform(vector mins, vector maxs, vector slot, float wobble, out vector mat[4])
	{
		vector size = maxs - mins;
		float yaw = wobble;
		if (size[0] > size[2])
			yaw = yaw + 90;

		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), mat);
		vector bottomCentre = Vector((mins[0] + maxs[0]) * 0.5, mins[1], (mins[2] + maxs[2]) * 0.5);
		mat[3] = slot - bottomCentre.Multiply3(mat);
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the stack: one local copy per unit. Skipped on a dedicated server (nobody looks).
	protected void Redraw()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		string packed = LF_CargoBed.Pack(m_aStack);
		if (packed == m_sDrawn)
			return;

		m_sDrawn = packed;
		ClearVisuals(owner.GetWorld());
		if (!GetGame().GetPlayerController())
			return;

		array<ref LF_CargoKind> kinds = LF_CargoBed.Kinds();
		foreach (int index, int kindIndex : m_aStack)
		{
			if (kindIndex < 0 || kindIndex >= kinds.Count() || kinds[kindIndex].m_Model.IsEmpty())
				continue;

			Resource model = Resource.Load(kinds[kindIndex].m_Model);
			if (!model || !model.IsValid())
			{
				LF_Debug.Error(LF_CargoBed.FEATURE, "cargo model does not resolve: " + kinds[kindIndex].m_Model);
				continue;
			}

			EntitySpawnParams params();
			params.TransformMode = ETransformMode.LOCAL;
			params.Parent = owner;
			params.Transform[3] = SlotPosition(index);

			IEntity visual = GetGame().SpawnEntity(GenericEntity, owner.GetWorld(), params);
			if (!visual)
			{
				LF_Debug.Error(LF_CargoBed.FEATURE, "cargo visual failed to spawn on slot " + index.ToString());
				continue;
			}

			visual.SetObject(model.GetResource().ToVObject(), "");
			// Lay it along the bed by its own bounds; a small turn per slot so the stack does not look stamped.
			vector mins, maxs;
			visual.GetBounds(mins, maxs);
			vector mat[4];
			SlotTransform(mins, maxs, SlotPosition(index), (index - (index / 3) * 3 - 1) * 1.5, mat);
			visual.SetLocalTransform(mat);

			m_aVisualIds.Insert(visual.GetID());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearVisuals(BaseWorld world)
	{
		if (world)
		{
			foreach (EntityID id : m_aVisualIds)
			{
				IEntity visual = world.FindEntityByID(id);
				if (visual)
					SCR_EntityHelper.DeleteEntityAndChildren(visual);
			}
		}

		m_aVisualIds.Clear();
	}
}
