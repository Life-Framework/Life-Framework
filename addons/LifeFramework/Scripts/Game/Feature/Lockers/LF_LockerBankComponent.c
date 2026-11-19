[ComponentEditorProps(category: "LifeFramework/Lockers", description: "A bank of rentable lockers: one locker per character at this site (#2659)")]
class LF_LockerBankComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Marks a prop as a locker bank (#2659). The site id is the stable key rentals are saved under; set it
//! on the placed instance (pack data). A bank left without one is keyed by its rounded position and
//! warns, so moving it loses its rentals. Banks register themselves so the server finds the one a
//! character stands at (LF_LockerBank.RentNearest).
class LF_LockerBankComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Stable site id the rentals at this bank are saved under, unique per bank")]
	protected string m_sSiteId;

	[Attribute("2.5", UIWidgets.Auto, "Metres from the bank a character may rent from", params: "0.5 20")]
	protected float m_fReach;

	protected static ref array<EntityID> s_aBanks;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aBanks)
			s_aBanks = new array<EntityID>();

		s_aBanks.Insert(owner.GetID());
		if (m_sSiteId.IsEmpty())
			LF_Debug.Warn(LF_LockerBank.FEATURE, string.Format("locker bank at %1 has no site id; keyed by position", owner.GetOrigin()));

		// Rents lapse at every bank, with or without a lost-and-found box beside it (#3457).
		LF_LostAndFound.ArmSweep();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aBanks)
			s_aBanks.RemoveItem(owner.GetID());

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! The stable key of this site; a bank with none set falls back to its rounded position.
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
	//! The nearest registered bank whose reach covers `position`, or null.
	static LF_LockerBankComponent FindNearest(vector position)
	{
		if (!s_aBanks || !GetGame() || !GetGame().GetWorld())
			return null;

		BaseWorld world = GetGame().GetWorld();
		LF_LockerBankComponent best;
		float bestDistance = float.MAX;
		foreach (EntityID id : s_aBanks)
		{
			IEntity entity = world.FindEntityByID(id);
			if (!entity)
				continue;

			LF_LockerBankComponent bank = LF_LockerBankComponent.Cast(entity.FindComponent(LF_LockerBankComponent));
			if (!bank)
				continue;

			float distance = vector.Distance(position, entity.GetOrigin());
			if (distance <= bank.GetReach() && distance < bestDistance)
			{
				best = bank;
				bestDistance = distance;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aBanks = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LockerBankComponent's statics (LF_StaticReset finds it by reflection).
class LF_LockerBankComponent_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LockerBankComponent";
	}

	override void Apply()
	{
		LF_LockerBankComponent.ResetStatics();
	}
}
