//------------------------------------------------------------------------------------------------
//! Client->server bridge for the organization desk. The character is the entity
//! the player owns, so the client can RPC it. The server checks the character,
//! moves MoneyStack, and answers with the menu row.
//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void EL_AskOrganizationView()
	{
		Rpc(RpcAsk_EL_OrganizationView);
	}

	//------------------------------------------------------------------------------------------------
	void EL_AskOrganizationCreate(string name)
	{
		Rpc(RpcAsk_EL_OrganizationCreate, name);
	}

	//------------------------------------------------------------------------------------------------
	void EL_AskOrganizationDeposit(int amount)
	{
		Rpc(RpcAsk_EL_OrganizationDeposit, amount);
	}

	//------------------------------------------------------------------------------------------------
	void EL_AskOrganizationWithdraw(int amount)
	{
		Rpc(RpcAsk_EL_OrganizationWithdraw, amount);
	}

	//------------------------------------------------------------------------------------------------
	void EL_AskOrganizationBuy(int upgrade)
	{
		Rpc(RpcAsk_EL_OrganizationBuy, upgrade);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_OrganizationView()
	{
		EL_PushOrganizationView();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_OrganizationCreate(string name)
	{
		EL_OrganizationPlayer.Create(this, name);
		EL_PushOrganizationView();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_OrganizationDeposit(int amount)
	{
		EL_OrganizationPlayer.DepositCash(this, amount);
		EL_PushOrganizationView();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_OrganizationWithdraw(int amount)
	{
		EL_OrganizationPlayer.WithdrawCash(this, amount);
		EL_PushOrganizationView();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_OrganizationBuy(int upgrade)
	{
		EL_EOrganizationUpgrade kind = EL_EOrganizationUpgrade.PROCESSING_SLOT;
		if (upgrade == EL_EOrganizationUpgrade.EXTRA_STORAGE)
			kind = EL_EOrganizationUpgrade.EXTRA_STORAGE;
		else if (upgrade == EL_EOrganizationUpgrade.PARKED_VEHICLE)
			kind = EL_EOrganizationUpgrade.PARKED_VEHICLE;
		else if (upgrade != EL_EOrganizationUpgrade.PROCESSING_SLOT)
		{
			EL_Debug.Error("Organizations", string.Format("upgrade refused: unknown id %1", upgrade));
			EL_PushOrganizationView();
			return;
		}

		EL_OrganizationPlayer.BuyUpgrade(this, kind);
		EL_PushOrganizationView();
	}

	//------------------------------------------------------------------------------------------------
	protected void EL_PushOrganizationView()
	{
		if (!Replication.IsServer())
			return;

		bool hasOrg = false;
		string name = "";
		int fund = 0;
		bool processing = false;
		bool extraStorage = false;
		bool parkedVehicle = false;

		string characterId = EL_OrganizationPlayer.CharacterId(this);
		EL_Organization org;
		if (!characterId.IsEmpty())
			org = EL_OrganizationManager.GetByCharacter(characterId);

		if (org)
		{
			hasOrg = true;
			name = org.GetName();
			fund = org.GetFund();
			processing = org.HasProcessingSlot();
			extraStorage = org.HasExtraStorage();
			parkedVehicle = org.HasParkedVehicle();
		}

		Rpc(RpcDo_EL_OrganizationView, hasOrg, name, fund, processing, extraStorage, parkedVehicle);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_EL_OrganizationView(bool hasOrg, string name, int fund, bool processing, bool extraStorage, bool parkedVehicle)
	{
		EL_OrganizationMenu menu = EL_OrganizationMenu.GetInstance();
		if (!menu)
			return;

		menu.ApplyView(hasOrg, name, fund, processing, extraStorage, parkedVehicle);
	}
}
