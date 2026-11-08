// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: replaced the
// IsIssued() check in LF_IdCardComponent.Issue with `if (false)`; got
// `[ELTEST] FAIL creation/id-card`, `- a printed card cannot be printed again`,
// `- a refused reprint leaves the name (expected 'Ada', got 'Bob')`. Restored; green.
// The same run caught a real bug: GetItems listed the card twice (`expected 1, got 2`), so
// LF_IdCard.FindCarried now counts each card once.
// tier: WORLD
class EL_Test_IdCardGrant : EL_Test
{
	override string GetName()
	{
		return "creation/id-card";
	}

	override void Run(EL_TestContext ctx)
	{
		IEntity character = EL_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		ctx.True(LF_IdCard.Grant(character, "creation-id-card-holder", "Ada", "Lovelace", 36), "the registered character is granted a card");

		array<LF_IdCardComponent> cards = {};
		LF_IdCard.FindCarried(character, cards);
		ctx.Equal(1, cards.Count(), "the character carries exactly one card");

		LF_IdCardComponent card = LF_IdCard.FindShown(character, "creation-id-card-holder");
		ctx.True(card != null, "the carried card is the one shown");
		if (card)
		{
			ctx.EqualStr("creation-id-card-holder", card.GetHolderId(), "the card carries the holder id");
			ctx.EqualStr("Ada", card.GetFirstName(), "the card carries the first name");
			ctx.EqualStr("Lovelace", card.GetLastName(), "the card carries the last name");
			ctx.Equal(36, card.GetAge(), "the card carries the age");
			ctx.False(card.Issue("someone-else", "Bob", "Builder", 40), "a printed card cannot be printed again");
			ctx.EqualStr("Ada", card.GetFirstName(), "a refused reprint leaves the name");
		}

		ctx.False(LF_IdCard.Grant(character, "creation-id-card-holder", "Ada", "Lovelace", 36), "a second card for the same character is refused");
		LF_IdCard.FindCarried(character, cards);
		ctx.Equal(1, cards.Count(), "the refused grant added no card");

		EL_TestCharacterFixture.Delete(character);
	}
}
