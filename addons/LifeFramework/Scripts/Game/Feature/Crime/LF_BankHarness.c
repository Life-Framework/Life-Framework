//------------------------------------------------------------------------------------------------
//! Session proof verb `vaultoffer <vault id>` (runs where the command lands, a client in a session):
//! reads the vault's rob action the way this machine's action menu does (CanBeShownScript,
//! CanBePerformedScript, the reason and the name), so a dedicated client's offer can be checked
//! without the server's word. The `use` verb checks on the server and hides a client-only refusal (#2418).
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "vaultoffer")
			return super.Handle(cmd);

		IEntity user = LF_Utils.GetLocalCharacter();
		if (!user)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_character");
			return true;
		}

		IEntity vaultEntity = LF_Places.Find(LF_Bank.PLACE_KIND, cmd.Arg1);
		if (!vaultEntity)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "vault_not_found");
			return true;
		}

		LF_BankRobAction action = FindRobAction(vaultEntity);
		if (!action)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "action_not_found");
			return true;
		}

		bool shown = action.CanBeShownScript(user);
		bool perform = action.CanBePerformedScript(user);
		string reason = "";
		if (!perform)
			reason = action.GetCannotPerformReason();

		string name;
		action.GetActionNameScript(name);
		string detail = string.Format("server=%1 shown=%2 perform=%3 reason=%4 name=%5", Replication.IsServer(), shown, perform, reason, name);
		LF_Debug.Info("Bank", "vaultoffer " + detail);
		LF_AgentHarness.Answer(cmd.Id, "ok", detail);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_BankRobAction FindRobAction(IEntity entity)
	{
		BaseActionsManagerComponent mgr = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!mgr)
			return null;

		array<BaseUserAction> actions = {};
		mgr.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_BankRobAction rob = LF_BankRobAction.Cast(action);
			if (rob)
				return rob;
		}

		return null;
	}
}
