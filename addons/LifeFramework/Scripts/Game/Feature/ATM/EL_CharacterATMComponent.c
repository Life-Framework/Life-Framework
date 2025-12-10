[ComponentEditorProps(category: "EveronLife/ATM", description: "Component for character ATM interaction")]
class EL_CharacterATMComponentClass : ScriptComponentClass
{
}

class EL_CharacterATMComponent : ScriptComponent
{
	protected EL_BankAccount m_BankAccount;

	//------------------------------------------------------------------------------------------------
	void Init(string playerUid)
	{
		EL_ATMManager atmManager = EL_ATMManager.GetInstance();
		atmManager.LoadAccountAsync(playerUid, EL_ATMInitCallback.Create(this));
	}

	//------------------------------------------------------------------------------------------------
	EL_BankAccount GetBankAccount()
	{
		return m_BankAccount;
	}

	// Public setter to allow external callbacks to set the bank account safely
	void SetBankAccount(EL_BankAccount account)
	{
		m_BankAccount = account;
	}

	//------------------------------------------------------------------------------------------------
	void OpenATMMenu()
	{
		PlayerController playerController = PlayerController.Cast(GetGame().GetPlayerController());
		if (playerController)
		{
			EL_ATMMenu menu = EL_ATMMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.ATM));
			if (menu)
			{
				menu.SetPlayerController(playerController);
			}
		}
	}
};

class EL_ATMInitCallback : EDF_DataCallbackSingle<EL_BankAccount>
{
	//------------------------------------------------------------------------------------------------
	override void OnComplete(EL_BankAccount data, Managed context)
	{
		EL_CharacterATMComponent comp = EL_CharacterATMComponent.Cast(context);
		if (comp)
			comp.SetBankAccount(data);
	}

	//------------------------------------------------------------------------------------------------
	static EL_ATMInitCallback Create(EL_CharacterATMComponent comp)
	{
		EL_ATMInitCallback instance();
		instance.m_pContext = comp;
		return instance;
	}
};