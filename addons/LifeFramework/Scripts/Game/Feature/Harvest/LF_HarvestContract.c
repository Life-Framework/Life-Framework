// LF_HarvestContract.c: the harvest contract the farm board posts (#3297, harvester spike H9). One contract per
// ripe wheat field. A player takes it with their own reaper (no deposit, the reaper is on their owned list) or takes
// a rental kit: a $300 deposit buys a tractor or a UAZ469 with a reaper hitched, spawned at the board. When the
// field's cut count reaches 95 % the contract pays a cash bonus and FARMER XP per 100 kg cut since it was taken; the
// deposit comes back when the kit is returned at the board. One taker per field. A kit that is not returned within
// the timeout is lost: the deposit stays with the farm, the kit is deleted and the field is free again.
//
// Server only. Contracts are session state; the kit is a job vehicle, never an owned record, and is kept out of the
// world save. Only the open deposits are saved (section "harvest_deposits"): a restart with a kit out pays its
// deposit back into the taker's bank on the next boot, once. Posting a contract on a board prop and the board
// prompt are a follow-up card.

enum LF_EHarvestContract
{
	OK,
	NOT_SERVER,
	NO_FIELD,
	NO_USER,
	NOT_RIPE,		//!< The field is already cut to the completion line, nothing is left to contract
	ALREADY_POSTED,
	NOT_POSTED,
	ALREADY_TAKEN,	//!< One taker per field
	NO_REAPER,		//!< The own-reaper route, but the character owns none
	NO_CASH,		//!< The deposit could not be paid
	KIT_FAILED,		//!< The rental vehicle or its reaper did not spawn; the deposit is back
	NOT_TAKER,
	NO_KIT,			//!< The taker rented no kit, or it is gone
	KIT_FAR,		//!< The kit is not at the board
	NOT_DONE,		//!< Settle: the cut count has not reached the line yet
	PAID,			//!< Settle: the bonus and XP were paid
	FORFEIT			//!< Settle: the timeout passed, the deposit stays with the farm
}

enum LF_EHarvestContractState
{
	OPEN,
	TAKEN,
	DONE
}

//------------------------------------------------------------------------------------------------
//! One posted contract. Server memory only.
class LF_HarvestContractRow
{
	string m_sField;
	EntityID m_FieldId = EntityID.INVALID;
	LF_EHarvestContractState m_eState = LF_EHarvestContractState.OPEN;
	string m_sTaker;
	EntityID m_TakerBody = EntityID.INVALID;
	bool m_bRental;
	int m_iDeposit;
	EntityID m_Kit = EntityID.INVALID;
	int m_iTakenAt;
	int m_iStartCut;
	int m_iBonus;
	int m_iXp;
}

//------------------------------------------------------------------------------------------------
class LF_HarvestContract
{
	static const string FEATURE = "Harvest";

	static const int DEPOSIT = 300; // lf-fixed: the card's rental deposit (#3297)
	static const int BONUS = 40; // lf-fixed: the card's completion bonus (#3297)
	//! FARMER XP for every whole 100 kg cut while the contract is taken; as much as a fruit pick (5) is worth.
	static const int XP_PER_100KG = 5; // lf-fixed: tuning of the contract, same unit as the xpAwards rows
	//! The contract is complete when this share of the field's cells is cut.
	static const int COMPLETE_PERCENT = 95; // lf-fixed: the card's completion line (#3297)
	//! Seconds a taker keeps the contract before it lapses and a rented kit is forfeit.
	static const int TIMEOUT_SECONDS = 1800; // lf-fixed: the card's timeout; a 60 x 40 m field takes a few minutes
	//! How often the server looks at taken contracts.
	static const int SWEEP_MS = 5000; // lf-fixed: a settle cadence, not gameplay
	//! A kit with someone seated is never deleted with them: they are put out and the delete waits and tries again.
	static const int KIT_RETRY_MS = 5000; // lf-fixed: retry cadence, not gameplay
	static const int KIT_RETRIES = 60; // lf-fixed: five minutes of retries before the kit is left to the restart

