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
//! Puts the escorted prisoner in the nearest free non-driver seat. The vehicle
//! id is this entity's name. Seat rows come from LF_JailEscort.OfferSeat. A
//! vehicle with no free passenger seat is refused and logged. The sentence
//! does not start here.
//------------------------------------------------------------------------------------------------
class LF_JailPutInVehicleAction : ScriptedUserAction
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

		if (GetOwner().GetName().IsEmpty())
		{
			SetCannotPerformReason("#EL-Escort_NoPrisoner");
			return false;
		}

		string officerUid = EL_Utils.GetPlayerUid(user);
		if (LF_JailEscort.GetOnFootPrisoner(officerUid).IsEmpty())
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
		string prisonerUid = LF_JailEscort.GetOnFootPrisoner(officerUid);
		string vehicleId = pOwnerEntity.GetName();
		if (officerUid.IsEmpty() || prisonerUid.IsEmpty() || vehicleId.IsEmpty())
		{
			EL_Debug.Error("Jail", "escort put-in refused: officer, prisoner, or vehicle id is empty");
			return;
		}

		LF_JailEscort.Load(officerUid, prisonerUid, vehicleId);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Escort_PutIn";
		return true;
	}
}
