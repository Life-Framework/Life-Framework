// red-proof: observed 2026-11-23 `tools/cli test --no-build --red LF_Test_PerkSync`: RED-OK perks/sync (35 assertions); with the sync not taken it fails "the client holds swift hands", "fast cuff speeds up the client's cuff bar", "the client reads its counter rows from the sync". Green with `--only`.
//! perks/sync (#4525): the owner sync carries a character's counter-trade levels and every perk they
//! hold. A client that takes it (its store empty, as on a remote machine) then holds the same perks:
//! swift hands and fast cuff shorten its hold bars, double yield is on, and the counter rows come
//! from the synced levels. A later sync replaces the character's picks; another character's stay. A
//! wire with no character is refused.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PerkSync : LF_Test
{
	static const string UID = "test-perk-sync";
	static const string OTHER = "test-perk-sync-other";
	static const string MINER = "MINER";
	static const string POLICE = "POLICE";

	override string GetName()
	{
		return "perks/sync";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_JobTrackDef track = LF_JobLevels.Track(MINER);
		ctx.True(track != null, "the jobs config has a miner ladder");
		if (!track)
			return;

		array<ref LF_JobXpRow> levelsBefore = LF_JobLevels.ExportAll();
		array<ref LF_TradePerkRow> perksBefore = LF_TradePerks.ExportAll();
		LF_PerkSync.ResetStatics();

		array<ref LF_JobXpRow> rows = {};
		foreach (LF_JobXpRow row : levelsBefore)
		{
			if (row && row.m_sPlayerUid != UID)
				rows.Insert(row.Copy());
		}

		rows.Insert(LF_JobXpRow.Create(UID, MINER, track.XpForLevel(12)));
		LF_JobLevels.ApplyAll(rows);
		LF_TradePerks.Forget(UID);
		LF_TradePerks.Forget(OTHER);
		LF_TradePerks.Pick(UID, MINER, 10, LF_PerkEffects.PERK_SWIFT_HANDS);
		LF_TradePerks.Pick(UID, POLICE, 10, LF_PoliceSkill.PERK_FAST_CUFF);
		LF_TradePerks.Pick(OTHER, MINER, 10, LF_PerkEffects.PERK_DOUBLE_YIELD);

		RunWith(ctx);

		LF_TradePerks.ApplyAll(perksBefore);
		LF_JobLevels.ApplyAll(levelsBefore);
		LF_PerkSync.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWith(LF_TestContext ctx)
	{
		string wire = LF_PerkSync.Encode(UID);
		ctx.True(wire.StartsWith("U|" + UID), "the wire names the character first");
		ctx.True(wire.Contains("L|MINER|12"), "the wire carries the mining level");
		ctx.True(wire.Contains("P|MINER|10|swift_hands"), "the wire carries the mining perk");
		ctx.True(wire.Contains("P|POLICE|10|fast_cuff"), "the wire carries the police perk");
		ctx.False(wire.Contains(OTHER), "the wire carries no other character");
		ctx.EqualStr(string.Empty, LF_PerkSync.Encode(string.Empty), "no character, no wire");
		ctx.Equal(0, LF_PerkSync.Push(UID), "a character with no live body is sent nothing");

		// A remote client's store starts empty.
		LF_TradePerks.Forget(UID);
		ctx.False(LF_TradePerks.Has(UID, MINER, LF_PerkEffects.PERK_SWIFT_HANDS), "the client starts with no perk");
		ctx.False(LF_PerkSync.Ready(UID), "the client has no sync yet");

		// RedProof(): the sync is not taken, so every client read below fails (`--red`).
		if (!RedProof())
			ctx.True(LF_PerkSync.Apply(wire, true), "the client takes the sync");

		ctx.True(LF_PerkSync.Ready(UID), "the client has its sync");
		ctx.Equal(12, LF_PerkSync.LevelOf(UID, MINER), "the client knows its mining level");
		ctx.True(LF_TradePerks.Has(UID, MINER, LF_PerkEffects.PERK_SWIFT_HANDS), "the client holds swift hands");
		ctx.True(LF_PerkEffects.SpeedFactor(UID, MINER) > 1.0, "swift hands speeds up the client's gather bar");
		ctx.True(LF_PoliceSkill.CuffSpeed(UID) > 1.0, "fast cuff speeds up the client's cuff bar");
		ctx.True(LF_PoliceSkill.CuffSeconds(UID, 4.0) < 4.0, "fast cuff shortens the client's cuff hold");
		ctx.True(LF_TradePerks.Has(OTHER, MINER, LF_PerkEffects.PERK_DOUBLE_YIELD), "another character's pick is left alone");

		// The counter rows from the sync: a paid change to double yield in mining, no free take.
		array<ref LF_PerkPickOffer> offers = {};
		ctx.True(LF_PerkPick.OffersSynced(UID, offers), "the client reads its counter rows from the sync");
		LF_PerkPickOffer change = LF_PerkPick.Find(offers, 10, LF_PerkEffects.PERK_DOUBLE_YIELD, true, MINER);
		ctx.True(change != null, "the synced counter offers the paid change in mining");
		ctx.True(LF_PerkPick.Find(offers, 10, LF_PerkEffects.PERK_DOUBLE_YIELD, false) == null, "the synced counter offers no free take at a picked level");

		// A later sync with no picks (a reset character) clears this character's picks only.
		ctx.True(LF_PerkSync.Apply("U|" + UID + "\nL|MINER|12", true), "a sync with no picks is taken");
		ctx.False(LF_TradePerks.Has(UID, MINER, LF_PerkEffects.PERK_SWIFT_HANDS), "the later sync replaces the picks");
		ctx.True(LF_PoliceSkill.CuffSpeed(UID) == 1.0, "with no perk the cuff bar runs at its own speed");
		ctx.True(LF_TradePerks.Has(OTHER, MINER, LF_PerkEffects.PERK_DOUBLE_YIELD), "the other character keeps its pick");

		ctx.False(LF_PerkSync.Apply("L|MINER|12", true), "a wire with no character is refused");
		ctx.False(LF_PerkSync.Apply("U|", true), "a wire with an empty character is refused");
		ctx.True(LF_PerkSync.Ready(UID), "a refused wire leaves the last sync");
	}
}
