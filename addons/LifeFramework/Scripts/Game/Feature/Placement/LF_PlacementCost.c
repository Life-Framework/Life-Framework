//------------------------------------------------------------------------------------------------
//! What a placement costs (#2448, epic #927, refs #1030). A kind's entry in
//! Configs/Placement/LF_PlacementPolicy.conf names its cost: free, cash from the player's pockets,
//! or items used up from their inventory, and whether picking it up gives the cost back. A pack
//! overrides the prices in its own copy of the `.conf`. The server's Confirm (LF_PlacementService,
//! LF_PlacementCompositions) takes the cost once, only after every other check and the probes said
//! VALID, gives it back when the spawn then fails, and remembers a refundable charge by the placed
//! entities so the pick-up refunds it once, to the player who paid. Cash moves only through
//! LF_MoneyUtils with a reason, so the ledger sees every take and every refund (LF_OnMoneyMoved).
//! Charges live in memory: placed sets are not saved either, so a restart forgets both together.
//------------------------------------------------------------------------------------------------

enum LF_EPlacementCost
{
	FREE,
	//! Cash from the player's pockets (LF_MoneyUtils).
	CASH,
	//! Items of the entry's cost prefab used up from the player's inventory.
	ITEM
}

//------------------------------------------------------------------------------------------------
//! One kind's cost, read from its policy entry.
class LF_PlacementCostTerms
{
	string m_sKind;
	LF_EPlacementCost m_eCost;
	//! CASH: the price; ITEM: how many items.
	int m_iAmount;
	//! ITEM: the prefab used up.
	ResourceName m_sItem;
	//! Whether picking the placement up gives the cost back.
	bool m_bRefund;

	//------------------------------------------------------------------------------------------------
	//! Whether anything is taken at all: a cash or item cost of zero or less is free.
	bool IsFree()
	{
		if (m_iAmount <= 0)
			return true;

		if (m_eCost == LF_EPlacementCost.CASH)
			return false;

		if (m_eCost == LF_EPlacementCost.ITEM)
			return m_sItem.IsEmpty();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	string Describe()
	{
		if (IsFree())
			return "free";

		if (m_eCost == LF_EPlacementCost.CASH)
			return LF_MoneyFormat.FormatMoney(m_iAmount);

		return string.Format("%1 x %2", m_iAmount, LF_PlacementPolicy.KindOfPrefab(m_sItem));
	}
}

//------------------------------------------------------------------------------------------------
//! A cost that was taken: who paid, what, and (once placed) the entities a pick-up refunds it for.
class LF_PlacementCharge
{
	ref LF_PlacementCostTerms m_Terms;
	//! The payer's character uid, and their entity for a body with no player (tests, AI).
	string m_sPayerUid;
	EntityID m_PayerId = EntityID.INVALID;
	ref array<EntityID> m_aIds = {};
	//! Set once the cost went back, so nothing refunds twice.
	bool m_bRefunded;
}

//------------------------------------------------------------------------------------------------
class LF_PlacementCost
{
	static const string FEATURE = "Placement";
	//! Ledger reasons for the take and the refund.
	static const string REASON = "placement";
	static const string REASON_REFUND = "placement-refund";
	static const string TOAST_CASH = "#LF-Placement_NeedCash";
	static const string TOAST_ITEM = "#LF-Placement_NeedItem";

	//! Refundable charges still standing in the world.
	protected static ref array<ref LF_PlacementCharge> s_aCharges;
	protected static array<ref LF_PlacementCharge> LzCharges()
	{
		if (!s_aCharges)
			s_aCharges = new array<ref LF_PlacementCharge>();
		return s_aCharges;
	}

