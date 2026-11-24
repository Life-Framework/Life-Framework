// red-proof: RED-OK law/booking-desk-suspect-id, observed 2026-11-24 with `tools/cli test --red LF_Test_BookingDeskSuspectId,LF_Test_BookChargeHarness`: with the old `Replication.FindId(entity)` id (RedProof), `the server resolves the id back to the same character` fails (4 assertions). Green: `--only` passed.
//! law/booking-desk-suspect-id (#4699): "Book suspect" sends the cuffed suspect's id to the server, which
//! resolves it with LF_NetworkUtils.FindEntityByRplId. The id has to be the character's RplComponent id; the
//! desk once sent Replication.FindId(entity), which no lookup resolves, so a real booking with a real cuffed
//! suspect beside the officer was refused as "no suspect" (found in the island session, 2026-11-24).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BookingDeskSuspectId : LF_Test
{
	override string GetName()
	{
		return "law/booking-desk-suspect-id";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
		{
			ctx.Fail("suspect fixture did not spawn");
			return;
		}

		ctx.False(LF_BookingDeskBookAction.SuspectRplId(null).IsValid(), "no entity names no id");

		RplId id = LF_BookingDeskBookAction.SuspectRplId(body);
		if (RedProof())
			id = Replication.FindId(body);

		ctx.True(id.IsValid(), "a replicated character has a valid id");
		ctx.True(LF_NetworkUtils.FindEntityByRplId(id) == body, "the server resolves the id back to the same character");
		LF_TestCharacterFixture.Delete(body);
	}
}
