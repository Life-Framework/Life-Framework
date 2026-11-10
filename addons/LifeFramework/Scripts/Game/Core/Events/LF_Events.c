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

//! A player went down but is not dead (LF_Downed). m_sDownerUid is empty when no player did it.
//! The murder and the hospital wake come later, with the real death (LF_OnPlayerKilled).
class LF_OnPlayerDowned : LF_Event
{
	string m_sPlayerUid;
	string m_sDownerUid;
	//! Where the player went down.
	vector m_vPosition;

	static LF_OnPlayerDowned Create(string playerUid, string downerUid, vector position)
	{
		LF_OnPlayerDowned e = new LF_OnPlayerDowned();
		e.m_sPlayerUid = playerUid;
		e.m_sDownerUid = downerUid;
		e.m_vPosition = position;
		return e;
	}
}

//! A downed player was dragged, carried, dropped or loaded into a vehicle (LF_Carry, #261).
//! m_sKind is "drag", "carry", "drop" or "load".
class LF_OnPlayerCarried : LF_Event
{
	string m_sPlayerUid;
	string m_sCarrierUid;
	string m_sKind;
	vector m_vPosition;

	static LF_OnPlayerCarried Create(string playerUid, string carrierUid, string kind, vector position)
	{
		LF_OnPlayerCarried e = new LF_OnPlayerCarried();
		e.m_sPlayerUid = playerUid;
		e.m_sCarrierUid = carrierUid;
		e.m_sKind = kind;
		e.m_vPosition = position;
		return e;
	}
}

//! A medic on duty brought a downed player back (LF_Revive, #259). The downed state is gone and the
//! bleed-out clock stopped by the time this is published.
class LF_OnPlayerRevived : LF_Event
{
	string m_sPlayerUid;
	string m_sMedicUid;
	//! Where the player was revived.
	vector m_vPosition;

	static LF_OnPlayerRevived Create(string playerUid, string medicUid, vector position)
	{
		LF_OnPlayerRevived e = new LF_OnPlayerRevived();
		e.m_sPlayerUid = playerUid;
		e.m_sMedicUid = medicUid;
		e.m_vPosition = position;
		return e;
	}
}

//! A medic put a downed patient in a hospital bed (LF_BedDelivery, #262). The patient is already
//! revived (LF_OnPlayerRevived is published first) and the reduced revive bill is written.
class LF_OnPatientDelivered : LF_Event
{
	string m_sPlayerUid;
	string m_sMedicUid;
	//! Bill number written for the patient, 0 when none (an NPC).
	int m_iBillNumber;
	vector m_vPosition;

	static LF_OnPatientDelivered Create(string playerUid, string medicUid, int billNumber, vector position)
	{
		LF_OnPatientDelivered e = new LF_OnPatientDelivered();
		e.m_sPlayerUid = playerUid;
		e.m_sMedicUid = medicUid;
		e.m_iBillNumber = billNumber;
		e.m_vPosition = position;
		return e;
	}
}

//! First aid bought a downed player more time (LF_FirstAid, #257). m_sHelperUid is empty when
//! the helper is not a player.
class LF_OnFirstAid : LF_Event
{
	string m_sPlayerUid;
	string m_sHelperUid;
	//! "bandage", "tourniquet" or "saline".
	string m_sKind;
	//! Bleed-out seconds actually added (the cap may have trimmed it).
	float m_fSeconds;
	vector m_vPosition;

	static LF_OnFirstAid Create(string playerUid, string helperUid, string kind, float seconds, vector position)
	{
		LF_OnFirstAid e = new LF_OnFirstAid();
		e.m_sPlayerUid = playerUid;
		e.m_sHelperUid = helperUid;
		e.m_sKind = kind;
		e.m_fSeconds = seconds;
		e.m_vPosition = position;
		return e;
	}
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

//! A zone was added, changed, or removed (LF_Zones). m_sRule is "safe", "lawless" or "normal".
class LF_OnZoneChanged : LF_Event
{
	string m_sZone;
	string m_sRule;
	bool m_bRemoved;