	//------------------------------------------------------------------------------------------------
	//! How many refundable charges this machine remembers (the tests read it).
	static int Count()
	{
		LzCharges();
		return s_aCharges.Count();
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		LzCharges();
		s_aCharges.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! The cost of a kind: its policy entry's, else free.
	static LF_PlacementCostTerms TermsFor(string kind)
	{
		LF_PlacementCostTerms terms = new LF_PlacementCostTerms();
		terms.m_sKind = kind;
		LF_PlacementPolicyConfig config = LF_PlacementPolicyConfig.Get();
		if (!config)
			return terms;

		LF_PlacementPolicyEntry entry = config.Find(kind);
		if (!entry)
			return terms;

		terms.m_eCost = entry.m_eCost;
		terms.m_iAmount = entry.m_iCostAmount;
		terms.m_sItem = entry.m_sCostItem;
		terms.m_bRefund = entry.m_bRefundOnPickup;
		// Cash named as an item cost still moves through LF_MoneyUtils, so the ledger sees it.
		if (terms.m_eCost == LF_EPlacementCost.ITEM && IsCash(terms.m_sItem))
		{
			LF_Debug.Warn(FEATURE, string.Format("the policy names cash (%1) as the item cost of %2; charged as cash", terms.m_sItem, kind));
			terms.m_eCost = LF_EPlacementCost.CASH;
			terms.m_sItem = "";
		}

		return terms;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsCash(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return false;

		if (LF_Utils.SamePrefab(prefab, LF_MoneyUtils.CashPrefab()))
			return true;

		foreach (ResourceName legacy : LF_MoneyUtils.LegacyCashPrefabs())
		{
			if (LF_Utils.SamePrefab(prefab, legacy))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an ITEM cost names `prefab` (the placeable or kit being placed): a config error the
	//! confirm refuses, so one item is never both the thing placed and its price.
	static bool CostIsItself(notnull LF_PlacementCostTerms terms, IEntity item)
	{
		if (!item || terms.IsFree() || terms.m_eCost != LF_EPlacementCost.ITEM)
			return false;

		return LF_Utils.SamePrefab(terms.m_sItem, LF_Utils.GetPrefabName(item));
	}

	//------------------------------------------------------------------------------------------------
	//! Pure. Whether a player holding `have` (cash, or items of the cost prefab) can pay `terms`.
	//! \return Empty when they can, else the refusal tail naming the cost; `toastKey` is its toast.
	static string Judge(notnull LF_PlacementCostTerms terms, int have, out string toastKey)
	{
		toastKey = "";
		if (terms.IsFree() || have >= terms.m_iAmount)
			return "";

		if (terms.m_eCost == LF_EPlacementCost.CASH)
		{
			toastKey = TOAST_CASH;
			return string.Format("cannot pay %1 for %2 (carries %3)", LF_MoneyFormat.FormatMoney(terms.m_iAmount), terms.m_sKind, LF_MoneyFormat.FormatMoney(LF_Utils.MaxInt(have, 0)));
		}

		toastKey = TOAST_ITEM;
		return string.Format("needs %1 x %2 for %3 (carries %4)", terms.m_iAmount, LF_PlacementPolicy.KindOfPrefab(terms.m_sItem), terms.m_sKind, LF_Utils.MaxInt(have, 0));
	}

	//------------------------------------------------------------------------------------------------
	//! What `player` carries toward `terms`: cash, or items of the cost prefab.
	static int Holding(IEntity player, notnull LF_PlacementCostTerms terms)
	{
		if (!player)
			return 0;

		if (terms.m_eCost == LF_EPlacementCost.CASH)
			return LF_MoneyUtils.GetCash(player);

		if (terms.m_eCost == LF_EPlacementCost.ITEM && !terms.m_sItem.IsEmpty())
			return LF_InventoryUtils.GetAmount(player, terms.m_sItem);

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes `terms` from `player`, all or nothing. A free kind returns a charge that took
	//! nothing. A player who cannot pay, or a take that comes up short (the inventory changed under
	//! it), takes nothing: a short take is given back.
	//! \return the charge, or null with `why` and `toastKey` set.
	static LF_PlacementCharge Take(IEntity player, notnull LF_PlacementCostTerms terms, out string why, out string toastKey)
	{
		why = "";
		toastKey = "";
		LF_PlacementCharge charge = new LF_PlacementCharge();
		charge.m_Terms = terms;
		if (terms.IsFree())
			return charge;

		if (!Replication.IsServer() || !player)
		{
			why = "the cost is taken on the server from a player";
			return null;
		}

		why = Judge(terms, Holding(player, terms), toastKey);
		if (!why.IsEmpty())
			return null;

		int taken;
		if (terms.m_eCost == LF_EPlacementCost.CASH)
			taken = LF_MoneyUtils.RemoveCash(player, terms.m_iAmount, REASON);
		else
			taken = LF_InventoryUtils.RemoveAmount(player, terms.m_sItem, terms.m_iAmount);

		if (taken < terms.m_iAmount)
		{
			int back = GiveBack(player, terms, taken);
			if (back < taken)
				LF_Debug.Error(FEATURE, string.Format("short take for %1 from %2: took %3, gave back only %4; %5 owed", terms.m_sKind, LF_PoliceRanks.ResolveUid(player), taken, back, taken - back));

			why = string.Format("took only %1 of %2 for %3; given back", taken, terms.Describe(), terms.m_sKind);
			if (terms.m_eCost == LF_EPlacementCost.CASH)
				toastKey = TOAST_CASH;
			else
				toastKey = TOAST_ITEM;

			return null;
		}

		charge.m_sPayerUid = LF_Utils.GetPlayerUID(player);
		charge.m_PayerId = player.GetID();
		LF_Debug.Info(FEATURE, string.Format("took %1 from %2 for %3", terms.Describe(), LF_PoliceRanks.ResolveUid(player), terms.m_sKind));
		return charge;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gives a charge's cost back to `player` (the spawn failed, or the pick-up). Once only.
	static void Refund(IEntity player, LF_PlacementCharge charge, string why)
	{
		if (!charge || charge.m_bRefunded)
			return;

		charge.m_bRefunded = true;
		if (charge.m_Terms.IsFree())
			return;

		int given = GiveBack(player, charge.m_Terms, charge.m_Terms.m_iAmount);
		if (given < charge.m_Terms.m_iAmount)
		{
			LF_Debug.Error(FEATURE, string.Format("refund of %1 for %2 to %3 gave back only %4 (%5); the rest is owed (inventory full)", charge.m_Terms.Describe(), charge.m_Terms.m_sKind, LF_PoliceRanks.ResolveUid(player), given, why));
			return;
		}

		LF_Debug.Info(FEATURE, string.Format("refunded %1 to %2 for %3 (%4)", charge.m_Terms.Describe(), LF_PoliceRanks.ResolveUid(player), charge.m_Terms.m_sKind, why));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a placement stood up with `charge` taken; a refundable cost is remembered by its entities.
	static void Keep(LF_PlacementCharge charge, notnull array<EntityID> ids)
	{
		if (!charge || charge.m_bRefunded || charge.m_Terms.IsFree() || !charge.m_Terms.m_bRefund)
			return;

		foreach (EntityID id : ids)
			charge.m_aIds.Insert(id);

		Prune();
		LzCharges();
		s_aCharges.Insert(charge);
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets charges whose placement is gone without a pick-up (a roadblock that expired, a strip
	//! that was destroyed): no refund, so the list never outgrows what stands in the world.
	//! \return how many it forgot.
	static int Prune()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		int forgotten;
		LzCharges();
		for (int i = s_aCharges.Count() - 1; i >= 0; i--)
		{
			LF_PlacementCharge charge = LzCharges()[i];
			bool standing;
			foreach (EntityID id : charge.m_aIds)
			{
				if (world.FindEntityByID(id))
				{
					standing = true;
					break;
				}
			}

			if (standing)
				continue;

			LF_Debug.Info(FEATURE, string.Format("forgot the charge for %1 paid by %2: the placement is gone without a pick-up, no refund", charge.m_Terms.m_sKind, charge.m_sPayerUid));
			LzCharges();
			s_aCharges.Remove(i);
			forgotten++;
		}

		return forgotten;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `player` picked up the placement `id` belongs to. Its refundable charge is forgotten
	//! and given back once, to the player who paid; another player's pick-up refunds nothing.
	//! \return whether a refund was given.
	static bool OnPickedUp(IEntity player, EntityID id)
	{
		if (!Replication.IsServer())
			return false;

		LF_PlacementCharge charge;
		foreach (LF_PlacementCharge candidate : LzCharges())
		{
			if (candidate.m_aIds.Find(id) != -1)
			{
				charge = candidate;
				break;
			}
		}

		if (!charge)
			return false;

		LzCharges();
		s_aCharges.RemoveItem(charge);
		if (!IsPayer(player, charge))
		{
			LF_Debug.Info(FEATURE, string.Format("%1 picked up %2 paid by %3: no refund", LF_PoliceRanks.ResolveUid(player), charge.m_Terms.m_sKind, charge.m_sPayerUid));
			return false;
		}

		Refund(player, charge, "picked up");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The payer is the same character (uid), or, for a body with no player, the same entity.
	protected static bool IsPayer(IEntity player, notnull LF_PlacementCharge charge)
	{
		if (!player)
			return false;

		if (!charge.m_sPayerUid.IsEmpty())
			return LF_Utils.GetPlayerUID(player) == charge.m_sPayerUid;

		return player.GetID() == charge.m_PayerId;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives `amount` of the terms' cost to `player`; returns how much went back.
	protected static int GiveBack(IEntity player, notnull LF_PlacementCostTerms terms, int amount)
	{
		if (!player || amount <= 0)
			return 0;

		if (terms.m_eCost == LF_EPlacementCost.CASH)
			return LF_MoneyUtils.AddCash(player, amount, REASON_REFUND, "", true);

		return LF_InventoryUtils.AddAmount(player, terms.m_sItem, amount, true);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PlacementCost's statics (LF_StaticReset finds it by reflection).
class LF_PlacementCost_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PlacementCost";
	}

	override void Apply()
	{
		LF_PlacementCost.ResetStatics();
	}
}
