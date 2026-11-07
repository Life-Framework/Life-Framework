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
//! On-foot escort. The sentence does not start here. The server re-checks the
//! officer and the wanted level inside LF_JailEscort.Escort.
//------------------------------------------------------------------------------------------------
class LF_JailEscortAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!GetOwner() || !user)
			return false;

		if (GetOwner() == user)
			return false;

		return EL_PoliceUtils.IsOnDutyOfficer(EL_PoliceUtils.GetAccountForEntity(user));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!CanBeShownScript(user))
		{
			SetCannotPerformReason("#EL-Jail_NotOfficer");
			return false;
		}

		EL_PlayerAccount suspect = EL_PoliceUtils.GetAccountForEntity(GetOwner());
		if (!suspect || suspect.GetWantedLevel() <= 0)
		{
			SetCannotPerformReason("#EL-Jail_NotWanted");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!pOwnerEntity || !pUserEntity)
			return;

		string officerUid = EL_Utils.GetPlayerUid(pUserEntity);
		string prisonerUid = EL_Utils.GetPlayerUid(pOwnerEntity);
		if (officerUid.IsEmpty() || prisonerUid.IsEmpty())
		{
			EL_Debug.Error("Jail", "escort action refused: officer or suspect has no account id");
			return;
		}

		LF_JailEscort.Escort(officerUid, prisonerUid);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Escort_Grab";
		return true;
	}
}
