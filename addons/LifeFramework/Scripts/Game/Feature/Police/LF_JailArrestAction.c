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
//! On-duty officer action on the suspect, in the world. Sentence length comes
//! from the jail config and the suspect's wanted level. The server re-checks
//! every request inside LF_Jail.Arrest.
//------------------------------------------------------------------------------------------------
class LF_JailArrestAction : ScriptedUserAction
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

		if (!LF_Systems.IsEnabled(LF_Jail.SYSTEM))
			return false;

		// One entry in the player interaction menu: same reach as the rest of the list.
		if (!LF_PlayerInteraction.CanOffer(user, GetOwner()))
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

		// The cell registers on the server only; a client leaves this check to the server.
		if (Replication.IsServer() && !LF_Jail.HasCell())
		{
			SetCannotPerformReason("#EL-Jail_NoCell");
			return false;
		}

		if (vector.Distance(GetOwner().GetOrigin(), user.GetOrigin()) > LF_PoliceConfig.ArrestReach())
		{
			SetCannotPerformReason("#EL-Jail_NotWanted");
			return false;
		}

		// Offer checks run on the client too, where accounts do not exist: read the replicated
		// state (the account cache on the server). PerformAction re-checks on the server.
		if (LF_AccountStateBits.GetWantedLevel(LF_AccountReplication.GetState(GetOwner())) <= 0)
		{
			SetCannotPerformReason("#EL-Jail_NotWanted");
			return false;
		}

		if (!LF_PoliceRanks.CanArrestRank(LF_AccountStateBits.GetPoliceRank(LF_AccountReplication.GetState(user))))
		{
			SetCannotPerformReason("#EL-Rank_ArrestDenied");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_JailArrestAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_EInteractionRefusal refusal = LF_PlayerInteraction.CheckPair(pUserEntity, pOwnerEntity);
		if (refusal != LF_EInteractionRefusal.NONE)
		{
			LF_Debug.Warn("Jail", "arrest action refused: " + typename.EnumToString(LF_EInteractionRefusal, refusal));
			return;
		}

		LF_PlayerAccount officerAccount = LF_PoliceUtils.GetAccountForEntity(pUserEntity);
		if (!LF_PoliceRanks.AuthorizeArrest(officerAccount))
			return;

		string officerUid = LF_Utils.GetPlayerUid(pUserEntity);
		string prisonerUid = LF_Utils.GetPlayerUid(pOwnerEntity);
		if (officerUid.IsEmpty() || prisonerUid.IsEmpty())
		{
			LF_Debug.Error("Jail", "arrest action refused: officer or suspect has no account id");
			return;
		}

		LF_Jail.Arrest(officerUid, prisonerUid, pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Jail_Arrest";
		return true;
	}
}
