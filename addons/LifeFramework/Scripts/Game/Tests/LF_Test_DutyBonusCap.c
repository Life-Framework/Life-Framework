// red-proof: 2026-11-23 `tools\cli test --no-build --red LF_Test_DutyBonusCap` (skipping the cadet award loop) printed
// RED PROOF OK: "both hours wait for clock-out (expected 115, got 35)"; green with the hook off.

//------------------------------------------------------------------------------------------------
//! Outcome bonuses add up to the rank's hourly cap and no further, are paid once with the shift at
//! clock-out, and a fine never reaches the officer (#4210).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DutyBonusCap : LF_Test
{
	override string GetName()
	{
		return "jobs/duty-bonus-cap";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_DutyBonus.ResetStatics();
		LF_MedicDutyBonus.ResetStatics();

		// The amounts and the cap tables.
		ctx.Equal(15, LF_DutyBonus.AmountFor(LF_DutyBonusKind.CALL_RESOLVED), "a resolved call is $15");
		ctx.Equal(10, LF_DutyBonus.AmountFor(LF_DutyBonusKind.BOOKING), "a booking is $10");
		ctx.Equal(20, LF_DutyBonus.AmountFor(LF_DutyBonusKind.STOLEN_CAR), "a stolen car is $20");
		ctx.Equal(10, LF_DutyBonus.AmountFor(LF_DutyBonusKind.REVIVE), "a revive is $10");
		ctx.Equal(15, LF_DutyBonus.AmountFor(LF_DutyBonusKind.PATIENT_DELIVERED), "a delivered patient is $15");
		ctx.Equal(100, LF_DutyBonus.PoliceCap(0), "a cadet's cap is $100");
		ctx.Equal(95, LF_DutyBonus.PoliceCap(3), "a sergeant's cap is $95");
		ctx.Equal(70, LF_DutyBonus.PoliceCap(4), "a lieutenant's cap is $70");
		ctx.Equal(70, LF_DutyBonus.PoliceCap(9), "a rank past the table keeps the last cap");
		ctx.Equal(40, LF_MedicDutyBonus.Cap(0), "an orderly's cap is $40");
		ctx.Equal(50, LF_MedicDutyBonus.Cap(1), "an EMT's cap is $50");
		ctx.Equal(40, LF_MedicDutyBonus.Cap(3), "a doctor's cap is $40");

		// A cadet resolving seven calls in an hour: 6 x $15 = $90, the seventh tops up to $100, then nothing.
		LF_PlayerAccount cadet = OnDuty("duty-bonus-cadet", LF_Faction.POLICE, 0);
		int earned = 0;
		if (!RedProof())
		{
			for (int i = 0; i < 7; i++)
				earned += LF_DutyBonus.AwardAt(cadet, LF_DutyBonusKind.CALL_RESOLVED, 10000 + i * 60);
		}

		ctx.Equal(100, earned, "seven calls add up to the cadet's $100 cap");
		ctx.Equal(0, LF_DutyBonus.AwardAt(cadet, LF_DutyBonusKind.STOLEN_CAR, 10600), "a bonus past the cap adds nothing");
		ctx.Equal(100, LF_DutyBonus.Pending(cadet), "the capped bonus waits for clock-out");

		// A fine is not a bonus: the officer's pending pay does not move.
		LF_EventBus.Publish(LF_OnFineIssued.Create(cadet.GetPersistentId(), "duty-bonus-fined", vector.Zero, 1, 500));
		ctx.Equal(100, LF_DutyBonus.Pending(cadet), "a $500 fine never reaches the officer");

		// The wrong role or an officer off duty earns nothing.
		ctx.Equal(0, LF_DutyBonus.AwardAt(cadet, LF_DutyBonusKind.REVIVE, 10700), "an officer earns no medic bonus");
		LF_PlayerAccount offDuty = OnDuty("duty-bonus-off", LF_Faction.POLICE, 0);
		offDuty.SetOnDuty(false);
		ctx.Equal(0, LF_DutyBonus.AwardAt(offDuty, LF_DutyBonusKind.BOOKING, 10000), "an officer off duty earns no bonus");

		// The next hour opens a new window.
		ctx.Equal(15, LF_DutyBonus.AwardAt(cadet, LF_DutyBonusKind.CALL_RESOLVED, 10000 + 3600), "the next hour earns again");
		ctx.Equal(115, LF_DutyBonus.Pending(cadet), "both hours wait for clock-out");

		// A lieutenant stops at $70.
		LF_PlayerAccount lieutenant = OnDuty("duty-bonus-lieutenant", LF_Faction.POLICE, 4);
		int lieutenantEarned = 0;
		for (int j = 0; j < 4; j++)
			lieutenantEarned += LF_DutyBonus.AwardAt(lieutenant, LF_DutyBonusKind.STOLEN_CAR, 20000 + j);

		ctx.Equal(70, lieutenantEarned, "four stolen cars stop at the lieutenant's $70 cap");

		// Clock-out takes the bonus once.
		ctx.Equal(115, LF_DutyPay.TakeBonus(cadet), "clock-out pays the whole pending bonus");
		ctx.Equal(0, LF_DutyPay.TakeBonus(cadet), "a second clock-out pays no bonus");
		int secondHour = 0;
		for (int m = 0; m < 8; m++)
			secondHour += LF_DutyBonus.AwardAt(cadet, LF_DutyBonusKind.CALL_RESOLVED, 10000 + 3700 + m);

		ctx.Equal(85, secondHour, "clocking out and in does not reopen the hour's cap: $85 left after $15");

		// Medic: a revive and deliveries through the bus, capped at the orderly's $40.
		LF_PlayerAccount orderly = OnDuty("duty-bonus-orderly", LF_Faction.MEDIC, 0);
		ctx.True(LF_MedicDutyBonus.Install(), "the medic bonus listens for revives and deliveries");
		LF_EventBus.Publish(LF_OnPlayerRevived.Create("duty-bonus-patient", "duty-bonus-orderly", vector.Zero));
		ctx.Equal(10, LF_DutyBonus.Pending(orderly), "a revive pays the medic $10");
		LF_EventBus.Publish(LF_OnPlayerRevived.Create("duty-bonus-orderly", "duty-bonus-orderly", vector.Zero));
		ctx.Equal(10, LF_DutyBonus.Pending(orderly), "reviving yourself pays nothing");

		// A bed delivery publishes the revive and then the delivery: $15 for the pair, not $25.
		LF_EventBus.Publish(LF_OnPlayerRevived.Create("duty-bonus-bed", "duty-bonus-orderly", vector.Zero));
		LF_EventBus.Publish(LF_OnPatientDelivered.Create("duty-bonus-bed", "duty-bonus-orderly", 0, vector.Zero));
		ctx.Equal(25, LF_DutyBonus.Pending(orderly), "a bed delivery pays $15 once, its own revive included");
		for (int k = 0; k < 2; k++)
			LF_EventBus.Publish(LF_OnPatientDelivered.Create("duty-bonus-patient", "duty-bonus-orderly", 0, vector.Zero));

		ctx.Equal(40, LF_DutyBonus.Pending(orderly), "deliveries stop at the orderly's $40 cap");

		// A civilian is owed no bonus.
		LF_PlayerAccount civilian = LF_PlayerAccountManager.GetOrCreate("duty-bonus-civilian");
		civilian.SetFaction(LF_Faction.CIVILIAN);
		ctx.Equal(0, LF_DutyPay.TakeBonus(civilian), "a civilian is paid no duty bonus");

		LF_MedicDutyBonus.ResetStatics();
		LF_DutyBonus.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerAccount OnDuty(string uid, LF_Faction role, int rank)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(role);
		if (role == LF_Faction.POLICE)
			account.SetPoliceRank(rank);
		else
			account.SetMedicRank(rank);

		account.SetOnDuty(true);
		return account;
	}
}
