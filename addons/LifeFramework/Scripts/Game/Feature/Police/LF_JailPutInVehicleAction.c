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
//! Puts the escorted prisoner in a free passenger seat of this car
//! (LF_JailEscortCar.PutIn: the real seat, the escort hold, the reach check).
//! A car with no free passenger seat is refused and the officer told. The
//! sentence does not start here.
//------------------------------------------------------------------------------------------------
class LF_JailPutInVehicleAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police"))
			return false;

		if (!GetOwner() || !user)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!CanBeShownScript(user))
			return false;

		if (!LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user)))
		{
			SetCannotPerformReason(LF_Terms.Text("#EL-Jail_NotOfficer"));
			return false;
		}

		// The escort hold lives only on the server. A dedicated server never shows the officer a
		// reason from here, so the hold, the reach and the seat are judged in PerformAction, which
		// tells the officer why it refused (#3431).
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_JailPutInVehicleAction"))
			return;

		if (!Replication.IsServer())
			return;

		if (!pOwnerEntity || !pUserEntity)
			return;

		LF_JailEscortCar.PutIn(pUserEntity, pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Escort_PutIn";
		return true;
	}
}
