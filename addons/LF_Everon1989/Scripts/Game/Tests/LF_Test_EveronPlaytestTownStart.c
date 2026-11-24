// red-proof: RedProof() reads the opening bank as the unclosed top-up would set it and the two hint keys without the header's aliases, so the playtest town run fails on the bank and on both hostel lines; the island run has nothing to fail.
//! everon/playtest-town-start (#4678, #4676): under the playtest town's header a fresh character opens with the
//! island's money, not the playtest top-up (the bank is the pack's m_iStartingBank, never the top-up target), and
//! the first hint (the clerk errand on the HUD, page 1 of the F3 guide) no longer sends the player south from a
//! hostel the town does not have. Under the island's header the same keys still read the hostel text, and its
//! top-up is closed by the header already. Run the town side with `test --no-build --only
//! LF_Test_EveronPlaytestTownStart --world "{1054CC8646357708}Missions/LF_PlaytestTown.conf"`.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronPlaytestTownStart : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/playtest-town-start";
	}

	static const string KEY_ERRAND = "#LF-Quest_ArrivalRegister_Desc";
	static const string KEY_GUIDE = "#LF-WelcomeGuide_Body1a";
	//! What the playtest town's money top-up fills a new character's bank to (LF_SpawnLogic.m_iTopUpBankTarget's default).
	static const int TOP_UP_TARGET = 1000000;

	//------------------------------------------------------------------------------------------------
	//! A hint as the player reads it; the red proof asks the engine directly, past the header's aliases.
	protected string Shown(string key)
	{
		if (RedProof())
			return WidgetManager.Translate(key);

		return LF_Terms.Text(key);
	}

	//------------------------------------------------------------------------------------------------
	protected bool MentionsHostel(string text)
	{
		text.ToLower();
		return text.IndexOf("hostel") != -1;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckMoney(LF_TestContext ctx)
	{
		LF_CharacterConfig.Reload();
		int packBank = LF_CharacterConfig.StartingBank(-1);
		ctx.True(packBank >= 0, "the pack sets an opening bank");

		LF_SpawnLogic logic = LF_SpawnLogic.GetInstance();
		ctx.NotNull(logic, "the game mode has a spawn logic");
		if (!logic)
			return;

		int opening = logic.GetStartingBank();
		if (RedProof())
			opening = LF_DebugMoneyTopUp.StartingBank(true, packBank, TOP_UP_TARGET);

		ctx.Equal(packBank, opening, "a new character opens with the island's bank, not the playtest top-up");
		ctx.False(logic.IsMoneyTopUpOn(), "the bank and cash top-up is off in this mission");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckHints(LF_TestContext ctx, bool town)
	{
		LF_TextAliases.ResetStatics();
		string errand = Shown(KEY_ERRAND);
		string guide = Shown(KEY_GUIDE);
		if (town)
		{
			ctx.False(MentionsHostel(errand), "the playtest town's HUD errand does not name a hostel");
			ctx.False(MentionsHostel(guide), "the playtest town's F3 guide page 1 does not name a hostel");
			ctx.True(errand.IndexOf("town hall") != -1, "the town's errand sends the player to the town hall");
			ctx.True(guide.IndexOf("town hall") != -1, "the town's guide sends the player to the town hall");
		}
		else
		{
			ctx.True(MentionsHostel(errand), "the island's HUD errand still starts at the hostel");
			ctx.True(MentionsHostel(guide), "the island's F3 guide page 1 still starts at the hostel");
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		bool town = header && header.m_aLF_TextAliases && !header.m_aLF_TextAliases.IsEmpty();
		if (!header)
		{
			ctx.Pass("no mission header: the boot has no pack mission to check");
			return;
		}

		// The island's header closes every helper; the town's closes the top-up alone.
		if (header.m_bLF_PlaytestHelpersOff || header.m_bLF_MoneyTopUpOff)
			CheckMoney(ctx);

		CheckHints(ctx, town);
		LF_TextAliases.ResetStatics();
	}
}
