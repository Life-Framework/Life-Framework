// red-proof: 2026-11-17, a bug fix: one boot of this test against the unfixed CommitRobbery (the paid<=0 return removed): `- the robber is not made wanted for cash they never took (expected 0, got 1)`, `- the till cooldown does not start`. Green on the fixed code.
// tier: WORLD
//! A till robbery whose cash the robber cannot carry (crime review 2026-11-17). It used to pay 0
//! and still start the till cooldown, raise wanted and call dispatch. Now nothing happens.
class LF_Test_RobNoRoom : LF_Test
{
	override string GetName()
	{
		return "crime/rob-no-room";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PlayerAccountManager.Reset();
		LF_RobAction.ResetSession();

		string uid = "test-rob-no-room";
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		ctx.NotNull(account, "robber account exists");

		// An entity with no inventory takes no cash, the same as a full pocket.
		IEntity pocketless = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld());
		ctx.NotNull(pocketless, "a pocketless robber spawns");
		if (!account || !pocketless)
		{
			if (pocketless)
				SCR_EntityHelper.DeleteEntityAndChildren(pocketless);

			return;
		}

		float nowMs = GetGame().GetWorld().GetWorldTime() + 1000;
		float next = 0;
		int paid = LF_RobAction.CommitRobbery(uid, pocketless, 200, 0, 0, 60, 0, nowMs, next);
		SCR_EntityHelper.DeleteEntityAndChildren(pocketless);

		ctx.Equal(0, paid, "nothing is paid");
		ctx.EqualFloat(0, next, 0.5, "the till cooldown does not start");
		ctx.Equal(0, account.GetWantedLevel(), "the robber is not made wanted for cash they never took");
		ctx.EqualStr(LF_RobAction.REFUSED_NO_ROOM, LF_RobAction.GetLastRefusal(), "the refusal names the full pocket");
	}
}
