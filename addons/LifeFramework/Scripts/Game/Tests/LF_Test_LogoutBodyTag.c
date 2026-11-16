// red-proof: RED-OK character/logout-body-tag (fails with the hook on, 11 assertions), 2026-11-16
// tier: LOGIC
//! Logged-out tag (#1974): the model is on for a logged-out body and off for a connected or rejoined one.
class LF_Test_LogoutBodyTag : LF_Test
{
	override string GetName()
	{
		return "character/logout-body-tag";
	}

	override void Run(LF_TestContext ctx)
	{
		// A logged-out body: flagged by the server, alive, nobody controls it.
		bool body = false;
		// Red-proof hook: with --red the act is skipped, so the assertions on the body must fail.
		if (!RedProof())
			body = LF_LogoutBodyTag.IsOn(true, true, false);

		ctx.True(body, "a logged-out body shows the tag");

		ctx.False(LF_LogoutBodyTag.IsOn(false, true, true), "a connected player's character shows no tag");
		ctx.False(LF_LogoutBodyTag.IsOn(false, true, false), "a rejoined player's new character (flag never set) shows no tag");
		ctx.False(LF_LogoutBodyTag.IsOn(false, true, false), "a released body (flag cleared) shows no tag");
		ctx.False(LF_LogoutBodyTag.IsOn(true, true, true), "a flagged character a player controls again shows no tag");
		ctx.False(LF_LogoutBodyTag.IsOn(true, false, false), "a killed body is a corpse: no tag");

		float range = LF_LogoutBodyTag.RANGE_METERS;
		ctx.True(LF_LogoutBodyTag.Draws(true, range - 1, 3), "a tag in range and in front of the camera is drawn");
		ctx.False(LF_LogoutBodyTag.Draws(true, range + 1, 3), "a tag out of range is not drawn");
		ctx.False(LF_LogoutBodyTag.Draws(true, 2, -1), "a tag behind the camera is not drawn");
		ctx.False(LF_LogoutBodyTag.Draws(false, 2, 3), "a body with the tag off is never drawn");
	}
}
