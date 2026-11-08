//! Opens the counter's price list for the player who used it. Buying and selling go through the
//! server RPCs on EL_CharacterShopComponent, which re-derive every price.
[BaseContainerProps(configRoot: true)]
class EL_ShopAction : ScriptedUserAction
{
	[Attribute("SOUND_INV_HOTKEY_SCROLL", desc: "UI sound event for choosing a line or changing the quantity")]
	protected string m_sKeySound;

	[Attribute("SOUND_HUD_GADGET_CONFIRM", desc: "UI sound event when the till takes the payment or pays out")]
	protected string m_sTillSound;

	[Attribute("SOUND_FE_BUTTON_FAIL", desc: "UI sound event when the till refuses")]
	protected string m_sRefuseSound;

	[Attribute("SOUND_FE_TURN_PAGE", desc: "UI sound event when the price list is picked up")]
	protected string m_sOpenSound;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		EL_ShopComponent shop = EL_ShopComponent.Cast(GetOwner().FindComponent(EL_ShopComponent));
		return shop != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The price list is local UI: run PerformAction only on the acting client. Without this a listen
	//! host also runs it for every other player and opens the list on the host screen. Server
	//! work goes through the RPCs, which the server validates.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			EL_Debug.Error("Shop", "action performed without a counter or a user");
			return;
		}

		// Second guard behind HasLocalEffectOnlyScript: only the player at the counter sees it.
		if (!LF_CounterAccess.IsLocalActor(pUserEntity, EL_Utils.GetLocalCharacter()))
		{
			EL_Debug.Info("Shop", "price list not opened: another player used the counter");
			return;
		}

		EL_ShopComponent shop = EL_ShopComponent.Cast(pOwnerEntity.FindComponent(EL_ShopComponent));
		if (!shop)
		{
			EL_Debug.Error("Shop", "counter has no EL_ShopComponent");
			return;
		}

		LF_ShopPriceList.Open(shop, this);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user)
			return false;

		IEntity owner = GetOwner();
		if (!owner)
			return false;

		return vector.Distance(user.GetOrigin(), owner.GetOrigin()) <= LF_ShopPriceList.SHOW_DISTANCE;
	}

	//------------------------------------------------------------------------------------------------
	string GetKeySound()
	{
		return m_sKeySound;
	}

	//------------------------------------------------------------------------------------------------
	string GetTillSound()
	{
		return m_sTillSound;
	}

	//------------------------------------------------------------------------------------------------
	string GetRefuseSound()
	{
		return m_sRefuseSound;
	}

	//------------------------------------------------------------------------------------------------
	string GetOpenSound()
	{
		return m_sOpenSound;
	}
};
