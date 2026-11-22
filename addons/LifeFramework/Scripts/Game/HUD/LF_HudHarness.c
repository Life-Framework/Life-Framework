// LF_HudHarness.c: the `hud` harness verb, so a session can put the subtle HUD into the states its
// pictures need: `hud <wanted> <hunger> <thirst> [bankDeposit]`. The server applies it behind the
// dev gate through the same mutators play uses (SetWantedLevel, SetSurvivalStats, Deposit), so the
// HUD shows what replication really delivers.

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "hud")
			return super.Handle(cmd);

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_character");
			return true;
		}

		character.LF_AskHarnessHud(cmd.Id, cmd.Arg1.ToInt(), cmd.Arg2.ToInt(), cmd.Arg3.ToInt(), cmd.Arg4.ToInt());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskHarnessHud(int commandId, int wanted, int hunger, int thirst, int bankDeposit)
	{
		if (Replication.IsServer())
		{
			LF_ApplyHarnessHud(commandId, true, wanted, hunger, thirst, bankDeposit);
			return;
		}

		Rpc(RpcAsk_LF_HarnessHud, commandId, wanted, hunger, thirst, bankDeposit);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessHud(int commandId, int wanted, int hunger, int thirst, int bankDeposit)
	{
		bool gateOpen = LF_DevGate.CheckHarnessRequest(this, "hud");
		LF_ApplyHarnessHud(commandId, gateOpen, wanted, hunger, thirst, bankDeposit);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ApplyHarnessHud(int commandId, bool gateOpen, int wanted, int hunger, int thirst, int bankDeposit)
	{
		if (!gateOpen)
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		// The account is keyed by the player (as LF_AccountReplication.PublishForPlayer reads it), the
		// bank by the active character (as the ATM reads it).
		string uid = LF_Utils.GetPlayerUID(this);
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(this);
		string accountUid = LF_Utils.GetAccountUID(playerId);
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account;
		if (accounts && !accountUid.IsEmpty())
			account = accounts.GetAccount(accountUid);
		if (account)
			account.SetWantedLevel(wanted);

		LF_CharacterSurvivalComponent survival = LF_CharacterSurvivalComponent.Cast(FindComponent(LF_CharacterSurvivalComponent));
		if (survival && survival.GetSurvivalStats())
		{
			LF_SurvivalStats stats = survival.GetSurvivalStats();
			stats.SetHunger(hunger);
			stats.SetThirst(thirst);
			survival.SetSurvivalStats(stats);
		}

		if (bankDeposit > 0)
		{
			LF_BankAccount bank = LF_ATMManager.GetOrCreate(uid);
			if (bank)
				bank.Deposit(bankDeposit);
		}

		LF_Debug.Info(LF_HudLogic.FEATURE, string.Format("harness hud: wanted %1 hunger %2 thirst %3 bank +%4 (account %5)", wanted, hunger, thirst, bankDeposit, account != null));
		LF_HarnessReply(commandId, "ok", string.Format("wanted=%1 hunger=%2 thirst=%3 bank+=%4", wanted, hunger, thirst, bankDeposit));
	}
}
