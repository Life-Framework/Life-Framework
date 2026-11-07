// red-proof: the server was not booted. tools\cli test was not run, so no failing suite line was observed.
// tier: LOGIC
class EL_Test_Roadmap_Escort : EL_Test
{
	override string GetName()
	{
		return "roadmap/escort";
	}

	override void Run(EL_TestContext ctx)
	{
		string officerUid = "roadmap-escort-officer";
		string offDutyUid = "roadmap-escort-offduty";
		string prisonerUid = "roadmap-escort-prisoner";
		string cleanUid = "roadmap-escort-clean";
		string bystanderUid = "roadmap-escort-bystander";
		string vehicleId = "roadmap-escort-van";
		string customId = "roadmap-escort-custom";
		string carId = "roadmap-escort-car";
		vector cell = "48 0 64";
		vector away = "200 0 200";

		LF_Jail.Configure(60, 300, 1, 8);
		LF_Jail.SetCellOrigin(cell);

		EL_PlayerAccount officer = EL_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(EL_Faction.POLICE);
		officer.SetOnDuty(true);
		EL_PlayerAccount offDuty = EL_PlayerAccountManager.GetOrCreate(offDutyUid);
		offDuty.SetFaction(EL_Faction.POLICE);
		offDuty.SetOnDuty(false);
		EL_PlayerAccount clean = EL_PlayerAccountManager.GetOrCreate(cleanUid);
		clean.SetWantedLevel(0);
		EL_PlayerAccount prisoner = EL_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(2);

		LF_JailEscort.RegisterVehicle(vehicleId, true);
		LF_JailEscort.OfferSeat(vehicleId, 0, true, 0.5, "");
		LF_JailEscort.OfferSeat(vehicleId, 1, false, 1.0, bystanderUid);
		LF_JailEscort.OfferSeat(vehicleId, 2, false, 5.0, "");
		LF_JailEscort.OfferSeat(vehicleId, 3, false, 3.0, "");

		LF_JailEscort.RegisterVehicle(customId, false);
		LF_JailEscort.OfferSeat(customId, 1, false, 1.0, "");

		LF_JailEscort.RegisterVehicle(carId, true);
		LF_JailEscort.OfferSeat(carId, 0, true, 1.0, "");

		ctx.Equal(3, LF_JailEscort.NearestFreePassengerSlot(vehicleId), "the nearest free seat is the closer passenger, not the driver or the occupied seat");
		ctx.Equal(-1, LF_JailEscort.NearestFreePassengerSlot(carId), "a driver seat is not a passenger seat");
		ctx.Equal(1, LF_JailEscort.NearestFreePassengerSlot(customId), "the custom vehicle still has a free passenger seat");

		ctx.False(LF_JailEscort.Load(officerUid, cleanUid, vehicleId), "a player who is not wanted is refused");
		ctx.True(LF_JailEscort.GetLastLog().Contains("not wanted"), "that refusal is logged");
		ctx.Equal(0, clean.GetWantedLevel(), "a refused load leaves a clean player clean");
		ctx.Equal(-1, LF_JailEscort.GetSeat(cleanUid), "a refused player is not seated");
		ctx.False(LF_Jail.IsServing(cleanUid), "a refused player is not serving");

		ctx.False(LF_JailEscort.Load(offDutyUid, prisonerUid, vehicleId), "an off-duty officer is refused");
		ctx.True(LF_JailEscort.GetLastLog().Contains("not an on-duty officer"), "the off-duty refusal is logged");
		ctx.False(LF_JailEscort.Load(officerUid, prisonerUid, customId), "a vehicle that is not base-game is refused");
		ctx.True(LF_JailEscort.GetLastLog().Contains("base-game"), "the base-game refusal is logged");
		ctx.False(LF_JailEscort.Load(officerUid, prisonerUid, carId), "a vehicle with no free passenger seat is refused");
		ctx.True(LF_JailEscort.GetLastLog().Contains("no free passenger seat"), "the full-vehicle refusal is logged");
		ctx.Equal(-1, LF_JailEscort.GetSeat(prisonerUid), "a refused load does not seat the prisoner");
		ctx.False(LF_Jail.IsServing(prisonerUid), "a refused load does not start the sentence");

		ctx.True(LF_JailEscort.Escort(officerUid, prisonerUid), "an on-duty officer escorts a wanted player");
		ctx.Equal(-1, LF_JailEscort.GetSeat(prisonerUid), "escort on foot is not a vehicle seat");
		ctx.False(LF_Jail.IsServing(prisonerUid), "escort on foot does not start the sentence");
		ctx.Equal(2, prisoner.GetWantedLevel(), "escort does not change wanted");

		ctx.True(LF_JailEscort.Load(officerUid, prisonerUid, vehicleId), "the officer loads the wanted player into the vehicle");
		ctx.Equal(3, LF_JailEscort.GetSeat(prisonerUid), "the prisoner sits in the nearest free non-driver seat");
		ctx.False(LF_Jail.IsServing(prisonerUid), "entering the car does not start the sentence");
		ctx.EqualFloat(0, LF_Jail.GetRemaining(prisonerUid), 0.01, "entering the car leaves no remaining sentence");
		ctx.Equal(2, prisoner.GetWantedLevel(), "the seat does not change wanted");

		int steps = LF_JailEscort.GetTickSteps();
		LF_JailEscort.Tick(1.0);
		int after = steps + 1;
		ctx.Equal(after, LF_JailEscort.GetTickSteps(), "the escort tick runs while the prisoner is in the vehicle");
		ctx.False(LF_Jail.IsServing(prisonerUid), "the escort tick does not start the sentence");
		ctx.EqualFloat(0, LF_Jail.GetRemaining(prisonerUid), 0.01, "the escort tick does not add sentence time");

		ctx.False(LF_JailEscort.Arrive(officerUid, prisonerUid, away), "arrival away from the cell does not start the sentence");
		ctx.Equal(3, LF_JailEscort.GetSeat(prisonerUid), "the prisoner stays seated until the cell");
		ctx.False(LF_Jail.IsServing(prisonerUid), "a missed cell is not a sentence");

		ctx.True(LF_JailEscort.TakeOut(officerUid, prisonerUid), "the officer can take the prisoner out before the cell");
		ctx.Equal(-1, LF_JailEscort.GetSeat(prisonerUid), "take-out clears the seat");
		ctx.False(LF_Jail.IsServing(prisonerUid), "take-out away from the cell does not start the sentence");

		ctx.True(LF_JailEscort.Load(officerUid, prisonerUid, vehicleId), "the officer loads the prisoner again");
		ctx.Equal(3, LF_JailEscort.GetSeat(prisonerUid), "the second load uses the same nearest free passenger seat");
		ctx.True(LF_JailEscort.Arrive(officerUid, prisonerUid, cell), "arrival at the cell starts the sentence");
		ctx.Equal(-1, LF_JailEscort.GetSeat(prisonerUid), "the cell is not a vehicle seat");
		ctx.True(LF_Jail.IsServing(prisonerUid), "the prisoner is serving after arrival");
		ctx.Equal(120, LF_Jail.SentenceSeconds(2), "the arrival sentence is the config length");
		ctx.EqualFloat(120, LF_Jail.GetRemaining(prisonerUid), 0.01, "remaining time starts when they reach the cell");
		ctx.Equal(2, prisoner.GetWantedLevel(), "arrival does not clear wanted");

		ctx.False(LF_Jail.ReleaseEarly(prisonerUid), "early release is refused");
		ctx.EqualFloat(120, LF_Jail.GetRemaining(prisonerUid), 0.01, "a refused release keeps the remaining time");
		ctx.True(LF_Jail.IsServing(prisonerUid), "a refused release keeps the prisoner serving");
		ctx.Equal(2, prisoner.GetWantedLevel(), "a refused release does not change wanted");

		ctx.EqualFloat(100, LF_Jail.Advance(prisonerUid, 20), 0.01, "time passes after arrival");
		LF_JailRecord saved = LF_Jail.Export(prisonerUid);
		ctx.NotNull(saved, "export keeps the remaining sentence");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualFloat(100, saved.m_fRemaining, 0.01, "the saved record is the remaining sentence");
		ctx.True(saved.m_bServing, "the saved record is still serving");
		ctx.EqualFloat(80, LF_Jail.Advance(prisonerUid, 20), 0.01, "live time moves before the reconnect");
		ctx.True(LF_Jail.Apply(saved), "reconnect applies the saved sentence");
		ctx.EqualFloat(100, LF_Jail.GetRemaining(prisonerUid), 0.01, "a reconnect keeps the remaining sentence");
		ctx.True(LF_Jail.IsServing(prisonerUid), "a reconnect keeps the prisoner serving");
		ctx.Equal(2, prisoner.GetWantedLevel(), "a reconnect does not change wanted");
	}
}
