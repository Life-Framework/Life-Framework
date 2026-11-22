// LF_HudBankMirror.c: the bank balance the HUD shows under the cash. The server owns the balance
// (LF_BankAccount); this copies it onto the player's own character as an owner-only RplProp, so
// the HUD reads it without asking. Written on spawn and on every bank LF_OnMoneyMoved; a client
// never writes it. Display only: no purchase, withdrawal or check reads this copy.

//------------------------------------------------------------------------------------------------
class LF_HudBankMirrorSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnMoneyMoved moved = LF_OnMoneyMoved.Cast(busEvent);
		if (!moved || moved.m_sAccount != LF_Ledger.ACCOUNT_BANK)
			return true;

		LF_HudBankMirror.Write(moved.m_sPlayerUid, moved.m_iBalanceAfter);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_HudBankMirror
{
	protected static ref LF_HudBankMirrorSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	//! Server: subscribe once. Called when the game mode starts.
	static void EnsureSubscribed()
	{
		if (!Replication.IsServer() || s_Subscriber)
			return;

		s_Subscriber = new LF_HudBankMirrorSubscriber();
		LF_EventBus.Subscribe(LF_OnMoneyMoved, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the balance of the account this freshly controlled body plays. No account yet is 0.
	static void PublishFor(int playerId, IEntity controlled)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(controlled);
		if (!Replication.IsServer() || !character || playerId <= 0)
			return;

		// By player id: the body may not be registered as controlled yet when the spawn hook runs.
		string uid = LF_Utils.GetPlayerUID(playerId);
		if (uid.IsEmpty())
			return;

		int balance = 0;
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		if (atm)
		{
			LF_BankAccount account = atm.GetAccount(uid);
			if (account)
				balance = account.GetBalance();
		}

		character.LF_SetHudBank(balance);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: write a new balance onto every body playing that account (normally one, or none
	//! when the owner is offline: a transfer to someone away).
	static void Write(string uid, int balance)
	{
		if (uid.IsEmpty() || !GetGame())
			return;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int playerId : ids)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(players.GetPlayerControlledEntity(playerId));
			if (character && LF_Utils.GetPlayerUID(character) == uid)
				character.LF_SetHudBank(balance);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Subscriber = null;
	}
}

//------------------------------------------------------------------------------------------------
class LF_HudBankMirror_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_HudBankMirror";
	}

	override void Apply()
	{
		LF_HudBankMirror.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//! The owner's bank balance for the HUD; -1 until the server writes one.
	[RplProp(condition: RplCondition.OwnerOnly)]
	protected int m_iLF_HudBank = -1;

	//------------------------------------------------------------------------------------------------
	void LF_SetHudBank(int balance)
	{
		if (!Replication.IsServer() || balance == m_iLF_HudBank)
			return;

		m_iLF_HudBank = balance;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	int LF_GetHudBank()
	{
		return m_iLF_HudBank;
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_HudBankMirror.EnsureSubscribed();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);
		LF_HudBankMirror.PublishFor(playerId, controlledEntity);
	}
}
