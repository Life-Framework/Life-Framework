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
//! Takes the prisoner out of this vehicle. At the placed cell the sentence
//! starts. Anywhere else they leave the seat and the sentence stays unstarted.
//------------------------------------------------------------------------------------------------
class LF_JailTakeOutOfVehicleAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!GetOwner() || !user)
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

		string officerUid = EL_Utils.GetPlayerUid(user);
		if (LF_JailEscort.GetSeatedPrisoner(officerUid).IsEmpty())
		{
			SetCannotPerformReason("#EL-Escort_NoPrisoner");
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
		string prisonerUid = LF_JailEscort.GetSeatedPrisoner(officerUid);
		if (officerUid.IsEmpty() || prisonerUid.IsEmpty())
		{
			EL_Debug.Error("Jail", "escort take-out refused: officer or prisoner id is empty");
			return;
		}

		vector position = pOwnerEntity.GetOrigin();
		if (LF_JailEscort.IsAtCell(position))
			LF_JailEscort.Arrive(officerUid, prisonerUid, position);
		else
			LF_JailEscort.TakeOut(officerUid, prisonerUid);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Escort_TakeOut";
		return true;
	}
}
