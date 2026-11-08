[BaseContainerProps(configRoot: true)]
class EL_ShopAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		EL_ShopComponent shop = EL_ShopComponent.Cast(GetOwner().FindComponent(EL_ShopComponent));
		return shop != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The menu is local UI: run PerformAction only on the acting client. Without this a listen
	//! host also runs it for every other player and opens the menu on the host screen. Server
	//! work goes through the menu RPCs, which the server validates.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		EL_ShopComponent shop = EL_ShopComponent.Cast(pOwnerEntity.FindComponent(EL_ShopComponent));
		if (shop)
		{
			shop.OpenShopMenu(pUserEntity);
		}
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user)
			return false;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		// Check distance
		vector userPos = user.GetOrigin();
		vector ownerPos = owner.GetOrigin();
		float distance = vector.Distance(userPos, ownerPos);
		if (distance > 3.0)
			return false;

		return true;
	}
};