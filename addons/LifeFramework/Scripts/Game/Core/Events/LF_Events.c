//------------------------------------------------------------------------------------------------
//! The first bus events (docs/architecture/events.md). Fields are plain values: uids and ints,
//! never IEntity or EntityID. Published on the server only, after the state change happened.

//! Cash or bank balance moved. m_iAmount is positive; m_bCredit says the direction; m_sSource is
//! the reason key ("atm-deposit", "shop-buy", ...). One event per account side: an ATM deposit is
//! a cash debit and a bank credit. Published by LF_MoneyUtils (cash) and LF_BankAccount (bank).
class LF_OnMoneyMoved : LF_Event
{
	string m_sPlayerUid;
	int m_iAmount;
	string m_sSource;
	//! LF_Ledger.ACCOUNT_CASH or ACCOUNT_BANK. Empty on an event built with Create.
	string m_sAccount;
	//! True when the balance went up. Meaningful with m_sAccount.
	bool m_bCredit;
	//! The balance of m_sAccount after the change.
	int m_iBalanceAfter;
	//! The other character of a hand-over, empty for a system source or sink.
	string m_sCounterpartyUid;

	static LF_OnMoneyMoved CreateFull(string playerUid, string account, int amount, bool credit, string reason, int balanceAfter, string counterpartyUid)
	{
		LF_OnMoneyMoved e = LF_OnMoneyMoved.Create(playerUid, amount, reason);
		e.m_sAccount = account;
		e.m_bCredit = credit;
		e.m_iBalanceAfter = balanceAfter;
		e.m_sCounterpartyUid = counterpartyUid;
		return e;
	}

	static LF_OnMoneyMoved Create(string playerUid, int amount, string source)
	{
		LF_OnMoneyMoved e = new LF_OnMoneyMoved();
		e.m_sPlayerUid = playerUid;
		e.m_iAmount = amount;
		e.m_sSource = source;
		return e;
	}
}

//! A crime was reported (the wanted report is filed). m_sOffence is an OFFENCE_ localization key.
class LF_OnCrimeCommitted : LF_Event
{
	string m_sPlayerUid;
	string m_sOffence;

	static LF_OnCrimeCommitted Create(string playerUid, string offence)
	{
		LF_OnCrimeCommitted e = new LF_OnCrimeCommitted();
		e.m_sPlayerUid = playerUid;
		e.m_sOffence = offence;
		return e;
	}
}

//! A player went down but is not dead. Defined; no feature has a downed state yet, so nothing publishes it.
class LF_OnPlayerDowned : LF_Event
{
	string m_sPlayerUid;
}

//! A player character died. m_sKillerUid is empty when no player killed them.
class LF_OnPlayerKilled : LF_Event
{
	string m_sVictimUid;
	string m_sKillerUid;
	//! Where the victim died. Zero when the publisher did not know.
	vector m_vPosition;
	//! Prefab of the killer's current weapon; empty when unarmed or unknown.
	string m_sWeapon;
	//! LF_Murder's rule counts this death as murder (set before the wanted decision).
	bool m_bMurder;

	static LF_OnPlayerKilled Create(string victimUid, string killerUid)
	{
		LF_OnPlayerKilled e = new LF_OnPlayerKilled();
		e.m_sVictimUid = victimUid;
		e.m_sKillerUid = killerUid;
		return e;
	}
}

//! A jail sentence started.
class LF_OnArrested : LF_Event
{
	string m_sPlayerUid;
	int m_iSentenceSeconds;
	int m_iWantedLevel;

	static LF_OnArrested Create(string playerUid, int seconds, int wanted)
	{
		LF_OnArrested e = new LF_OnArrested();
		e.m_sPlayerUid = playerUid;
		e.m_iSentenceSeconds = seconds;
		e.m_iWantedLevel = wanted;
		return e;
	}
}

//! A job run was completed and paid.
class LF_OnJobCompleted : LF_Event
{
	string m_sPlayerUid;
	int m_iJob;
	int m_iPay;

	static LF_OnJobCompleted Create(string playerUid, int job, int pay)
	{
		LF_OnJobCompleted e = new LF_OnJobCompleted();
		e.m_sPlayerUid = playerUid;
		e.m_iJob = job;
		e.m_iPay = pay;
		return e;
	}
}

//! A vehicle was bought and registered.
class LF_OnVehicleBought : LF_Event
{
	string m_sBuyerUid;
	string m_sCatalogPrefab;
	int m_iPrice;

	static LF_OnVehicleBought Create(string buyerUid, string catalogPrefab, int price)
	{
		LF_OnVehicleBought e = new LF_OnVehicleBought();
		e.m_sBuyerUid = buyerUid;
		e.m_sCatalogPrefab = catalogPrefab;
		e.m_iPrice = price;
		return e;
	}
}

//------------------------------------------------------------------------------------------------
//! One sampled stretch of player-on-player damage. LF_IncidentDamage folds every hit between a
//! victim and an attacker into at most one event per sample window; m_fDamage is the total in it.
class LF_OnPlayerDamaged : LF_Event
{
	string m_sVictimUid;
	string m_sAttackerUid;
	float m_fDamage;
	vector m_vPosition;
	string m_sWeapon;

	static LF_OnPlayerDamaged Create(string victimUid, string attackerUid, float damage, vector position, string weapon)
	{
		LF_OnPlayerDamaged e = new LF_OnPlayerDamaged();
		e.m_sVictimUid = victimUid;
		e.m_sAttackerUid = attackerUid;
		e.m_fDamage = damage;
		e.m_vPosition = position;
		e.m_sWeapon = weapon;
		return e;
	}
}

//------------------------------------------------------------------------------------------------
//! A player called a service (police, medic, taxi, ...) and at least one receiver is on duty for it.
//! Dispatch (#250) and any delivery channel subscribe; the caller does not know who listens.
//! m_aReceiverIds are server-side player ids, never sent to clients.
class LF_OnServiceCall : LF_Event
{
	string m_sCallerUid;
	string m_sService;
	//! Place key or grid text the call came from (a payphone's place).
	string m_sPlace;
	//! What happened, a preset key or free text.
	string m_sSummary;
	//! Where the call is delivered: comma list of pager, board, radio.
	string m_sChannels;
	//! World position of the caller's phone; zero when unknown.
	vector m_vPosition;
	ref array<int> m_aReceiverIds = {};

	static LF_OnServiceCall Create(string callerUid, string service, string place, string summary, string channels, vector position, array<int> receivers)
	{
		LF_OnServiceCall e = new LF_OnServiceCall();
		e.m_sCallerUid = callerUid;
		e.m_sService = service;
		e.m_sPlace = place;
		e.m_sSummary = summary;
		e.m_sChannels = channels;
		e.m_vPosition = position;
		if (receivers)
			e.m_aReceiverIds.Copy(receivers);
		return e;
	}
}


//! A fence sold a tool (#271). Published by LF_ShopComponent.Buy after the cash was taken and the
//! tool delivered, so the serial book (#272) can stamp the tool and name its buyer.
class LF_OnFenceSale : LF_Event
{
	string m_sBuyerUid;
	string m_sPrefab;
	int m_iQuantity;
	int m_iPrice;

	static LF_OnFenceSale Create(string buyerUid, string prefab, int quantity, int price)
	{
		LF_OnFenceSale e = new LF_OnFenceSale();
		e.m_sBuyerUid = buyerUid;
		e.m_sPrefab = prefab;
		e.m_iQuantity = quantity;
		e.m_iPrice = price;
		return e;
	}
}
