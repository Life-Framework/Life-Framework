//------------------------------------------------------------------------------------------------
enum LF_EFarmPlotMode
{
	PLANT,
	HARVEST
}

//------------------------------------------------------------------------------------------------
//! What a farmer does at a plot (#614), one class with a mode. PLANT shows on an empty plot and
//! needs a seed packet. HARVEST shows while something grows: the prompt names the stage until the
//! crop is ripe, then it harvests. There is no menu and no bar; the crop on the bed is the state.
class LF_FarmPlotAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "What this action does at the plot", "", ParamEnumArray.FromEnum(LF_EFarmPlotMode))]
	protected LF_EFarmPlotMode m_eMode;

	protected LF_FarmPlotComponent m_Plot;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Plot = LF_Component<LF_FarmPlotComponent>.Find(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		// A server with farming off offers nothing at the plot; the bed keeps showing its crop (#960).
		if (!m_Plot || !LF_Systems.IsEnabled(LF_FarmPlots.SYSTEM))
			return false;

		if (m_eMode == LF_EFarmPlotMode.PLANT)
			return m_Plot.IsEmpty();

		return !m_Plot.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! A client check for the offer. The server checks everything again.
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_Plot)
			return false;

		if (m_eMode == LF_EFarmPlotMode.PLANT)
		{
			if (LF_InventoryUtils.GetAmount(user, m_Plot.GetSeedPrefab()) > 0)
				return true;

			SetCannotPerformReason("#LF-Farm_NeedSeeds");
			return false;
		}

		if (m_Plot.IsRipe())
			return true;

		SetCannotPerformReason("#LF-Farm_NotRipe");
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The plot in words: "Plant Cabbage seeds", "Growing: Sprout", "Harvest Cabbage".
	override bool GetActionNameScript(out string outName)
	{
		if (!m_Plot)
			return false;

		if (m_eMode == LF_EFarmPlotMode.PLANT)
		{
			outName = WidgetManager.Translate("#LF-Farm_Plant", WidgetManager.Translate(ItemName(m_Plot.GetSeedPrefab())));
			return true;
		}

		if (m_Plot.IsRipe())
		{
			outName = WidgetManager.Translate("#LF-Farm_Harvest", WidgetManager.Translate(ItemName(m_Plot.GetHarvestPrefab())));
			return true;
		}

		outName = WidgetManager.Translate("#LF-Farm_Growing", WidgetManager.Translate(m_Plot.GetStageName()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static string ItemName(ResourceName prefab)
	{
		UIInfo info = LF_UIInfoUtils.GetInfo(prefab);
		if (info && !info.GetName().IsEmpty())
			return info.GetName();

		return "#LF-Gather_Unknown";
	}

	//------------------------------------------------------------------------------------------------
	//! The reach-down gesture starts with the hold, on the acting client only.
	override void OnActionStart(IEntity pUserEntity)
	{
		if (!pUserEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(pUserEntity.FindComponent(CharacterControllerComponent));
		if (controller && controller.CanPlayItemGesture())
			controller.TryPlayItemGesture(EItemGesture.EItemGesturePickUp);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!m_Plot || !pUserEntity)
		{
			LF_Debug.Error(LF_FarmPlots.FEATURE, "plot action performed without a plot or a user");
			return;
		}

		if (!Replication.IsServer())
			return;

		LF_EFarmPlot result;
		if (m_eMode == LF_EFarmPlotMode.PLANT)
			result = m_Plot.ServerPlant(pUserEntity);
		else
			result = m_Plot.ServerHarvest(pUserEntity);

		if (result != LF_EFarmPlot.OK)
		{
			LF_Debug.Info(LF_FarmPlots.FEATURE, string.Format("plot %1 refused %2 (%3)", m_Plot.GetPlotKey(), typename.EnumToString(LF_EFarmPlotMode, m_eMode), typename.EnumToString(LF_EFarmPlot, result)));
			string key = LF_FarmPlotRules.RefusalKey(result);
			if (!key.IsEmpty())
				LF_Utils.NotifyEntity(pUserEntity, key, "#LF-Farm_Title", 4.0);
			return;
		}

		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(pUserEntity);
		if (inventory)
			inventory.RpcAsk_PlaySound(LF_NetworkUtils.GetRplId(pUserEntity), "SOUND_PICK_UP");
	}
}
