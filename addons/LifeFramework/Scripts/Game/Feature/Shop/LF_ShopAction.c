//! Opens the counter's price list for the player who used it. Buying and selling go through the
//! server RPCs on LF_CharacterShopComponent, which re-derive every price.
[BaseContainerProps(configRoot: true)]
class LF_ShopAction : ScriptedUserAction
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
		LF_ShopComponent shop = LF_ShopComponent.Cast(GetOwner().FindComponent(LF_ShopComponent));
		if (!shop)
			return false;

		string reason = LF_BackRoomDoor.ReasonKey(shop.IsBackRoom(), shop.IsShutShown());
		if (!reason.IsEmpty())
		{
			SetCannotPerformReason(reason);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A back room names itself while it is shut; every other counter keeps the name on its prefab.
	override bool GetActionNameScript(out string outName)
	{
		LF_ShopComponent shop = LF_ShopComponent.Cast(GetOwner().FindComponent(LF_ShopComponent));
		if (!shop)
			return false;

		outName = LF_BackRoomDoor.LabelKey(shop.IsBackRoom(), shop.IsShutShown());
		if (outName == "#LF-Shop_Open_BackRoom")
			return false;

		return !outName.IsEmpty();
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
			LF_Debug.Error("Shop", "action performed without a counter or a user");
			return;
		}

		// Second guard behind HasLocalEffectOnlyScript: only the player at the counter sees it.
		if (!LF_CounterAccess.IsLocalActor(pUserEntity, LF_Utils.GetLocalCharacter()))
		{
			LF_Debug.Info("Shop", "price list not opened: another player used the counter");
			return;
		}

		LF_ShopComponent shop = LF_ShopComponent.Cast(pOwnerEntity.FindComponent(LF_ShopComponent));
		if (!shop)
		{
			LF_Debug.Error("Shop", "counter has no LF_ShopComponent");
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
		if (!owner || LF_NpcStun.BlocksTalk(owner))
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
