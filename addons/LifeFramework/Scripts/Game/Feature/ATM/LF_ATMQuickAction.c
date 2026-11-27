//! One line of the cash machine's in-world menu: look at a part, scroll to a line, press use.
//! The cash slot carries the Withdraw lines, the deposit drawer the Deposit lines, the screen and
//! keypad "Check balance". Each line shows its amount; a line the player cannot afford is greyed
//! with the reason. No window opens: the server moves the money (LF_CharacterATMComponent), the
//! machine's screen and a toast show the result (LF_ATMLive).
[BaseContainerProps(configRoot: true)]
class LF_ATMQuickAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, desc: "What this line does", enums: ParamEnumArray.FromEnum(LF_EATMQuickKind))]
	protected LF_EATMQuickKind m_eKind;

	[Attribute("0", desc: "Fixed amount; -1 is everything (the whole balance, or all cash carried)")]
	protected int m_iPreset;

	//------------------------------------------------------------------------------------------------
	LF_EATMQuickKind GetKind()
	{
		return m_eKind;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user || !LF_Systems.IsEnabled(LF_ATMAction.SYSTEM))
			return false;

		IEntity owner = GetOwner();
		if (!owner || vector.Distance(user.GetOrigin(), owner.GetOrigin()) > LF_ATMTerminal.SHOW_DISTANCE)
			return false;

		if (LF_CounterAccess.IsLocalActor(user, LF_Utils.GetLocalCharacter()))
			LF_ATMLive.Near(owner);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user || !user.FindComponent(LF_CharacterATMComponent))
			return false;

		LF_EATMRefusal reason = LF_CharacterATMComponent.ActorRefusal(user);
		if (reason == LF_EATMRefusal.NONE && m_eKind != LF_EATMQuickKind.BALANCE && LF_ATMLive.IsWaiting())
		{
			SetCannotPerformReason("#LF-ATM_QuickWorking");
			return false;
		}

		// "All" needs the figure first; the screen asks for it the moment the player arrives.
		if (reason == LF_EATMRefusal.NONE && m_iPreset == LF_ATMQuick.ALL && LF_ATMQuick.Resolve(m_eKind, m_iPreset, LF_ATMLive.Balance(), LF_ATMLive.Cash()) <= 0
			&& (LF_ATMLive.Balance() == LF_ATMQuick.UNKNOWN || LF_ATMLive.Cash() == LF_ATMQuick.UNKNOWN))
		{
			SetCannotPerformReason("#LF-ATM_QuickReading");
			return false;
		}

		if (reason == LF_EATMRefusal.NONE)
			reason = LF_ATMQuick.Check(m_eKind, m_iPreset, LF_ATMLive.Balance(), LF_ATMLive.Cash());

		if (reason == LF_EATMRefusal.NONE)
			return true;

		SetCannotPerformReason(LF_ATMQuick.ReasonKey(reason));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		string arg;
		string key = LF_ATMQuick.LabelKey(m_eKind, m_iPreset, LF_ATMLive.Balance(), LF_ATMLive.Cash(), arg);
		outName = WidgetManager.Translate(key, arg);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error("ATM", "quick line performed without a machine or a user");
			return;
		}

		if (!LF_CounterAccess.IsLocalActor(pUserEntity, LF_Utils.GetLocalCharacter()))
			return;

		if (m_eKind == LF_EATMQuickKind.BALANCE)
		{
			LF_ATMLive.CheckBalance(pOwnerEntity);
			return;
		}

		int amount = LF_ATMQuick.Resolve(m_eKind, m_iPreset, LF_ATMLive.Balance(), LF_ATMLive.Cash());
		LF_ATMLive.Send(m_eKind, amount, pOwnerEntity);
	}
}
