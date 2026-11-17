// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_DevGameMasterReset,LF_Test_CharacterCreationReset,LF_Test_VehicleStateReset,LF_Test_WhitelistStaticReset,LF_Test_OrgStaticReset,LF_Test_UiSheetStaticReset`: RED PROOF OK (all 6 fail with the hook on, e.g. "a world end clears the missing-file counter (expected 0, got 1)"); green with the hooks (--only, 6 passed).
// tier: LOGIC
//! character/creation-reset (#2415): the pick and register offers are keyed by player id and ids
//! restart at 1, so a stale entry lets a new player 1 pass RefuseFurtherCharacter. A disconnect
//! forgets the player's offers and the world-end reset drops the manager.
class LF_Test_CharacterCreationReset : LF_Test
{
	protected static const int PLAYER = 7771;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "character/creation-reset";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_CharacterCreationManager oldManager = LF_CharacterCreationManager.GetInstance();
		oldManager.OfferForTest(PLAYER);
		ctx.True(oldManager.HasPlayerState(PLAYER), "the offers are recorded for the player id");

		if (!RedProof())
			LF_CharacterCreationManager.ForgetPlayer(PLAYER);

		ctx.False(oldManager.HasPlayerState(PLAYER), "a disconnect forgets the player's offers");

		oldManager.OfferForTest(PLAYER);
		LF_StaticResetHook hook1 = new LF_CharacterCreationManager_StaticResetHook();
		if (!RedProof())
			hook1.Apply();

		LF_CharacterCreationManager newManager = LF_CharacterCreationManager.GetInstance();
		ctx.True(newManager != oldManager, "the world-end reset drops the manager singleton");
		ctx.False(newManager.HasPlayerState(PLAYER), "the next world starts without the last world's offers");
		ctx.True(LF_StaticReset.Registered().Contains("LF_CharacterCreationManager"), "a world end resets the character creation manager");
	}
}
