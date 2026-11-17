// red-proof: observed 2026-11-18 with `--red` (RED-OK): the hook skips the limiter, so 'an officer's ticket requests are capped at the burst inside one window' fails. The uid assertion, run on the unfixed code (m_sHolderUid an RplProp again, log tmp/logs/test-20261118-003827.log): FAIL 'the ticket component replicates no uid field (expected 0, got 1)'. Restored: green.
// tier: LOGIC
//! police/ticket-request-limit (#2416): one officer may send ticketRateBurst ticket requests per
//! ticketRateWindowSeconds; the rest are refused, another officer is unaffected, and the window
//! reopens. The ticket paper no longer carries the holder's platform uid as a replicated property.
class LF_Test_TicketRequestLimit : LF_Test
{
	protected static const int OFFICER = 7301;
	protected static const int OTHER = 7302;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/ticket-request-limit";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_RateLimit.ResetStatics();
		int burst = LF_PoliceConfig.TicketRateBurst();
		int window = LF_PoliceConfig.TicketRateWindowMs();
		ctx.True(burst >= 1 && window >= 1000, "the police config loads a ticket request limit");

		int now = 100000;
		int allowed = 0;
		int tries = burst + 5;
		for (int i = 0; i < tries; i++)
		{
			bool ok = true;
			if (!RedProof())
				ok = LF_FineTicketRole.AllowWriteRequest(OFFICER, now + i, false);
			if (ok)
				allowed++;
		}

		ctx.Equal(burst, allowed, "an officer's ticket requests are capped at the burst inside one window");
		ctx.True(LF_FineTicketRole.AllowWriteRequest(OTHER, now, false), "another officer has their own allowance");
		ctx.False(LF_FineTicketRole.AllowWriteRequest(OFFICER, now + window - 1, false), "the window has not reopened yet");
		ctx.True(LF_FineTicketRole.AllowWriteRequest(OFFICER, now + window + 10, false), "the next window admits requests again");
		ctx.True(LF_FineTicketRole.AllowWriteRequest(0, now, false), "a server-local call with no player is never limited");

		// The paper: no replicated property holds the platform uid of its holder.
		typename ticket = LF_FineTicketComponent;
		int count = ticket.GetVariableCount();
		int uidVars = 0;
		for (int v = 0; v < count; v++)
		{
			string name = ticket.GetVariableName(v);
			name.ToLower();
			if (name.Contains("uid"))
				uidVars++;
		}

		ctx.Equal(0, uidVars, "the ticket component replicates no uid field");
		LF_RateLimit.ResetStatics();
	}
}