	static LF_OnZoneChanged Create(string zone, string rule, bool removed)
	{
		LF_OnZoneChanged e = new LF_OnZoneChanged();
		e.m_sZone = zone;
		e.m_sRule = rule;
		e.m_bRemoved = removed;
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
	//! Suspect description snapshot when the call is a crime report (LF_Dispatch.Report); empty for a witness call.
	string m_sClothing;
	string m_sVehicle;
	//! On-duty officers when the call was placed, when that differs from the receiver count; -1 means use the receivers.
	int m_iOnDuty = -1;

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

//------------------------------------------------------------------------------------------------
//! A responder took a service call (#251). Published by LF_ServiceTake.ServerTake after the server
//! validated that the taker is on duty for the service and the call had room.
class LF_OnServiceCallTaken : LF_Event
{
	string m_sCallerUid;
	string m_sService;
	string m_sTakerUid;
	string m_sPlace;
	string m_sSummary;
	//! Takers so far, this one included.
	int m_iTakers;

	static LF_OnServiceCallTaken Create(string callerUid, string service, string takerUid, string place, string summary, int takers)
	{
		LF_OnServiceCallTaken e = new LF_OnServiceCallTaken();
		e.m_sCallerUid = callerUid;
		e.m_sService = service;
		e.m_sTakerUid = takerUid;
		e.m_sPlace = place;
		e.m_sSummary = summary;
		e.m_iTakers = takers;
		return e;
	}
}

//------------------------------------------------------------------------------------------------
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

//! A player held another up: weapon in hand, hands-up call, in range (LF_HoldUp). Intent only; no
//! rule changes because of it. The incident log records it (kind "holdup").
class LF_OnHoldUp : LF_Event
{
	string m_sActorUid;
	string m_sTargetUid;
	//! Where the actor stood.
	vector m_vPosition;

	static LF_OnHoldUp Create(string actorUid, string targetUid, vector position)
	{
		LF_OnHoldUp e = new LF_OnHoldUp();
		e.m_sActorUid = actorUid;
		e.m_sTargetUid = targetUid;
		e.m_vPosition = position;
		return e;
	}
}

//! An administrator did something: a Game Master editor action, #kick, #ban, !wadd, !handled
//! (LF_AdminAction). The incident log records it (kind "admin") so moderators are accountable.
class LF_OnAdminAction : LF_Event
{
	string m_sAdminUid;
	//! The player acted on; empty when the action has none.
	string m_sTargetUid;
	string m_sAction;
	vector m_vPosition;

	static LF_OnAdminAction Create(string adminUid, string targetUid, string action, vector position)
	{
		LF_OnAdminAction e = new LF_OnAdminAction();
		e.m_sAdminUid = adminUid;
		e.m_sTargetUid = targetUid;
		e.m_sAction = action;
		e.m_vPosition = position;
		return e;
	}
}

//! A player kept a raised weapon on another player for the aim window (LF_AimIntent). Intent
//! only; no rule changes because of it. The incident log records it (kind "aim").
class LF_OnAimedAt : LF_Event
{
	string m_sActorUid;
	string m_sTargetUid;
	//! Where the actor stood.
	vector m_vPosition;

	static LF_OnAimedAt Create(string actorUid, string targetUid, vector position)
	{
		LF_OnAimedAt e = new LF_OnAimedAt();
		e.m_sActorUid = actorUid;
		e.m_sTargetUid = targetUid;
		e.m_vPosition = position;
		return e;
	}
}

//! A police scanner picked up a service call (#253). Published by LF_Scanner for each scanner
//! carrier who is not a receiver of the call, so a criminal listening in leaves a trace on the bus.
class LF_OnScannerHeard : LF_Event
{
	string m_sListenerUid;
	string m_sService;
	string m_sSummary;
	string m_sPlace;

	static LF_OnScannerHeard Create(string listenerUid, string service, string summary, string place)
	{
		LF_OnScannerHeard e = new LF_OnScannerHeard();
		e.m_sListenerUid = listenerUid;
		e.m_sService = service;
		e.m_sSummary = summary;
		e.m_sPlace = place;
		return e;
	}
}

//! A witness described a suspect (LF_Witness): from the payphone or to an officer in person.
//! Carries what the witness could see, never the suspect's name.
class LF_OnWitnessStatement : LF_Event
{
	int m_iReport;
	//! The player witness; empty for a town NPC.
	string m_sWitnessUid;
	bool m_bNpc;
	//! "payphone" or "officer".
	string m_sChannel;
	string m_sClothing;
	string m_sVehicle;
	string m_sPlateFragment;

	static LF_OnWitnessStatement Create(int report, string witnessUid, bool npc, string channel, string clothing, string vehicle, string plateFragment)
	{
		LF_OnWitnessStatement e = new LF_OnWitnessStatement();
		e.m_iReport = report;
		e.m_sWitnessUid = witnessUid;
		e.m_bNpc = npc;
		e.m_sChannel = channel;
		e.m_sClothing = clothing;
		e.m_sVehicle = vehicle;
		e.m_sPlateFragment = plateFragment;
		return e;
	}
}

//------------------------------------------------------------------------------------------------
//! A robber tried a vault with a fence tool (#273). Published by LF_Bank.Rob. m_sOutcome is
//! "opened", "jammed", or "refused" (no tool in hand). m_sSerial is the tool's fence serial, empty
//! for a refusal; the evidence locker reads the same serial off the bagged drill.
class LF_OnVaultBreach : LF_Event
{
	string m_sPlayerUid;
	string m_sVaultId;
	string m_sTool;
	string m_sSerial;
	string m_sOutcome;

	static LF_OnVaultBreach Create(string playerUid, string vaultId, string tool, string serial, string outcome)
	{
		LF_OnVaultBreach e = new LF_OnVaultBreach();
		e.m_sPlayerUid = playerUid;
		e.m_sVaultId = vaultId;
		e.m_sTool = tool;
		e.m_sSerial = serial;
		e.m_sOutcome = outcome;
		return e;
	}
}
