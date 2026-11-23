// red-proof: RED-OK residents/rumor-talk (observed 2026-11-23 with tools/cli test --no-build --red LF_Test_ResidentRumorTalk: RedProof() skips every ask, so each "told" assertion fails, e.g. "the next game day the barkeeper tells again")
//! residents/rumor-talk (#4461, slice of #4170, plan W5.2): a friendly character who asks a giver
//! "Heard anything?" gets one rumor from `LF_RumorPool.Offers`/`Pick` and its place turns heard
//! (`LF_RumorPool.Tell`); a second ask of the same giver the same game day gets "nothing new"; another
//! day, or another giver, tells again; a crime rumor is refused below `friendly`, and by a giver that
//! is not bar staff or the informant.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ResidentRumorTalk : LF_Test
{
	protected static const string CHARACTER = "rt-char";
	protected static const string BAR = "bar";
	protected static const string SHOP = "shop";
	protected static const int DAY = 11000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/rumor-talk";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_RumorDef Rumor(string id, string place, bool crime, string giver)
	{
		LF_RumorDef def = LF_RumorDef.Make(id, place, "#LF_Test_Rumor_" + id, crime);
		def.m_aGivers.Insert(giver);
		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! One ask. With --red nothing is asked, so the outcome stays NOTHING_NEW and every "told" check fails.
	protected LF_RumorDef Ask(string who, string giverKey, string giver, string tier, int day, out LF_ERumorTalk outcome)
	{
		LF_RumorDef def;
		outcome = LF_ERumorTalk.NOTHING_NEW;
		if (!RedProof())
			def = LF_ResidentTalkRumor.Ask(who, giverKey, giver, tier, day, new LF_RumorState(), outcome);

		return def;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Heard(string who, string place)
	{
		return LF_MapKnown.KnownOf(who, place) == LF_EMapPoiKnown.HEARD;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapKnown.ResetStatics();
		LF_RumorsConfig.ResetStatics();
		LF_ResidentTalkRumor.ResetStatics();

		LF_RumorsConfig config = new LF_RumorsConfig();
		config.m_aCrimeGivers = {BAR, "informant"};
		config.m_aRumors = {};
		config.m_aRumors.Insert(Rumor("bar_a", "rt-a", false, BAR));
		config.m_aRumors.Insert(Rumor("bar_b", "rt-b", false, BAR));
		config.m_aRumors.Insert(Rumor("bar_still", "rt-still", true, BAR));
		config.m_aRumors.Insert(Rumor("shop_a", "rt-shop", false, SHOP));
		config.m_aRumors.Insert(Rumor("shop_crime", "rt-shop-crime", true, SHOP));
		LF_RumorsConfig.UseForTest(config);

		// A crime row is refused below friendly: a neutral player is only offered the plain rows.
		LF_ERumorTalk outcome;
		LF_RumorDef first = Ask(CHARACTER, "barkeeper", BAR, LF_ResidentMemory.TIER_NEUTRAL, DAY, outcome);
		ctx.True(outcome == LF_ERumorTalk.TOLD, "the barkeeper tells a neutral player a rumor");
		ctx.True(first != null && !first.m_bCrime, "what a neutral player is told is never a crime rumor");
		ctx.True(first != null && Heard(CHARACTER, first.m_sPlaceId), "the place of the rumor told turns heard of");
		ctx.True(!Heard(CHARACTER, "rt-still"), "the crime place stays unknown to a neutral player");

		// One a day: the same giver the same day has nothing new, and nothing more is marked.
		LF_ERumorTalk again;
		LF_RumorDef second = Ask(CHARACTER, "barkeeper", BAR, LF_ResidentMemory.TIER_NEUTRAL, DAY, again);
		ctx.True(again == LF_ERumorTalk.ASKED_TODAY, "a second ask the same game day is 'nothing new'");
		ctx.True(second == null, "a second ask the same day tells nothing");
		ctx.True(LF_ResidentTalkRumor.AskedToday(CHARACTER, "barkeeper", DAY), "the giver remembers telling today");
		ctx.True(LF_ResidentTalkRumor.NothingKey(again) == LF_ResidentTalkRumor.KEY_ASKED_TODAY, "the second ask says the asked-today line");

		// Another giver the same day tells its own.
		LF_ERumorTalk shopOutcome;
		LF_RumorDef shopTold = Ask(CHARACTER, "clerk", SHOP, LF_ResidentMemory.TIER_FRIENDLY, DAY, shopOutcome);
		ctx.True(shopOutcome == LF_ERumorTalk.TOLD, "a different giver tells the same day");
		ctx.True(shopTold != null && shopTold.m_sId == "shop_a", "a shop never tells a crime rumor, even to a friendly player");

		// The next day the barkeeper tells again; a friendly player may be told the crime row.
		LF_ERumorTalk nextOutcome;
		LF_RumorDef next = Ask(CHARACTER, "barkeeper", BAR, LF_ResidentMemory.TIER_FRIENDLY, DAY + 1, nextOutcome);
		ctx.True(nextOutcome == LF_ERumorTalk.TOLD, "the next game day the barkeeper tells again");
		ctx.True(next != null && next.m_sId != first.m_sId, "the next rumor is not the one already told");

		// Everything this giver has is known: nothing new, however many days pass.
		LF_ERumorTalk spent;
		LF_RumorDef last;
		int day = DAY + 2;
		while (day < DAY + 8 && spent != LF_ERumorTalk.NOTHING_NEW)
		{
			last = Ask(CHARACTER, "barkeeper", BAR, LF_ResidentMemory.TIER_FRIENDLY, day, spent);
			day++;
		}

		ctx.True(spent == LF_ERumorTalk.NOTHING_NEW, "a giver with no unknown place left has nothing new");
		ctx.True(Heard(CHARACTER, "rt-still"), "a friendly player was told the crime place by the barkeeper before then");

		// A shop never has a crime row for anyone: nothing new once its plain row is known.
		LF_ERumorTalk shopAgain;
		Ask(CHARACTER, "clerk", SHOP, LF_ResidentMemory.TIER_REGULAR, DAY + 9, shopAgain);
		ctx.True(shopAgain == LF_ERumorTalk.NOTHING_NEW, "a shop with only a crime row left has nothing new");
		ctx.True(!Heard(CHARACTER, "rt-shop-crime"), "a crime place is never told by a shop");

		LF_ResidentTalkRumor.ResetStatics();
		LF_MapKnown.ResetStatics();
		LF_RumorsConfig.ResetStatics();
	}
}
