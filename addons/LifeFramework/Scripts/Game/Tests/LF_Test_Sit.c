// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_Sit`: dropping the restrained check in
// LF_Sit.Decide failed it with `- a cuffed body is refused` and `- cuffs outrank a taken seat`. Restored; green.
// tier: LOGIC
//! emote/sit (#982): who may sit, the reason they read, where a seat puts the sitter, and who may emote.
class LF_Test_Sit : LF_Test
{
	override string GetName()
	{
		return "emote/sit";
	}

	override void Run(LF_TestContext ctx)
	{
		// The rule, in priority order.
		ctx.True(LF_Sit.Decide(true, true, false, false, false, false) == LF_ESitRefusal.NONE, "a free body on a free seat sits");
		ctx.True(LF_Sit.Decide(false, true, false, false, false, false) == LF_ESitRefusal.NO_CHARACTER, "no character is refused");
		ctx.True(LF_Sit.Decide(true, false, false, false, false, false) == LF_ESitRefusal.DEAD, "a dead or downed body is refused");
		ctx.True(LF_Sit.Decide(true, true, true, false, false, false) == LF_ESitRefusal.RESTRAINED, "a cuffed body is refused");
		ctx.True(LF_Sit.Decide(true, true, false, true, false, false) == LF_ESitRefusal.STUNNED, "a stunned body is refused");
		ctx.True(LF_Sit.Decide(true, true, false, false, true, false) == LF_ESitRefusal.OCCUPIED, "a seat someone sits on is refused");
		ctx.True(LF_Sit.Decide(true, true, false, false, false, true) == LF_ESitRefusal.BUSY, "a body mid-gesture or in a vehicle is refused");
		ctx.True(LF_Sit.Decide(true, true, true, false, true, false) == LF_ESitRefusal.RESTRAINED, "cuffs outrank a taken seat");

		ctx.EqualStr("#LF-Sit_Occupied", LF_Sit.ReasonKey(LF_ESitRefusal.OCCUPIED), "a taken seat says so");
		ctx.EqualStr("#LF-Emote_Restrained", LF_Sit.ReasonKey(LF_ESitRefusal.RESTRAINED), "cuffs say so");
		ctx.EqualStr("#LF-Sit_Denied", LF_Sit.ReasonKey(LF_ESitRefusal.STUNNED), "other refusals share one line");

		// The seat position: a seat at (10, 0, 20) facing world +z.
		vector seat[4];
		seat[0] = "1 0 0";
		seat[1] = "0 1 0";
		seat[2] = "0 0 1";
		seat[3] = "10 0 20";
		vector at[4];
		LF_Sit.SeatTransform(seat, "0.45 0 0.3", 0, at);
		ctx.EqualFloat(10.45, at[3][0], 0.001, "the offset moves along the seat's x");
		ctx.EqualFloat(20.3, at[3][2], 0.001, "the offset moves along the seat's z");
		ctx.EqualFloat(1, at[2][2], 0.001, "yaw 0 faces the seat's +z");

		LF_Sit.SeatTransform(seat, "0 0 0", 180, at);
		ctx.EqualFloat(-1, at[2][2], 0.001, "yaw 180 faces the seat's -z");

		// The same seat turned so its +z points along world +x.
		seat[0] = "0 0 -1";
		seat[2] = "1 0 0";
		LF_Sit.SeatTransform(seat, "0 0 0.3", 0, at);
		ctx.EqualFloat(10.3, at[3][0], 0.001, "a turned seat puts the sitter in front of it in world space");
		ctx.EqualFloat(20, at[3][2], 0.001, "a turned seat does not slide the sitter sideways");
		ctx.EqualFloat(1, at[2][0], 0.001, "a turned seat turns the sitter with it");
		ctx.EqualFloat(1, at[1][1], 0.001, "the sitter stays upright");

		// Who may emote.
		ctx.True(LF_Emote.Allowed(false, false), "a free body may emote");
		ctx.False(LF_Emote.Allowed(true, false), "a cuffed body may not emote");
		ctx.False(LF_Emote.Allowed(false, true), "a stunned body may not emote");
		ctx.False(LF_Emote.IsGroupFree(null), "no command is not group free");
	}
}
