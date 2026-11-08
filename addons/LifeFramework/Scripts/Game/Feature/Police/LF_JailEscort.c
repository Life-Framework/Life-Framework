// Ported from LNGCLifeRP_Public (MIT), author LNGCGenetic.
// Copyright (c) 2026 LNGCGenetic
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//------------------------------------------------------------------------------------------------
//! Vehicle escort. An on-duty officer seats a wanted player in the nearest free
//! non-driver seat of a base-game vehicle. The sentence does not start in the
//! car. It starts when that vehicle arrives at the placed cell. A player who is
//! not wanted is refused and logged. Early release stays on LF_Jail. The
//! remaining sentence is the jail record, so a reconnect keeps it.
//!
//! Seats are stand-in ids. This class does not spawn a vehicle and does not
//! store an IEntity.
//------------------------------------------------------------------------------------------------
class LF_JailEscortVehicle : Managed
{
	string m_sVehicleId;
	bool m_bBaseGame;
}

//------------------------------------------------------------------------------------------------
class LF_JailEscortSeat : Managed
{
	string m_sVehicleId;
	int m_iSlot;
	bool m_bDriver;
	float m_fDistance;
	string m_sOccupantUid;
}

//------------------------------------------------------------------------------------------------
class LF_JailEscortHold : Managed
{
	string m_sOfficerUid;
	string m_sPrisonerUid;
	string m_sVehicleId;
	int m_iSlot;
	bool m_bInVehicle;
	bool m_bActive;
}

//------------------------------------------------------------------------------------------------
class LF_JailEscort
{
	protected static const float ARRIVAL_RADIUS = 8;

