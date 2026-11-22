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
//! Detain for escort. This is not LF_JailArrestAction: it does not start the
//! sentence and it does not move the suspect to the cell.
//------------------------------------------------------------------------------------------------
class LF_JailEscortArrestAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police"))
			return false;

		if (!GetOwner() || !user)
			return false;

		if (GetOwner() == user)
			return false;

		return LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!CanBeShownScript(user))
		{
			SetCannotPerformReason(LF_Terms.Text("#EL-Jail_NotOfficer"));
			return false;
		}

		// Offer checks run on the client too, where accounts do not exist: read the replicated
		// state (the account cache on the server). PerformAction re-checks on the server.
		if (LF_AccountStateBits.GetWantedLevel(LF_AccountReplication.GetState(GetOwner())) <= 0)
		{
			SetCannotPerformReason("#EL-Jail_NotWanted");
			return false;
		}

		if (!LF_PoliceRanks.CanEscortRank(LF_AccountStateBits.GetPoliceRank(LF_AccountReplication.GetState(user))))
		{
			SetCannotPerformReason("#LF-Rank_EscortDenied");
			return false;
		}

		// Cuff first (hands up and unarmed, or downed): the escort follows the cuff, never replaces it.
		if (!LF_Restraint.IsRestrained(GetOwner()))
		{
			SetCannotPerformReason("#LF-JailBook_CuffFirst");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_JailEscortArrestAction"))
			return;

		if (!Replication.IsServer())
			return;

		if (!pOwnerEntity || !pUserEntity)
			return;

		// Reach and another officer's hold are judged on the server (#2542).
		LF_JailEscort.Grab(pUserEntity, pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Escort_Action";
		return true;
	}
}
