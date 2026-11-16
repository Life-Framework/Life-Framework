// red-proof: Observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_MurderLogoutBody`: RED-OK crime/murder-logout-body (10 assertions fail with the hook on), `a logout body's kill is the murder of its player (their uid) (expected 'murder-logout-body-uid', got '')`. Green after (`--only`). On main the resolver did not exist: the body's kill carried victimPlayerId 0 and was an NPC kill (label npc, no LF_OnPlayerKilled).
// tier: WORLD
//! Acceptance of #2103: a logged-out body has no controlling player (victimPlayerId 0), yet killing
//! it is the murder of that player: the murder path resolves the account uid through LF_LogoutBody.
//! A plain NPC (or any entity that is not a logout body) is still an NPC; a connected player keeps
//! their own uid. The real kill with a killer is a manual step in the PR.
class LF_Test_MurderLogoutBody : LF_Test
{
	protected static const string UID = "murder-logout-body-uid";
	protected static const string CHARACTER_ID = "murder-logout-body-character";

	override string GetName()
	{
		return "crime/murder-logout-body";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_LogoutBody.ResetStatics();

		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		IEntity npc = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(body, "the body character spawns");
		ctx.NotNull(npc, "the plain character spawns");
		if (!body || !npc)
		{
			LF_TestCharacterFixture.Delete(body);
			LF_TestCharacterFixture.Delete(npc);
			return;
		}

		ctx.True(LF_LogoutBody.KeepBody(UID, CHARACTER_ID, body, LF_LogoutBody.Now() + 300), "the body is kept as a logout body");

		// The kill arrives with victimPlayerId 0, as the body has no controlling player.
		string bodyVictim;
		if (!RedProof())
			bodyVictim = LF_Murder.VictimUid(body, 0);

		ctx.EqualStr(UID, bodyVictim, "a logout body's kill is the murder of its player (their uid)");
		ctx.True(LF_Murder.VictimIsPlayer(body, 0), "a logout body counts as a player victim");

		// A plain NPC is still an NPC.
		ctx.EqualStr("", LF_Murder.VictimUid(npc, 0), "a plain NPC has no player behind it");
		ctx.False(LF_Murder.VictimIsPlayer(npc, 0), "a plain NPC is not a player victim");
		ctx.EqualStr("", LF_Murder.VictimUid(null, 0), "no victim has no uid");

		// Once the body ends it is no one's body.
		LF_LogoutBody.ResetStatics();
		ctx.EqualStr("", LF_Murder.VictimUid(body, 0), "a released body is not a logout body any more");

		LF_TestCharacterFixture.Delete(body);
		LF_TestCharacterFixture.Delete(npc);
		LF_LogoutBody.ResetStatics();
	}
}
