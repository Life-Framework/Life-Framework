// red-proof: RED-OK everon/coal-card-text, observed 2026-11-27 (#5065) with `tools\cli test --no-build --red LF_Test_EveronCoalCardText`: with the hook on the expected seam level is the old 2, so the level 2 card, the Mining starter note and the foreman's caption checks fail; green with `--only`.
//! #5065: the texts that name the coal seam's level read the seam's real gate. The East Rise `coal-seam` node
//! (pack ResourceChains overlay) opens at MINER 4; the Mining 2 level-up card (`LF-Unlock_Miner_02`, the licence row),
//! the Mining starter note and the foreman's caption all say that level, built here from `m_iMinLevel` so a moved
//! gate fails this test. The hospital handbook page no longer promises an ambulance service ID that nothing issues:
//! the medic duty desk takes anyone.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronCoalCardText : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/coal-card-text";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ResourceChainCatalog.Reload();
		LF_ResourceNodeDef seam = LF_ResourceChainCatalog.FindNode("coal-seam");
		ctx.NotNull(seam, "the pack adds the coal-seam node");
		if (!seam)
			return;

		int level = seam.m_iMinLevel;
		if (RedProof())
			level = 2;

		string card = LF_Terms.Text("#LF-Unlock_Miner_02");
		string note = LF_Terms.Text("#LF-Activity_Mining_Ore_Note");
		string caption = LF_Terms.Text("#LF-Dialogue_ForemanCoalmansBrow_Next02");
		ctx.True(level > 2, "the East Rise seam is gated above the level-2 licence");
		ctx.True(card.Contains(string.Format("Mining %1", level)), "the Mining 2 card names the seam's level: " + card);
		ctx.True(note.Contains(string.Format("level %1 opens the East Rise coal seam", level)), "the Mining starter note names the seam's level: " + note);
		ctx.True(caption.Contains(string.Format("Mining %1", level)), "the foreman's caption names the seam's level: " + caption);
		ctx.False(note.Contains("coal license"), "the starter note no longer says level 2 opens the coal license");

		string medic = LF_Terms.Text("#LF-Handbook_Everon_MedicBody2");
		ctx.False(medic.Contains("service ID"), "the handbook no longer promises an ambulance service ID");
		ctx.True(medic.Contains("Anyone may sign on"), "the handbook says anyone may sign on at the duty desk");
	}
}
