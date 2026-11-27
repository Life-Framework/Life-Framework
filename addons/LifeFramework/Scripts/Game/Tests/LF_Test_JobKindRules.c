// red-proof: with `RedProof()` on the rules are not asked, so every answer keeps its wrong default and the delivery, cargo,
// toast-key and client-run checks fail.

//! jobs/kind-rules: which jobs are delivery work (LF_CivilianJob). Only parcel, fuel and taxi are delivery jobs: that is the
//! list saved as a sign-up and restored after a restart, so a job added to or dropped from it changes who keeps their job. Parcel
//! and fuel carry cargo, a taxi does not. The sign-up toast and the run-start toast name the next step per job (the courier
//! text is the fallback for any other job), only the parcel run says where cargo goes, and a client offers "Complete the run"
//! for exactly the delivery jobs.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_JobKindRules : LF_Test
{
	override string GetName()
	{
		return "jobs/kind-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		bool parcelDelivery = false;
		bool fuelDelivery = false;
		bool taxiDelivery = false;
		int otherDelivery = -1;
		bool parcelCargo = false;
		bool fuelCargo = false;
		bool taxiCargo = true;
		bool farmerCargo = true;
		string parcelSign = "-";
		string fuelSign = "-";
		string taxiSign = "-";
		string farmerSign = "-";
		string unemployedSign = "-";
		string parcelRun = "-";
		string fuelRun = "-";
		string taxiRun = "-";
		string farmerRun = "-";
		bool parcelClient = false;
		bool taxiClient = false;
		bool minerClient = true;
		bool unemployedClient = true;
		if (!RedProof())
		{
			parcelDelivery = LF_CivilianJob.IsDeliveryJob(LF_EJobType.PARCEL);
			fuelDelivery = LF_CivilianJob.IsDeliveryJob(LF_EJobType.FUEL_DELIVERY);
			taxiDelivery = LF_CivilianJob.IsDeliveryJob(LF_EJobType.TAXI);
			otherDelivery = 0;
			array<LF_EJobType> others = {LF_EJobType.UNEMPLOYED, LF_EJobType.FARMER, LF_EJobType.MINER, LF_EJobType.LUMBERJACK, LF_EJobType.MEDIC, LF_EJobType.SMELTING, LF_EJobType.SALVAGE, LF_EJobType.POLICE};
			foreach (LF_EJobType other : others)
			{
				if (LF_CivilianJob.IsDeliveryJob(other))
					otherDelivery++;
			}

			parcelCargo = LF_CivilianJob.HasCargo(LF_EJobType.PARCEL);
			fuelCargo = LF_CivilianJob.HasCargo(LF_EJobType.FUEL_DELIVERY);
			taxiCargo = LF_CivilianJob.HasCargo(LF_EJobType.TAXI);
			farmerCargo = LF_CivilianJob.HasCargo(LF_EJobType.FARMER);
			parcelSign = LF_CivilianJob.SignedUpKey(LF_EJobType.PARCEL);
			fuelSign = LF_CivilianJob.SignedUpKey(LF_EJobType.FUEL_DELIVERY);
			taxiSign = LF_CivilianJob.SignedUpKey(LF_EJobType.TAXI);
			farmerSign = LF_CivilianJob.SignedUpKey(LF_EJobType.FARMER);
			unemployedSign = LF_CivilianJob.SignedUpKey(LF_EJobType.UNEMPLOYED);
			parcelRun = LF_CivilianJob.RunStartedKey(LF_EJobType.PARCEL);
			fuelRun = LF_CivilianJob.RunStartedKey(LF_EJobType.FUEL_DELIVERY);
			taxiRun = LF_CivilianJob.RunStartedKey(LF_EJobType.TAXI);
			farmerRun = LF_CivilianJob.RunStartedKey(LF_EJobType.FARMER);
			parcelClient = LF_CivilianJob.ClientRunOpen(LF_EJobType.PARCEL);
			taxiClient = LF_CivilianJob.ClientRunOpen(LF_EJobType.TAXI);
			minerClient = LF_CivilianJob.ClientRunOpen(LF_EJobType.MINER);
			unemployedClient = LF_CivilianJob.ClientRunOpen(LF_EJobType.UNEMPLOYED);
		}

		ctx.True(parcelDelivery, "parcel is a delivery job");
		ctx.True(fuelDelivery, "fuel delivery is a delivery job");
		ctx.True(taxiDelivery, "taxi is a delivery job");
		ctx.Equal(0, otherDelivery, "no other job (farmer, miner, lumberjack, medic, smelter, salvage, police, unemployed) is delivery work");
		ctx.True(parcelCargo, "a parcel run carries cargo");
		ctx.True(fuelCargo, "a fuel run carries cargo");
		ctx.False(taxiCargo, "a taxi carries a passenger, not cargo");
		ctx.False(farmerCargo, "a farmer has no cargo run");
		ctx.EqualStr("#LF-Job_SignedUp_Courier", parcelSign, "a courier is told the courier step");
		ctx.EqualStr("#LF-Job_SignedUp_Fuel", fuelSign, "a fuel hauler is told the fuel step");
		ctx.EqualStr("#LF-Job_SignedUp_Taxi", taxiSign, "a taxi driver is told the taxi step");
		ctx.EqualStr("#LF-Job_SignedUp_Courier", farmerSign, "any other job falls back to the courier text");
		ctx.EqualStr("#LF-Job_SignedUp_Courier", unemployedSign, "no job falls back to the courier text");
		ctx.EqualStr("#LF-Job_RunStarted_CourierDeposit", parcelRun, "a parcel run says where the cargo goes");
		ctx.EqualStr("", fuelRun, "a fuel run has no run-start text");
		ctx.EqualStr("", taxiRun, "a taxi run has no run-start text");
		ctx.EqualStr("", farmerRun, "a job with no run has no run-start text");
		ctx.True(parcelClient, "a courier's client offers Complete the run");
		ctx.True(taxiClient, "a taxi driver's client offers Complete the run");
		ctx.False(minerClient, "a miner's client has no run to complete");
		ctx.False(unemployedClient, "an unemployed player's client has no run to complete");
	}
}
