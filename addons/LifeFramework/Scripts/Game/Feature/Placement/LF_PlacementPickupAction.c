//------------------------------------------------------------------------------------------------
//! The "Pick up" on a placed permanent prop (#2751, after #2653): the owner's own action to take it
//! back. Three parts:
//!  - LF_PlacedOwnerComponent on the placed prefab carries the placer's character key, replicated,
//!    so a client can tell whose prop it is. LF_PlacementSave stamps it when it records the
//!    placement and when it spawns a saved one again.
//!  - LF_PlacementPickupAction shows only to that owner and calls LF_PlacementPickup.PickUp on the server.
//!  - LF_PlacementPickup.PickUp decides again from the saved row (owner, reach): a client can show or
//!    perform nothing it was not allowed to, and a replicated key is only a prompt.
//! A placed prefab that should be pick-up-able carries both the component and the action
//! (`Prefabs/Placeables/Furniture/Furniture_*_Placed.et` are the pattern).
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Placement", description: "Carries the character key of whoever placed this permanent prop, for its Pick up action.")]
class LF_PlacedOwnerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_PlacedOwnerComponent : ScriptComponent
{
	//! The placer's character key (LF_Houses.CharacterKeyOf). Written on the server only; replicated
	//! so a client's prompt matches the server's decision.
	[RplProp()]
	protected string m_sOwnerKey;

	//------------------------------------------------------------------------------------------------
	string GetOwnerKey()
	{
		return m_sOwnerKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sets the owner and replicates it, only when it changed.
	void SetOwnerKey(string ownerKey)
	{
		if (!Replication.IsServer() || m_sOwnerKey == ownerKey)
			return;

		m_sOwnerKey = ownerKey;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stamps a placed entity with its owner. A prefab without the component simply has no
	//! pick-up action (nothing to log: a spike strip is not meant to).
	static void Stamp(IEntity placed, string ownerKey)
	{
		LF_PlacedOwnerComponent component = LF_Component<LF_PlacedOwnerComponent>.Find(placed);
		if (component)
			component.SetOwnerKey(ownerKey);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `user` is the character who placed `placed` (the key is known and equal).
	static bool IsOwnedBy(IEntity placed, IEntity user)
	{
		LF_PlacedOwnerComponent component = LF_Component<LF_PlacedOwnerComponent>.Find(placed);
		if (!component)
			return false;

		string ownerKey = component.GetOwnerKey();
		return !ownerKey.IsEmpty() && ownerKey == LF_Houses.CharacterKeyOf(user);
	}
}

//------------------------------------------------------------------------------------------------
class LF_PlacementPickupAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_PlacedOwnerComponent.IsOwnedBy(GetOwner(), user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return LF_PlacedOwnerComponent.IsOwnedBy(GetOwner(), user);
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: LF_PlacementPickup.PickUp checks the owner against the saved row and the reach.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_PlacementPickup.PickUp(pUserEntity, pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Placement_PickUp";
		return true;
	}
}