	protected static ref map<string, ref LF_JailEscortVehicle> s_mVehicles;
	protected static ref map<string, ref LF_JailEscortSeat> s_mSeats;
	protected static ref map<string, ref LF_JailEscortHold> s_mHolds;
	protected static string s_sLastLog = "";
	protected static int s_iTickSteps;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_JailEscortVehicle> Vehicles()
	{
		if (!s_mVehicles)
			s_mVehicles = new map<string, ref LF_JailEscortVehicle>();

		return s_mVehicles;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_JailEscortSeat> Seats()
	{
		if (!s_mSeats)
			s_mSeats = new map<string, ref LF_JailEscortSeat>();

		return s_mSeats;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_JailEscortHold> Holds()
	{
		if (!s_mHolds)
			s_mHolds = new map<string, ref LF_JailEscortHold>();

		return s_mHolds;
	}

	//------------------------------------------------------------------------------------------------
	static string GetLastLog()
	{
		return s_sLastLog;
	}

	//------------------------------------------------------------------------------------------------
	static int GetTickSteps()
	{
		return s_iTickSteps;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers a vehicle id. A base-game vehicle can take a prisoner. Any other id is refused.
	static void RegisterVehicle(string vehicleId, bool baseGame)
	{
		if (vehicleId.IsEmpty())
		{
			FailNote("escort vehicle refused: empty id");
			return;
		}

		LF_JailEscortVehicle vehicle = new LF_JailEscortVehicle();
		vehicle.m_sVehicleId = vehicleId;
		vehicle.m_bBaseGame = baseGame;
		Vehicles().Set(vehicleId, vehicle);
		int baseFlag = 0;
		if (baseGame)
			baseFlag = 1;
		EL_Debug.Log("Jail", string.Format("escort vehicle %1 base=%2", vehicleId, baseFlag));
	}

	//------------------------------------------------------------------------------------------------
	//! Offers one seat. Distance is meters from the officer's door. A lower distance is nearer.
	//! An empty occupant uid is a free seat. The driver seat is never a prisoner seat.
	static void OfferSeat(string vehicleId, int slot, bool driver, float distanceMeters, string occupantUid)
	{
		if (vehicleId.IsEmpty())
		{
			FailNote("escort seat refused: empty vehicle id");
			return;
		}

		if (slot < 0)
		{
			FailNote("escort seat refused: negative slot");
			return;
		}

		if (distanceMeters < 0)
		{
			EL_Debug.Error("Jail", "escort seat clamped: negative distance");
			distanceMeters = 0;
		}

		LF_JailEscortSeat seat = new LF_JailEscortSeat();
		seat.m_sVehicleId = vehicleId;
		seat.m_iSlot = slot;
		seat.m_bDriver = driver;
		seat.m_fDistance = distanceMeters;
		seat.m_sOccupantUid = occupantUid;
		Seats().Set(SeatKey(vehicleId, slot), seat);
	}

	//------------------------------------------------------------------------------------------------
	//! Lowest distance among free seats that are not the driver. -1 when none are free.
	static int NearestFreePassengerSlot(string vehicleId)
	{
		int bestSlot = -1;
		float bestDistance = 0;
		bool found = false;

		foreach (string key, LF_JailEscortSeat seat : Seats())
		{
			if (!seat)
				continue;

			if (key.IsEmpty())
				continue;

			if (seat.m_sVehicleId != vehicleId)
				continue;

			if (seat.m_bDriver)
				continue;

			if (!seat.m_sOccupantUid.IsEmpty())
				continue;

			bool closer = false;
			if (!found)
				closer = true;
			else if (seat.m_fDistance < bestDistance)
				closer = true;
			else if (seat.m_fDistance == bestDistance && seat.m_iSlot < bestSlot)
				closer = true;

			if (!closer)
				continue;

			found = true;
			bestDistance = seat.m_fDistance;
			bestSlot = seat.m_iSlot;
		}

		return bestSlot;
	}

	//------------------------------------------------------------------------------------------------
	//! On-foot escort. Does not seat the prisoner and does not start the sentence.
	static bool Escort(string officerUid, string prisonerUid)
	{
		if (!Gate(officerUid, prisonerUid))
			return false;

		if (LF_Jail.IsServing(prisonerUid))
		{
			Note(string.Format("escort refused: %1 is already serving", prisonerUid));
			return false;
		}

		if (IsSeated(prisonerUid))
		{
			Note(string.Format("escort refused: %1 is already seated", prisonerUid));
			return false;
		}

		LF_JailEscortHold hold = HoldFor(prisonerUid);
		hold.m_sOfficerUid = officerUid;
		hold.m_sVehicleId = "";
		hold.m_iSlot = -1;
		hold.m_bInVehicle = false;
		hold.m_bActive = true;
		Note(string.Format("escort started %1", prisonerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Seats a wanted player. The sentence stays unstarted until Arrive.
	static bool Load(string officerUid, string prisonerUid, string vehicleId)
	{
		if (!Gate(officerUid, prisonerUid))
			return false;

		if (vehicleId.IsEmpty())
		{
			FailNote("escort refused: vehicle id is empty");
			return false;
		}

		if (LF_Jail.IsServing(prisonerUid))
		{
			Note(string.Format("escort refused: %1 is already serving", prisonerUid));
			return false;
		}

		if (IsSeated(prisonerUid))
		{
			Note(string.Format("escort refused: %1 is already seated", prisonerUid));
			return false;
		}

		LF_JailEscortVehicle vehicle = Vehicles().Get(vehicleId);
		if (!vehicle || !vehicle.m_bBaseGame)
		{
			Note(string.Format("escort refused: %1 is not a base-game vehicle", vehicleId));
			return false;
		}

		int slot = NearestFreePassengerSlot(vehicleId);
		if (slot < 0)
		{
			Note(string.Format("escort refused: %1 has no free passenger seat", vehicleId));
			return false;
		}

		LF_JailEscortSeat seat = FindSeat(vehicleId, slot);
		if (!seat)
		{
			Note(string.Format("escort refused: %1 has no free passenger seat", vehicleId));
			return false;
		}

		if (seat.m_bDriver)
		{
			Note(string.Format("escort refused: %1 has no free passenger seat", vehicleId));
			return false;
		}

		if (!seat.m_sOccupantUid.IsEmpty())
		{
			Note(string.Format("escort refused: %1 has no free passenger seat", vehicleId));
			return false;
		}

		seat.m_sOccupantUid = prisonerUid;
		LF_JailEscortHold hold = HoldFor(prisonerUid);
		hold.m_sOfficerUid = officerUid;
		hold.m_sVehicleId = vehicleId;
		hold.m_iSlot = slot;
		hold.m_bInVehicle = true;
		hold.m_bActive = true;
		Note(string.Format("escort seated %1 vehicle=%2 slot=%3", prisonerUid, vehicleId, slot));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Leaves the car. Does not start the sentence.
	static bool TakeOut(string officerUid, string prisonerUid)
	{
		if (officerUid.IsEmpty() || prisonerUid.IsEmpty())
		{
			FailNote("escort take-out refused: officer or prisoner uid is empty");
			return false;
		}

		if (!OnDuty(officerUid))
		{
			Note(string.Format("escort refused: %1 is not an on-duty officer", officerUid));
			return false;
		}

		if (!IsSeated(prisonerUid))
		{
			Note(string.Format("escort take-out refused: %1 is not in a vehicle", prisonerUid));
			return false;
		}

		LF_JailEscortHold hold = Holds().Get(prisonerUid);
		if (!hold || hold.m_sOfficerUid != officerUid)
		{
			Note(string.Format("escort take-out refused: %1 is not the escorting officer", officerUid));
			return false;
		}

		ClearSeat(prisonerUid);
		hold.m_bInVehicle = false;
		hold.m_sVehicleId = "";
		hold.m_iSlot = -1;
		hold.m_bActive = true;
		Note(string.Format("escort unloaded %1", prisonerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the sentence only when the seated prisoner is at the cell.
	static bool Arrive(string officerUid, string prisonerUid, vector position)
	{
		if (!Gate(officerUid, prisonerUid))
			return false;

		if (!IsSeated(prisonerUid))
		{
			Note(string.Format("escort arrival refused: %1 is not in a vehicle", prisonerUid));
			return false;
		}

		if (!LF_Jail.HasCell())
		{
			Note("escort arrival refused: no cell is placed");
			return false;
		}

		if (!IsAtCell(position))
		{
			Note(string.Format("escort arrival refused: %1 is not at the cell", prisonerUid));
			return false;
		}

		if (!LF_Jail.Arrest(officerUid, prisonerUid, null))
		{
			Note(string.Format("escort arrival refused: sentence did not start for %1", prisonerUid));
			return false;
		}

		ClearSeat(prisonerUid);
		LF_JailEscortHold hold = Holds().Get(prisonerUid);
		if (hold)
		{
			hold.m_bInVehicle = false;
			hold.m_sVehicleId = "";
			hold.m_iSlot = -1;
			hold.m_bActive = false;
		}

		Note(string.Format("escort arrived %1", prisonerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsAtCell(vector position)
	{
		if (!LF_Jail.HasCell())
			return false;

		return vector.Distance(position, LF_Jail.GetCellOrigin()) <= ARRIVAL_RADIUS;
	}

	//------------------------------------------------------------------------------------------------
	//! Follows an active escort. Does not start or advance the sentence.
	static void Tick(float elapsedSeconds)
	{
		if (elapsedSeconds < 0)
			return;

		foreach (string prisonerUid, LF_JailEscortHold hold : Holds())
		{
			if (!hold)
				continue;

			if (prisonerUid.IsEmpty())
				continue;

			if (!hold.m_bActive)
				continue;

			s_iTickSteps = s_iTickSteps + 1;
		}
	}

	//------------------------------------------------------------------------------------------------
	static int GetSeat(string prisonerUid)
	{
		LF_JailEscortSeat seat = FindOccupantSeat(prisonerUid);
		if (!seat)
			return -1;

		return seat.m_iSlot;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsSeated(string prisonerUid)
	{
		return GetSeat(prisonerUid) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	static string GetOnFootPrisoner(string officerUid)
	{
		foreach (string prisonerUid, LF_JailEscortHold hold : Holds())
		{
			if (!hold)
				continue;

			if (!hold.m_bActive)
				continue;

			if (hold.m_bInVehicle)
				continue;

			if (hold.m_sOfficerUid != officerUid)
				continue;

			return prisonerUid;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	static string GetSeatedPrisoner(string officerUid)
	{
		foreach (string prisonerUid, LF_JailEscortHold hold : Holds())
		{
			if (!hold)
				continue;

			if (!hold.m_bActive)
				continue;

			if (!hold.m_bInVehicle)
				continue;

			if (hold.m_sOfficerUid != officerUid)
				continue;

			return prisonerUid;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Gate(string officerUid, string prisonerUid)
	{
		if (officerUid.IsEmpty() || prisonerUid.IsEmpty())
		{
			FailNote("escort refused: officer or prisoner uid is empty");
			return false;
		}

		EL_PlayerAccountManager accounts = EL_PlayerAccountManager.GetInstance();
		if (!accounts)
		{
			FailNote("escort refused: no account manager");
			return false;
		}

		EL_PlayerAccount officer = accounts.GetAccount(officerUid);
		if (!EL_PoliceUtils.IsOnDutyOfficer(officer))
		{
			Note(string.Format("escort refused: %1 is not an on-duty officer", officerUid));
			return false;
		}

		EL_PlayerAccount prisoner = accounts.GetAccount(prisonerUid);
		if (!prisoner)
		{
			FailNote(string.Format("escort refused: no account for %1", prisonerUid));
			return false;
		}

		if (prisoner.GetWantedLevel() <= 0)
		{
			Note(string.Format("escort refused: %1 is not wanted", prisonerUid));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool OnDuty(string officerUid)
	{
		EL_PlayerAccountManager accounts = EL_PlayerAccountManager.GetInstance();
		if (!accounts)
			return false;

		return EL_PoliceUtils.IsOnDutyOfficer(accounts.GetAccount(officerUid));
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_JailEscortHold HoldFor(string prisonerUid)
	{
		LF_JailEscortHold hold = Holds().Get(prisonerUid);
		if (!hold)
		{
			hold = new LF_JailEscortHold();
			hold.m_sPrisonerUid = prisonerUid;
			hold.m_iSlot = -1;
			Holds().Set(prisonerUid, hold);
		}

		return hold;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_JailEscortSeat FindSeat(string vehicleId, int slot)
	{
		if (slot < 0)
			return null;

		return Seats().Get(SeatKey(vehicleId, slot));
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_JailEscortSeat FindOccupantSeat(string prisonerUid)
	{
		if (prisonerUid.IsEmpty())
			return null;

		foreach (string key, LF_JailEscortSeat seat : Seats())
		{
			if (!seat)
				continue;

			if (key.IsEmpty())
				continue;

			if (seat.m_sOccupantUid == prisonerUid)
				return seat;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ClearSeat(string prisonerUid)
	{
		LF_JailEscortSeat seat = FindOccupantSeat(prisonerUid);
		if (!seat)
			return;

		seat.m_sOccupantUid = "";
	}

	//------------------------------------------------------------------------------------------------
	protected static string SeatKey(string vehicleId, int slot)
	{
		return string.Format("%1|%2", vehicleId, slot);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Note(string message)
	{
		s_sLastLog = message;
		EL_Debug.Log("Jail", message);
	}

	//------------------------------------------------------------------------------------------------
	protected static void FailNote(string message)
	{
		s_sLastLog = message;
		EL_Debug.Error("Jail", message);
	}
}
