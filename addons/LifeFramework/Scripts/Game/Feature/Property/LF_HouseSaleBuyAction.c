//------------------------------------------------------------------------------------------------
//! "Buy house (tier, $price)" on a house's for-sale sign, "Sold" once somebody owns it (#3207). The action
//! only asks: the server finds the house from the caller's position, checks the owner and the cash, and
//! answers with a toast. The client reads only the sign's replicated owned flag, never a property book.
//------------------------------------------------------------------------------------------------
class LF_HouseSaleBuyAction : ScriptedUserAction
{
	protected LF_HouseSaleComponent m_Sign;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Sign = LF_Component<LF_HouseSaleComponent>.Find(pOwnerEntity);
		if (!m_Sign)
			LF_Debug.Error("Property", "Buy house action on an entity with no LF_HouseSaleComponent; action hidden");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		// A priced house shows the action either way: "Buy" while for sale, "Sold" once it is not.
		return m_Sign && m_Sign.GetPrice() > 0;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return m_Sign && m_Sign.IsForSale();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!m_Sign)
			return false;

		if (m_Sign.IsOwned())
		{
			outName = "#LF-Property_HouseSold";
			return true;
		}

		outName = WidgetManager.Translate("#LF-Property_HouseBuy", WidgetManager.Translate(m_Sign.GetTierLabel()), LF_MoneyFormat.FormatMoney(m_Sign.GetPrice()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!m_Sign || !pUserEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error("Property", "Buy house dropped: no local player controller");
			return;
		}

		controller.LF_RequestBuyHouse(m_Sign.GetHouseId());
	}
}
