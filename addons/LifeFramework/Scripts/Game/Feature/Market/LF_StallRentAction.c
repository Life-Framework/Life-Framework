//------------------------------------------------------------------------------------------------
//! "Rent stall (fee)" on a market stall (#2925). A free stall offers the rent; a rented one shows whose it
//! is (the replicated holder name) and cannot be used. CanBeShown/CanBePerformed run on the client for the
//! prompt only; PerformAction runs on the server, which decides everything again in LF_StallComponent.Rent
//! (clock, one stall per player, cash) and answers with a toast. Nothing is trusted from the client.
//------------------------------------------------------------------------------------------------
class LF_StallRentAction : ScriptedUserAction
{
	protected LF_StallComponent m_Stall;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);

		m_Stall = LF_Component<LF_StallComponent>.Find(pOwnerEntity);
		if (!m_Stall)
			LF_Debug.Error(LF_StallConfig.FEATURE, "Rent stall action on an entity with no LF_StallComponent; action hidden");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Stall)
			return false;

		LF_StallConfig config = LF_StallConfig.Get();
		return config && config.m_bEnabled;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_Stall)
			return false;

		if (m_Stall.IsRented())
		{
			SetCannotPerformReason(WidgetManager.Translate("#LF-Stall_Taken", HolderLabel()));
			return false;
		}

		LF_StallConfig config = LF_StallConfig.Get();
		int day;
		int hour;
		if (!config || !LF_StallClock.Now(day, hour) || !LF_StallRules.IsMarketOpen(config, day, hour))
		{
			SetCannotPerformReason("#LF-Stall_Closed");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected string HolderLabel()
	{
		string name = m_Stall.GetHolderName();
		if (name.IsEmpty())
			return WidgetManager.Translate("#LF-Stall_Unnamed");

		return name;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!m_Stall)
			return false;

		if (m_Stall.IsRented())
		{
			outName = WidgetManager.Translate("#LF-Stall_Taken", HolderLabel());
			return true;
		}

		LF_StallConfig config = LF_StallConfig.Get();
		int fee;
		if (config)
			fee = config.m_iPitchFee;

		outName = WidgetManager.Translate("#LF-Stall_Rent", LF_MoneyFormat.FormatMoney(fee));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rents the stall for the user and tells them how it went.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !m_Stall || !pUserEntity)
			return;

		LF_EStallResult result = m_Stall.Rent(pUserEntity, LF_Utils.GetPlayerUid(pUserEntity));
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (playerId <= 0)
			return;

		string message = "#LF-Stall_Rented";
		LF_ENotificationType type = LF_ENotificationType.INFO;
		if (result != LF_EStallResult.OK)
		{
			type = LF_ENotificationType.WARNING;
			switch (result)
			{
				case LF_EStallResult.CANT_AFFORD:
					message = "#LF-Stall_NoCash";
					break;
				case LF_EStallResult.ALREADY_RENTED:
					message = "#LF-Stall_AlreadyRented";
					break;
				default:
					message = "#LF-Stall_Closed";
					break;
			}
		}

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Stall_Title", message, 6.0, type);
	}
}
