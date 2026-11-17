//------------------------------------------------------------------------------------------------
//! Vault robbery action. Offer checks read the gates and do not arm the charge.
//! Perform calls LF_Bank.Rob, which calls LF_Breach.Arm once the gates pass. With the drill held
//! (LF_HandTool) it reads "Drill the vault door" and starts the timed LF_VaultDrill; the robber who
//! is drilling sees "Stop drilling", which cancels and pays nothing.
//------------------------------------------------------------------------------------------------
class LF_BankRobAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		// Hidden while the Robbery system is off (#1073).
		return Vault() != null && LF_Systems.IsEnabled(LF_RobAction.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_BankVaultComponent vault = Vault();
		if (!vault)
		{
			SetCannotPerformReason("#EL-Bank_NotReady");
			return false;
		}

		if (LF_VaultDrill.IsDrillingBy(vault.GetVaultId(), user))
			return true;

		string reason = LF_Bank.GateReason(vault.GetVaultId(), LF_Utils.GetPlayerUid(user), user);
		if (!reason.IsEmpty())
		{
			SetCannotPerformReason(LF_Terms.Text(reason));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		// A prompt shown just before Robbery was turned off (#1073).
		if (LF_Systems.Refuses(LF_RobAction.SYSTEM, "vault robbery"))
		{
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Systems_Off", "#EL-Robbery_Failed", 3.0);
			return;
		}

		LF_BankVaultComponent vault = LF_Component<LF_BankVaultComponent>.Find(pOwnerEntity);
		if (!vault)
		{
			LF_Debug.Error("Bank", "rob action: owner has no LF_BankVaultComponent");
			return;
		}

		string playerUid = LF_Utils.GetPlayerUid(pUserEntity);
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error("Bank", "rob action refused: player has no account id");
			return;
		}

		if (LF_VaultDrill.IsDrillingBy(vault.GetVaultId(), pUserEntity))
		{
			LF_VaultDrill.Cancel(vault.GetVaultId(), "the robber stopped");
			return;
		}

		LF_Bank.Rob(vault.GetVaultId(), playerUid, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = NameFor(Vault(), SCR_PlayerController.GetLocalControlledEntity());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The prompt for this user: stop, drill (drill in the hands) or the vault's own rob name.
	static string NameFor(LF_BankVaultComponent vault, IEntity user)
	{
		if (!vault)
			return "#EL-Bank_Rob";

		if (LF_VaultDrill.IsDrillingBy(vault.GetVaultId(), user))
			return "#LF-Bank_DrillStop";

		if (user && LF_HandTool.Holds(user, LF_VaultDrill.TOOL_ID))
			return "#LF-Bank_Drill";

		return vault.GetActionName();
	}

	//------------------------------------------------------------------------------------------------
	protected LF_BankVaultComponent Vault()
	{
		if (!GetOwner())
			return null;

		return LF_Component<LF_BankVaultComponent>.Find(GetOwner());
	}
}