	static const string REASON_DEPOSIT = "harvest-deposit";
	static const string REASON_BONUS = "harvest-bonus";

	protected static ref map<string, ref LF_HarvestContractRow> s_mRows;
	protected static bool s_bSweeping;
	//! Every deposit token this session took or refunded: a saved deposit with one of these is not paid back again.
	protected static ref array<string> s_aSeenDeposits;
	//! Kits whose delete waits for a seat to clear, and the retries each has left.
	protected static ref array<EntityID> s_aPendingKits;
	protected static ref array<int> s_aPendingTries;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_HarvestContractRow> Rows()
	{
		if (!s_mRows)
			s_mRows = new map<string, ref LF_HarvestContractRow>();

		return s_mRows;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<string> SeenDeposits()
	{
		if (!s_aSeenDeposits)
			s_aSeenDeposits = new array<string>();

		return s_aSeenDeposits;
	}

	//------------------------------------------------------------------------------------------------
	//! One deposit's identity across a restart: taker, field and the second it was taken.
	protected static string DepositToken(LF_HarvestContractRow row)
	{
		return row.m_sTaker + "|" + row.m_sField + "|" + row.m_iTakenAt.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: every deposit still held (a rental kit is out).
	static array<ref LF_HarvestDepositRow> ExportDeposits()
	{
		array<ref LF_HarvestDepositRow> rows = new array<ref LF_HarvestDepositRow>();
		foreach (string id, LF_HarvestContractRow row : Rows())
		{
			if (row && row.m_bRental && row.m_iDeposit > 0 && !row.m_sTaker.IsEmpty())
				rows.Insert(LF_HarvestDepositRow.Create(row.m_sTaker, DepositToken(row), row.m_iDeposit));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: pays saved deposits back into the takers' bank accounts. Their kits went with the last
	//! session, so nothing can be returned any more. A deposit this session took, settled or already refunded is
	//! skipped, so a second load (or a re-apply to a live session) pays nothing twice.
	//! \return How many deposits were paid back.
	static int RefundSavedDeposits(notnull array<ref LF_HarvestDepositRow> rows)
	{
		int refunded;
		foreach (LF_HarvestDepositRow row : rows)
		{
			if (!row || row.m_sKey.IsEmpty() || row.m_sToken.IsEmpty() || row.m_iAmount <= 0)
				continue;

			if (SeenDeposits().Contains(row.m_sToken))
				continue;

			int amount = LF_Utils.MinInt(row.m_iAmount, DEPOSIT);
			LF_BankAccount account = LF_ATMManager.GetOrCreate(row.m_sKey);
			if (!account)
			{
				LF_Debug.Error(FEATURE, string.Format("restart refund of a $%1 deposit failed: no bank account for %2", amount, row.m_sKey));
				continue;
			}

			SeenDeposits().Insert(row.m_sToken);
			account.Deposit(amount, REASON_DEPOSIT + "-restart-refund");
			refunded++;
			LF_Debug.Info(FEATURE, string.Format("restart refund: the $%1 deposit of a rental kit out at the restart is back in %2's bank", amount, row.m_sKey));
		}

		return refunded;
	}

	//------------------------------------------------------------------------------------------------
	protected static string FieldKey(LF_WheatField field)
	{
		return field.GetID().ToString();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_HarvestContractRow RowOf(LF_WheatField field)
	{
		if (!field)
			return null;

		LF_HarvestContractRow row;
		if (!Rows().Find(FieldKey(field), row))
			return null;

		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! The rental vehicle: the Rally tractor when the taker's client says its licence covers it, else the UAZ469.
	//! DlcManager answers for the machine it runs on, so the answer is the client's hint (LF_VehicleDlcGate), and a
	//! client that lies gets a tractor vanilla refuses it the driver seat of.
	static ResourceName KitPrefab(bool dlcOwned)
	{
		if (dlcOwned)
			return LF_VehicleDlcGate.Z7211;

		return LF_VehicleCatalog.UAZ469;
	}

	//------------------------------------------------------------------------------------------------
	//! A field for a log line: its name, or where it stands (placed fields are often unnamed).
	protected static string Name(LF_WheatField field)
	{
		string name = field.GetName();
		if (!name.IsEmpty())
			return name;

		return "field at " + field.GetOrigin().ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Whole percent of the field's cells that are cut (rounded down).
	static int CutPercent(LF_WheatField field)
	{
		if (!field || field.CellCount() <= 0)
			return 0;

		return field.CutCount() * 100 / field.CellCount();
	}

	//------------------------------------------------------------------------------------------------
	//! A field is ripe while it is below the completion line.
	static bool IsRipe(LF_WheatField field)
	{
		return field && field.CellCount() > 0 && CutPercent(field) < COMPLETE_PERCENT;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsPosted(LF_WheatField field)
	{
		return RowOf(field) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The state of the field's contract, OPEN when it has none.
	static LF_EHarvestContractState StateOf(LF_WheatField field)
	{
		LF_HarvestContractRow row = RowOf(field);
		if (!row)
			return LF_EHarvestContractState.OPEN;

		return row.m_eState;
	}

	//------------------------------------------------------------------------------------------------
	//! The record key of whoever holds the field's contract, "" when nobody does.
	static string TakerOf(LF_WheatField field)
	{
		LF_HarvestContractRow row = RowOf(field);
		if (!row || row.m_eState == LF_EHarvestContractState.OPEN)
			return "";

		return row.m_sTaker;
	}

	//------------------------------------------------------------------------------------------------
	//! The rental kit's vehicle, or null (none rented, returned, or deleted).
	static IEntity KitOf(LF_WheatField field)
	{
		LF_HarvestContractRow row = RowOf(field);
		if (!row || row.m_Kit == EntityID.INVALID)
			return null;

		return GetGame().GetWorld().FindEntityByID(row.m_Kit);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the board posts the field's contract. One per field; a field already cut to the line has nothing to harvest.
	static LF_EHarvestContract Post(LF_WheatField field)
	{
		if (!Replication.IsServer())
			return Refuse(LF_EHarvestContract.NOT_SERVER, "contracts resolve on the server only");

		if (!field)
			return Refuse(LF_EHarvestContract.NO_FIELD, "no field");

		if (IsPosted(field))
			return Refuse(LF_EHarvestContract.ALREADY_POSTED, "the field has its contract: " + Name(field));

		if (!IsRipe(field))
			return Refuse(LF_EHarvestContract.NOT_RIPE, string.Format("%1 is %2 %% cut, nothing to harvest", Name(field), CutPercent(field)));

		LF_HarvestContractRow row = new LF_HarvestContractRow();
		row.m_sField = FieldKey(field);
		row.m_FieldId = field.GetID();
		Rows().Set(row.m_sField, row);
		LF_Debug.Info(FEATURE, string.Format("harvest contract posted for %1 (%2 cells)", Name(field), field.CellCount()));
		return LF_EHarvestContract.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `user` takes the field's contract. `rental` false uses the taker's own reaper (they must own one and
	//! hitch it at the rack); true charges the deposit and spawns a kit at `kitAt` with a reaper hitched.
	//! \param[in] dlcOwned The taker's client answer for the Rally tractor (LF_VehicleDlcGate.LocalOwns).
	//! \param[out] kit The rental vehicle (null on the own-reaper route or a refusal).
	static LF_EHarvestContract Take(LF_WheatField field, IEntity user, bool rental, bool dlcOwned, vector kitAt, vector kitAngles, out IEntity kit)
	{
		kit = null;
		if (!Replication.IsServer())
			return Refuse(LF_EHarvestContract.NOT_SERVER, "contracts resolve on the server only");

		if (!field)
			return Refuse(LF_EHarvestContract.NO_FIELD, "no field");

		string key = LF_Utils.GetPlayerUID(user);
		if (!user || key.IsEmpty())
			return Refuse(LF_EHarvestContract.NO_USER, "no taker");

		LF_HarvestContractRow row = RowOf(field);
		if (!row)
			return Refuse(LF_EHarvestContract.NOT_POSTED, "no contract for " + Name(field));

		if (row.m_eState != LF_EHarvestContractState.OPEN)
			return Refuse(LF_EHarvestContract.ALREADY_TAKEN, string.Format("%1 already holds the contract of %2", row.m_sTaker, Name(field)));

		if (!IsRipe(field))
			return Refuse(LF_EHarvestContract.NOT_RIPE, Name(field) + " is cut already");

		if (!rental && !LF_OwnedImplements.Owns(key, LF_OwnedImplements.ID_REAPER))
			return Refuse(LF_EHarvestContract.NO_REAPER, key + " owns no reaper");

		int deposit;
		if (rental)
		{
			int removed = LF_MoneyUtils.RemoveAmount(user, DEPOSIT, REASON_DEPOSIT);
			if (removed != DEPOSIT)
			{
				if (removed > 0)
					LF_MoneyUtils.AddAmount(user, removed, REASON_DEPOSIT + "-rollback");

				return Refuse(LF_EHarvestContract.NO_CASH, string.Format("%1 could not pay the %2 deposit", key, LF_MoneyFormat.FormatMoney(DEPOSIT)));
			}

			deposit = DEPOSIT;
			kit = SpawnKit(dlcOwned, kitAt, kitAngles);
			if (!kit)
			{
				LF_MoneyUtils.AddAmount(user, deposit, REASON_DEPOSIT + "-rollback");
				return Refuse(LF_EHarvestContract.KIT_FAILED, "the rental kit did not spawn, the deposit is back");
			}
		}

		row.m_eState = LF_EHarvestContractState.TAKEN;
		row.m_sTaker = key;
		row.m_TakerBody = user.GetID();
		row.m_bRental = rental;
		row.m_iDeposit = deposit;
		row.m_iTakenAt = System.GetUnixTime();
		row.m_iStartCut = field.CutCount();
		if (kit)
			row.m_Kit = kit.GetID();

		if (deposit > 0)
			SeenDeposits().Insert(DepositToken(row));

		LF_Debug.Info(FEATURE, string.Format("harvest contract of %1 taken by %2 (rental %3, deposit $%4, %5 cells already cut)", Name(field), key, rental, deposit, row.m_iStartCut));
		StartSweep();
		return LF_EHarvestContract.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: looks at the field's contract once. Taken and the cut count at the line: pays the bonus and XP to the
	//! taker (once) and marks it done. Taken for longer than the timeout: the kit is deleted, the deposit is kept and
	//! the contract is gone. Anything else changes nothing.
	//! \return PAID, FORFEIT, NOT_DONE (still running), or NOT_POSTED / NOT_TAKEN.
	static LF_EHarvestContract Settle(LF_WheatField field, int nowUnix)
	{
		LF_HarvestContractRow row = RowOf(field);
		if (!row)
			return LF_EHarvestContract.NOT_POSTED;

		if (row.m_eState == LF_EHarvestContractState.OPEN)
			return LF_EHarvestContract.NOT_DONE;

		// A done contract whose kit never came back (the taker left) lapses the same way, so it cannot hold the field.
		if (row.m_eState == LF_EHarvestContractState.DONE)
		{
			if (nowUnix - row.m_iTakenAt > TIMEOUT_SECONDS)
				return Forfeit(field, row);

			return LF_EHarvestContract.NOT_DONE;
		}

		if (CutPercent(field) >= COMPLETE_PERCENT)
			return Pay(field, row);

		if (nowUnix - row.m_iTakenAt > TIMEOUT_SECONDS)
			return Forfeit(field, row);

		return LF_EHarvestContract.NOT_DONE;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EHarvestContract Pay(LF_WheatField field, LF_HarvestContractRow row)
	{
		// The state moves before any money does, so a second settle finds nothing to pay.
		row.m_eState = LF_EHarvestContractState.DONE;
		int cutCells = field.CutCount() - row.m_iStartCut;
		if (cutCells < 0)
			cutCells = 0;

		int kg = LF_WheatFieldRules.YieldKg(cutCells);
		row.m_iBonus = BONUS;
		IEntity body = GetGame().GetWorld().FindEntityByID(row.m_TakerBody);
		int xp = LF_JobLevels.AddXp(row.m_sTaker, LF_JobLevels.JOB_FARMER, kg / 100 * XP_PER_100KG, "harvest-contract");
		row.m_iXp = xp;
		if (body)
		{
			int paid = LF_MoneyUtils.GiveCash(body, BONUS, REASON_BONUS);
			if (paid != BONUS)
				LF_Debug.Warn(FEATURE, string.Format("harvest bonus short: paid %1 of %2 to %3", paid, BONUS, row.m_sTaker));

			LF_Utils.NotifyEntity(body, "#LF-Harvest_ContractPaid", "#LF-Harvest_ContractTitle", 4.0, BONUS.ToString());
		}
		else
		{
			LF_Debug.Warn(FEATURE, string.Format("harvest bonus not paid: %1 has no body in the world", row.m_sTaker));
			row.m_iBonus = 0;
		}

		LF_Debug.Info(FEATURE, string.Format("harvest contract of %1 done by %2: %3 kg cut, bonus $%4, +%5 FARMER xp", Name(field), row.m_sTaker, kg, row.m_iBonus, xp));
		if (!row.m_bRental)
			Rows().Remove(row.m_sField);

		return LF_EHarvestContract.PAID;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EHarvestContract Forfeit(LF_WheatField field, LF_HarvestContractRow row)
	{
		LF_Debug.Info(FEATURE, string.Format("harvest contract of %1 lapsed: %2 keeps no deposit back ($%3), the kit is taken away", Name(field), row.m_sTaker, row.m_iDeposit));
		DeleteKit(row);
		IEntity body = GetGame().GetWorld().FindEntityByID(row.m_TakerBody);
		if (body)
			LF_Utils.NotifyEntity(body, "#LF-Harvest_ContractLapsed", "#LF-Harvest_ContractTitle", 4.0);

		Rows().Remove(row.m_sField);
		return LF_EHarvestContract.FORFEIT;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the taker brings the rental kit back to `at` (the board). The kit must stand within the rack's reach.
	//! Its deposit comes back and the kit is deleted; the grain left in the hopper is lost. A contract that is done
	//! is then closed; one that is not goes back to open for the next taker.
	//! \param[out] refunded The deposit paid back.
	static LF_EHarvestContract ReturnKit(LF_WheatField field, IEntity user, vector at, out int refunded)
	{
		refunded = 0;
		LF_HarvestContractRow row = RowOf(field);
		if (!row)
			return LF_EHarvestContract.NOT_POSTED;

		if (!user || row.m_eState == LF_EHarvestContractState.OPEN || row.m_sTaker != LF_Utils.GetPlayerUID(user))
			return Refuse(LF_EHarvestContract.NOT_TAKER, "only the taker returns the kit of " + Name(field));

		IEntity kit = KitOf(field);
		if (!row.m_bRental || !kit)
			return Refuse(LF_EHarvestContract.NO_KIT, "no rental kit to return for " + Name(field));

		if (vector.Distance(kit.GetOrigin(), at) > LF_OwnedImplements.RACK_REACH_M)
			return Refuse(LF_EHarvestContract.KIT_FAR, "the kit is not at the board");

		// The deposit is zeroed before it is paid: a second return finds nothing to refund.
		int deposit = row.m_iDeposit;
		row.m_iDeposit = 0;
		int paid = LF_MoneyUtils.AddAmount(user, deposit, REASON_DEPOSIT + "-refund");
		if (paid != deposit)
		{
			LF_Debug.Warn(FEATURE, string.Format("deposit refund short: %1 of %2 to %3", paid, deposit, row.m_sTaker));
		}

		refunded = paid;
		DeleteKit(row);
		LF_Utils.NotifyEntity(user, "#LF-Harvest_ContractRefund", "#LF-Harvest_ContractTitle", 4.0, paid.ToString());
		LF_Debug.Info(FEATURE, string.Format("rental kit of %1 returned by %2, deposit $%3 back", Name(field), row.m_sTaker, paid));
		if (row.m_eState == LF_EHarvestContractState.DONE)
		{
			Rows().Remove(row.m_sField);
			return LF_EHarvestContract.OK;
		}

		row.m_eState = LF_EHarvestContractState.OPEN;
		row.m_sTaker = "";
		row.m_TakerBody = EntityID.INVALID;
		row.m_bRental = false;
		return LF_EHarvestContract.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the rental vehicle at the board, keeps it out of the world save (a job vehicle is never an owned
	//! record, so a restart leaves no copy) and hitches a reaper to it. Null (logged) when any step fails.
	protected static IEntity SpawnKit(bool dlcOwned, vector at, vector angles)
	{
		ResourceName prefab = KitPrefab(dlcOwned);
		IEntity vehicle = LF_Utils.SpawnEntityPrefab(prefab, at, angles);
		// A server without Rally content cannot spawn the Z7211: the DLC owner gets the UAZ469 rather than no rental.
		if (!vehicle && dlcOwned)
		{
			LF_Debug.Warn(FEATURE, "the Rally tractor did not spawn (no Rally content on this server); the rental kit is the UAZ469");
			prefab = KitPrefab(false);
			vehicle = LF_Utils.SpawnEntityPrefab(prefab, at, angles);
		}

		if (!vehicle)
		{
			LF_Debug.Error(FEATURE, "rental kit vehicle did not spawn: " + prefab);
			return null;
		}

		LF_VehicleWorldSave.Hold(vehicle);
		string reason;
		IEntity reaper = LF_ImplementHitch.Hitch(vehicle, reason);
		// The reaper belongs to the kit too: a restart must not leave it standing at the board.
		if (reaper)
			LF_VehicleWorldSave.Hold(reaper);

		if (!reaper)
		{
			LF_Debug.Error(FEATURE, "rental kit has no reaper (" + reason + "): " + prefab);
			RplComponent.DeleteRplEntity(vehicle, false);
			return null;
		}

		return vehicle;
	}

	//------------------------------------------------------------------------------------------------
	protected static void DeleteKit(LF_HarvestContractRow row)
	{
		if (row.m_Kit == EntityID.INVALID)
			return;

		EntityID kitId = row.m_Kit;
		row.m_Kit = EntityID.INVALID;
		RemoveKit(kitId, KIT_RETRIES);
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes a kit with its reaper. Deleting a vehicle deletes whoever sits in it, so anyone seated is put out
	//! first; a seat that stays taken keeps the kit and the delete is tried again later (RetryKits).
	//! \return True when the kit is gone or was never there.
	static bool RemoveKit(EntityID kitId, int triesLeft)
	{
		IEntity kit = GetGame().GetWorld().FindEntityByID(kitId);
		if (!kit)
			return true;

		if (LF_Garage.HasOccupants(kit))
			LF_Garage.StopAndEject(kit);

		if (LF_Garage.HasOccupants(kit))
		{
			if (triesLeft <= 0)
			{
				LF_Debug.Error(FEATURE, "rental kit still has someone seated after every retry; it stays until the restart");
				return true;
			}

			LF_Debug.Warn(FEATURE, "rental kit has someone seated; the delete waits for the seat to clear");
			if (!s_aPendingKits)
			{
				s_aPendingKits = new array<EntityID>();
				s_aPendingTries = new array<int>();
			}

			s_aPendingKits.Insert(kitId);
			s_aPendingTries.Insert(triesLeft - 1);
			if (s_aPendingKits.Count() == 1)
				GetGame().GetCallqueue().CallLater(RetryKits, KIT_RETRY_MS, false);

			return false;
		}

		if (LF_ImplementHitch.FindHitched(kit))
			LF_ImplementHitch.Unhitch(kit);

		RplComponent.DeleteRplEntity(kit, false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Tries every kit that still had someone seated once more.
	static void RetryKits()
	{
		if (!s_aPendingKits)
			return;

		array<EntityID> kits = {};
		array<int> tries = {};
		kits.Copy(s_aPendingKits);
		tries.Copy(s_aPendingTries);
		s_aPendingKits.Clear();
		s_aPendingTries.Clear();
		for (int i = 0; i < kits.Count(); i++)
		{
			RemoveKit(kits[i], tries[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The server loop: settles every taken contract each SWEEP_MS, stops when none is left.
	protected static void StartSweep()
	{
		if (s_bSweeping)
			return;

		s_bSweeping = true;
		GetGame().GetCallqueue().CallLater(Sweep, SWEEP_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	static void Sweep()
	{
		int now = System.GetUnixTime();
		array<string> ids = {};
		foreach (string id, LF_HarvestContractRow row : Rows())
		{
			ids.Insert(id);
		}

		bool anyTaken;
		foreach (string fieldId : ids)
		{
			LF_HarvestContractRow current;
			if (!Rows().Find(fieldId, current) || current.m_eState == LF_EHarvestContractState.OPEN)
				continue;

			LF_WheatField field = LF_WheatField.Cast(GetGame().GetWorld().FindEntityByID(current.m_FieldId));
			if (!field)
			{
				LF_Debug.Warn(FEATURE, "harvest contract dropped: its field is gone (" + fieldId + ")");
				DeleteKit(current);
				Rows().Remove(fieldId);
				continue;
			}

			Settle(field, now);
			if (Rows().Find(fieldId, current) && current.m_eState != LF_EHarvestContractState.OPEN)
				anyTaken = true;
		}

		if (!anyTaken && s_bSweeping)
		{
			GetGame().GetCallqueue().Remove(Sweep);
			s_bSweeping = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EHarvestContract Refuse(LF_EHarvestContract why, string detail)
	{
		LF_Debug.Warn(FEATURE, string.Format("harvest contract refused (%1): %2", typename.EnumToString(LF_EHarvestContract, why), detail));
		return why;
	}

	//------------------------------------------------------------------------------------------------
	//! The toast line for a refusal; empty when nothing should show.
	static string RefusalKey(LF_EHarvestContract why)
	{
		switch (why)
		{
			case LF_EHarvestContract.ALREADY_TAKEN: return "#LF-Harvest_ContractTaken";
			case LF_EHarvestContract.NO_REAPER: return "#LF-Harvest_ContractNoReaper";
			case LF_EHarvestContract.NO_CASH: return "#LF-Harvest_ContractNoCash";
			case LF_EHarvestContract.KIT_FAR: return "#LF-Harvest_ContractKitFar";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		GetGame().GetCallqueue().Remove(Sweep);
		GetGame().GetCallqueue().Remove(RetryKits);
		s_mRows = null;
		s_bSweeping = false;
		s_aSeenDeposits = null;
		s_aPendingKits = null;
		s_aPendingTries = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_HarvestContract's statics (LF_StaticReset finds it by reflection).
class LF_HarvestContract_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_HarvestContract";
	}

	override void Apply()
	{
		LF_HarvestContract.ResetStatics();
	}
}
