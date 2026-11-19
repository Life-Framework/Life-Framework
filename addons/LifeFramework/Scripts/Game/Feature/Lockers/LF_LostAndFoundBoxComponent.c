[ComponentEditorProps(category: "LifeFramework/Lockers", description: "The lost-and-found box beside a locker bank: lapsed lockers' contents wait here for their owner (#3457)")]
class LF_LostAndFoundBoxComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Marks a prop as a site's lost-and-found box (#3457). Set the same site id as the locker bank beside it
//! (pack data): the things of every locker that lapsed at that site wait under it. A box left without one
//! is keyed by its rounded position and warns, so it never finds a bank's lapsed lockers. Boxes register
//! themselves so the server finds the one a character stands at (LF_LostAndFound.ReclaimNearest), and the
//! first one placed starts the slow lapse sweep.
class LF_LostAndFoundBoxComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Site id of the locker bank this box belongs to; the same id the bank's LF_LockerBankComponent carries")]
	protected string m_sSiteId;

	[Attribute("2.5", UIWidgets.Auto, "Metres from the box a character may reclaim from", params: "0.5 20")]
	protected float m_fReach;

	protected static ref array<EntityID> s_aBoxes;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aBoxes)
			s_aBoxes = new array<EntityID>();

		s_aBoxes.Insert(owner.GetID());
		if (m_sSiteId.IsEmpty())
			LF_Debug.Warn(LF_LockerBank.FEATURE, string.Format("lost-and-found box at %1 has no site id; keyed by position", owner.GetOrigin()));

		LF_LostAndFound.ArmSweep();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aBoxes)
			s_aBoxes.RemoveItem(owner.GetID());

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! The site this box belongs to; a box with none set falls back to its rounded position.
	string GetSiteId()
	{
		if (!m_sSiteId.IsEmpty())
			return m_sSiteId;

		vector origin = GetOwner().GetOrigin();
		return string.Format("locker@%1,%2", Math.Round(origin[0]), Math.Round(origin[2]));
	}

	//------------------------------------------------------------------------------------------------
	float GetReach()
	{
		return m_fReach;
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest registered box whose reach covers `position`, or null.
	static LF_LostAndFoundBoxComponent FindNearest(vector position)
	{
		if (!s_aBoxes || !GetGame() || !GetGame().GetWorld())
			return null;

		BaseWorld world = GetGame().GetWorld();
		LF_LostAndFoundBoxComponent best;
		float bestDistance = float.MAX;
		foreach (EntityID id : s_aBoxes)
		{
			IEntity entity = world.FindEntityByID(id);
			if (!entity)
				continue;

			LF_LostAndFoundBoxComponent box = LF_LostAndFoundBoxComponent.Cast(entity.FindComponent(LF_LostAndFoundBoxComponent));
			if (!box)
				continue;

			float distance = vector.Distance(position, entity.GetOrigin());
			if (distance <= box.GetReach() && distance < bestDistance)
			{
				best = box;
				bestDistance = distance;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aBoxes = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LostAndFoundBoxComponent's statics (LF_StaticReset finds it by reflection).
class LF_LostAndFoundBoxComponent_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LostAndFoundBoxComponent";
	}

	override void Apply()
	{
		LF_LostAndFoundBoxComponent.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Lockers", description: "Inventory manager of the lost-and-found box: the transient place a reclaim unpacks things into before they go to their owner")]
class LF_LostAndFoundStorageManagerComponentClass : ScriptedInventoryStorageManagerComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Holds nothing between reclaims: the server unpacks one owner's record into the box's storage and moves
//! it to that owner in the same call (LF_LostAndFound.ReclaimAt). Nobody opens this storage.
class LF_LostAndFoundStorageManagerComponent : ScriptedInventoryStorageManagerComponent
{
}
