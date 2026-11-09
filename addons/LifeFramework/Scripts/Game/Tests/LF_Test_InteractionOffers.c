// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only` (all four broken in one run, 0/4 passed), then restored; green. Breaking `distance > REACH` to `REACH * 100` in LF_PlayerInteraction.CheckReach failed
// `[LFTEST] FAIL interaction/offers`, `- 2.5 m away is refused as too far (expected 5, got 0)`.
// tier: LOGIC
class LF_Test_InteractionOffers : LF_Test
{
	override string GetName()
	{
		return "interaction/offers";
	}

	override void Run(LF_TestContext ctx)
	{
		// The list: a civilian (or an off-duty officer) sees 4, an on-duty officer sees 6.
		ctx.Equal(4, CountOffers(false), "a civilian or off-duty officer is offered 4 actions");
		ctx.Equal(6, CountOffers(true), "an on-duty officer is offered the 2 police actions too");
		ctx.False(LF_PlayerInteraction.Offers(LF_EPlayerAction.SEARCH, false), "search is not offered off duty");
		ctx.True(LF_PlayerInteraction.Offers(LF_EPlayerAction.ARREST, true), "arrest is offered on duty");

		// Reach: the server refuses anyone beyond REACH, the dead, yourself, and non-players.
		ctx.Equal(LF_EInteractionRefusal.NONE, LF_PlayerInteraction.CheckReach(true, false, true, true, true, 1.2), "a living player 1.2 m away is in reach");
		ctx.Equal(LF_EInteractionRefusal.NONE, LF_PlayerInteraction.CheckReach(true, false, true, true, true, LF_PlayerInteraction.REACH), "exactly at reach is in reach");
		ctx.Equal(LF_EInteractionRefusal.TOO_FAR, LF_PlayerInteraction.CheckReach(true, false, true, true, true, 2.5), "2.5 m away is refused as too far");
		ctx.Equal(LF_EInteractionRefusal.TOO_FAR, LF_PlayerInteraction.CheckReach(true, false, true, true, true, 40), "across the street is refused as too far");
		ctx.Equal(LF_EInteractionRefusal.DEAD, LF_PlayerInteraction.CheckReach(true, false, true, true, false, 1), "a dead target is refused");
		ctx.Equal(LF_EInteractionRefusal.DEAD, LF_PlayerInteraction.CheckReach(true, false, true, false, true, 1), "a dead user is refused");
		ctx.Equal(LF_EInteractionRefusal.SELF, LF_PlayerInteraction.CheckReach(true, true, true, true, true, 0), "yourself is refused");
		ctx.Equal(LF_EInteractionRefusal.NOT_A_PLAYER, LF_PlayerInteraction.CheckReach(true, false, false, true, true, 1), "an NPC is refused");
		ctx.Equal(LF_EInteractionRefusal.NO_TARGET, LF_PlayerInteraction.CheckReach(false, false, false, true, true, 0), "no target is refused");
		ctx.Equal(LF_EInteractionRefusal.NO_TARGET, LF_PlayerInteraction.CheckPair(null, null), "null entities are refused, not crashed on");

		// Typed amounts.
		ctx.Equal(50, LF_PlayerInteraction.ParseAmount(" 50 "), "50 parses");
		ctx.Equal(-1, LF_PlayerInteraction.ParseAmount("0"), "0 is refused");
		ctx.Equal(-1, LF_PlayerInteraction.ParseAmount("-5"), "a negative amount is refused");
		ctx.Equal(-1, LF_PlayerInteraction.ParseAmount("5.5"), "a fraction is refused");
		ctx.Equal(-1, LF_PlayerInteraction.ParseAmount("abc"), "letters are refused");
		ctx.Equal(-1, LF_PlayerInteraction.ParseAmount(""), "an empty field is refused");
		ctx.Equal(-1, LF_PlayerInteraction.ParseAmount("99999999"), "an absurd amount is refused");

		// Role slots stay hidden until a later scope registers a handler.
		ctx.True(LF_PlayerInteraction.GetRole("lf-test-slot") == null, "an unregistered slot has no handler");
		LF_PlayerInteraction.RegisterRole("lf-test-slot", new LF_PlayerRoleHandler());
		ctx.True(LF_PlayerInteraction.GetRole("lf-test-slot") != null, "a registered slot has its handler");
		LF_PlayerInteraction.RegisterRole("", new LF_PlayerRoleHandler());
		ctx.True(LF_PlayerInteraction.GetRole("") == null, "an empty slot name is refused");
		LF_PlayerInteraction.UnregisterRole("lf-test-slot");
		ctx.True(LF_PlayerInteraction.GetRole("lf-test-slot") == null, "an unregistered slot hides again");
	}

	protected int CountOffers(bool onDutyPolice)
	{
		array<LF_EPlayerAction> all = {LF_EPlayerAction.INTRODUCE, LF_EPlayerAction.SHOW_ID, LF_EPlayerAction.GIVE_CASH, LF_EPlayerAction.GIVE_ITEM, LF_EPlayerAction.SEARCH, LF_EPlayerAction.ARREST};
		int count = 0;
		foreach (LF_EPlayerAction action : all)
		{
			if (LF_PlayerInteraction.Offers(action, onDutyPolice))
				count++;
		}

		return count;
	}
}
