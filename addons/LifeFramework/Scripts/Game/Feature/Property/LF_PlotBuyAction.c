//------------------------------------------------------------------------------------------------
//! "Buy plot ($price)" on a for-sale plot sign (#2649). The action only asks: the server finds the
//! plot from the caller's position, checks the owner and the cash, and answers with a toast.
//------------------------------------------------------------------------------------------------
class LF_PlotBuyAction : ScriptedUserAction
{
	protected LF_PlotComponent m_Plot;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Plot = LF_Component<LF_PlotComponent>.Find(pOwnerEntity);
		if (!m_Plot)
			LF_Debug.Error("Property", "Buy plot action on an entity with no LF_PlotComponent; action hidden");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return m_Plot && m_Plot.GetPrice() > 0;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!m_Plot)
			return false;

		outName = WidgetManager.Translate("#LF-Property_Buy", LF_MoneyFormat.FormatMoney(m_Plot.GetPrice()));
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
		if (!pUserEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error("Property", "Buy plot dropped: no local player controller");
			return;
		}

		controller.LF_RequestBuyProperty();
	}
}
