// red-proof: 2026-11-18, `cli test --no-build --red LF_Test_InformantTip`: RedProof() skips the qualified player's ask, so the fee is never taken, no mark is put and `the qualified player gets a mark at the open spot` fails. The first run, before LF_InformantHook existed, did not compile.
//! laundering/informant-tip (#2921): the informant's tip-off. A player at the underworld tier pays the
//! config fee once and gets a private mark at today's open buyer spot; the same ask in the same period
//! takes nothing more; too little cash refuses with nothing taken; below the tier or on duty as police
//! refuses in the "never heard of him" words with nothing taken; the mark clears when the period ends and
//! the next period's ask marks the next spot; with no spot placed nothing is sold. Three fixture spots, a
//! forced period and a recording map sink, no network.
class LF_Test_InformantSink : LF_MapShareSink
{
	ref array<string> m_aSent = {};
	ref map<string, int> m_mX = new map<string, int>();
	ref map<string, int> m_mZ = new map<string, int>();

	override void Add(int playerId, LF_MapShareRow row)
	{
		m_aSent.Insert(string.Format("add:%1:%2", playerId, row.m_sKey));
		m_mX.Set(row.m_sKey, row.m_iX);
		m_mZ.Set(row.m_sKey, row.m_iZ);
	}

	override void Remove(int playerId, string key)
	{
		m_aSent.Insert(string.Format("remove:%1:%2", playerId, key));
	}

	bool Got(string line)
	{
		return m_aSent.Find(line) != -1;
	}
}

//------------------------------------------------------------------------------------------------
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_InformantTip : LF_Test
{
	protected static const string SEED = "informant-tip-test-seed";
	protected static const string UID = "informant-tip-uid";
	protected static const string SPOT_A = "it-a";
	protected static const string SPOT_B = "it-b";
	protected static const string SPOT_C = "it-c";
	protected static const int PERIOD_HOURS = 4;
	protected static const int FEE = 300;
	protected static const int PLAYER_ID = 1;
	protected static const float WORLD_NOW = 1000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "laundering/informant-tip";
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Spawn(vector at)
	{
		return LF_Utils.SpawnEntityPrefab(LF_Bank.DOOR_PREFAB, at, "0 0 0", false);
	}

	//------------------------------------------------------------------------------------------------
	//! A standing table with the underworld ladder.
	protected LF_ReputationConfig MakeStanding()
	{
		array<int> thresholds = {-300, -100, 0, 100, 400, 1000};
		LF_ReputationConfig config = new LF_ReputationConfig();
		config.m_aFactions = {};
		config.m_aFactions.Insert(LF_ReputationFactionDef.Make("UNDERWORLD", thresholds, 0));
		config.m_aAwards = {};
		config.m_aGates = {};
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity anchor = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.BREACH_DOOR);
		ctx.NotNull(anchor, "the annex breach door anchors the fixtures");
		if (!anchor)
			return;

		IEntity player = LF_TestCharacterFixture.Spawn(ctx);
		if (!player)
			return;

		LF_BuyerRotation.ResetStatics();
		LF_BuyerRotation.SetSeedForTests(SEED);
		LF_LaunderConfig.SetBuyerPeriodHoursForTests(PERIOD_HOURS);
		LF_InformantTip.ResetStatics();
		LF_MapShares.ResetStatics();

		LF_LaunderConfig config = new LF_LaunderConfig();
		config.m_iInformantFee = FEE;
		config.m_iInformantMinTier = 1;
		LF_LaunderConfig.Install(config);

		LF_Utils.SetUidForTests(player, UID);
		LF_Reputation.Forget(UID);
		LF_ReputationConfig.UseForTest(MakeStanding());

		vector at = anchor.GetOrigin() + "0 -60 0";
		IEntity a = Spawn(at);
		IEntity b = Spawn(at + "400 0 0");
		IEntity c = Spawn(at + "800 0 0");
		ctx.True(a && b && c, "three fixture spots spawn");
		if (a && b && c)
			RunWith(ctx, player, a, b, c);

		LF_Places.Unregister(LF_BuyerRotation.PLACE_KIND, SPOT_A);
		LF_Places.Unregister(LF_BuyerRotation.PLACE_KIND, SPOT_B);
		LF_Places.Unregister(LF_BuyerRotation.PLACE_KIND, SPOT_C);
		SCR_EntityHelper.DeleteEntityAndChildren(a);
		SCR_EntityHelper.DeleteEntityAndChildren(b);
		SCR_EntityHelper.DeleteEntityAndChildren(c);
		LF_Reputation.Forget(UID);
		LF_ReputationConfig.Reload();
		LF_MapShares.ResetStatics();
		LF_InformantTip.ResetStatics();
		LF_BuyerRotation.ResetStatics();
		LF_LaunderConfig.ResetStatics();
		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(player);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWith(LF_TestContext ctx, IEntity player, IEntity a, IEntity b, IEntity c)
	{
		int period = PERIOD_HOURS * 3600;
		int start = 125001 * period;
		string markKey = LF_InformantTip.MarkKey(UID);

		ctx.True(LF_Places.Register(LF_BuyerRotation.PLACE_KIND, SPOT_A, a), "spot a is placed");
		ctx.True(LF_Places.Register(LF_BuyerRotation.PLACE_KIND, SPOT_B, b), "spot b is placed");
		ctx.True(LF_Places.Register(LF_BuyerRotation.PLACE_KIND, SPOT_C, c), "spot c is placed");

		string open1 = LF_BuyerRotation.OpenSpot(start);
		string open2 = LF_BuyerRotation.OpenSpot(start + period);
		IEntity spot1 = LF_Places.Find(LF_BuyerRotation.PLACE_KIND, open1);
		IEntity spot2 = LF_Places.Find(LF_BuyerRotation.PLACE_KIND, open2);
		if (!spot1 || !spot2 || open1 == open2)
		{
			ctx.True(false, "two periods in a row open two different, placed spots");
			return;
		}

		LF_Test_InformantSink sink = new LF_Test_InformantSink();
		LF_Test_MapShareFacts facts = new LF_Test_MapShareFacts();
		facts.Set(PLAYER_ID, UID, "", "");
		LF_MapShares.SetSink(sink);
		LF_MapShares.SetFacts(facts);

		LF_DialogueHook hook = LF_DialogueHook.Find("Informant");
		ctx.NotNull(hook, "the Informant dialogue hook has its class");
		if (!hook)
			return;

		string key;
		int carried = LF_MoneyUtils.GetCash(player);
		if (carried > 0)
			LF_MoneyUtils.RemoveCash(player, carried);

		LF_MoneyUtils.GiveCash(player, FEE - 100);
		int cash = LF_MoneyUtils.GetCash(player);
		ctx.Equal(FEE - 100, cash, "the player holds less than the fee");

		// Below the tier: the reply is hidden, a forced ask is refused in words, nothing is taken or marked.
		ctx.False(hook.Allows(player, null, ""), "the reply is hidden below the config tier");
		ctx.Equal(LF_EInformantTip.NOT_TRUSTED, LF_InformantTip.Ask(player, UID, start, WORLD_NOW, key), "below the tier the ask is refused as not trusted");
		ctx.EqualStr(LF_InformantTip.KEY_NEVER_HEARD, key, "below the tier the answer is 'never heard of him'");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(player), "a refusal below the tier takes nothing");
		ctx.False(LF_MapShares.Has(markKey), "a refusal below the tier marks nothing");

		// At the tier but on duty as police: the same words, nothing taken. (Cash is short as well; the
		// standing check comes first, so the refusal does not say which it was.)
		LF_Reputation.Add(UID, "UNDERWORLD", 100, "test");
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(UID);
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		ctx.False(hook.Allows(player, null, ""), "the reply is hidden from police on duty");
		ctx.Equal(LF_EInformantTip.NOT_TRUSTED, LF_InformantTip.Ask(player, UID, start, WORLD_NOW, key), "police on duty are refused as not trusted");
		ctx.EqualStr(LF_InformantTip.KEY_NEVER_HEARD, key, "police on duty get 'never heard of him'");
		ctx.False(LF_MapShares.Has(markKey), "police on duty get no mark");
		account.SetOnDuty(false);
		account.SetFaction(LF_Faction.CIVILIAN);
		ctx.True(hook.Allows(player, null, ""), "the reply is offered at the config tier, off duty");

		// Too little cash: refused in words with nothing taken.
		ctx.Equal(LF_EInformantTip.NO_CASH, LF_InformantTip.Ask(player, UID, start, WORLD_NOW, key), "too little cash is refused");
		ctx.EqualStr(LF_InformantTip.KEY_NO_CASH, key, "too little cash is told in words");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(player), "too little cash takes nothing");
		ctx.False(LF_MapShares.Has(markKey), "too little cash marks nothing");

		// Enough cash: the act. Red proof: skip the ask.
		LF_MoneyUtils.GiveCash(player, FEE);
		cash = LF_MoneyUtils.GetCash(player);
		LF_EInformantTip paid = LF_EInformantTip.REFUSED;
		if (!RedProof())
			paid = LF_InformantTip.Ask(player, UID, start, WORLD_NOW, key);

		ctx.Equal(LF_EInformantTip.PAID, paid, "the qualified player gets a mark at the open spot");
		ctx.EqualStr(LF_InformantTip.KEY_TOLD, key, "the informant says where he is");
		ctx.Equal(cash - FEE, LF_MoneyUtils.GetCash(player), "the fee was taken once");
		ctx.True(LF_MapShares.Has(markKey), "the mark is on the map");
		ctx.True(sink.Got("add:1:" + markKey), "the mark goes to this player");
		ctx.Equal(Math.Round(spot1.GetOrigin()[0]), sink.m_mX.Get(markKey), "the mark is at the open spot (x)");
		ctx.Equal(Math.Round(spot1.GetOrigin()[2]), sink.m_mZ.Get(markKey), "the mark is at the open spot (z)");
		cash = LF_MoneyUtils.GetCash(player);

		// A second ask in the same period: told so, nothing more taken.
		ctx.Equal(LF_EInformantTip.ALREADY, LF_InformantTip.Ask(player, UID, start + 60, WORLD_NOW + 60, key), "a second ask in the period is not charged");
		ctx.EqualStr(LF_InformantTip.KEY_ALREADY, key, "a second ask is told he already knows");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(player), "a second ask takes no second fee");

		// The mark clears when the period ends: still there just before, gone at the change.
		float left = LF_InformantTip.SecondsLeft(start);
		ctx.EqualFloat(period, left, 0.5, "a mark made at the period's start lasts the whole period");
		LF_MapShares.Expire(WORLD_NOW + left - 1);
		ctx.True(LF_MapShares.Has(markKey), "the mark is still there just before the rotation moves");
		LF_MapShares.Expire(WORLD_NOW + left);
		ctx.False(LF_MapShares.Has(markKey), "the mark is gone when the rotation moves");
		ctx.True(sink.Got("remove:1:" + markKey), "the player's map is told to drop it");

		// The next period: a fresh ask pays again and marks the new spot.
		LF_BuyerRotation.ForgetPeriod();
		LF_MoneyUtils.GiveCash(player, FEE);
		cash = LF_MoneyUtils.GetCash(player);
		ctx.Equal(LF_EInformantTip.PAID, LF_InformantTip.Ask(player, UID, start + period, WORLD_NOW + left, key), "the next period's ask is sold again");
		ctx.Equal(cash - FEE, LF_MoneyUtils.GetCash(player), "the next period's tip costs the fee again");
		ctx.Equal(Math.Round(spot2.GetOrigin()[0]), sink.m_mX.Get(markKey), "the new mark is at the new open spot (x)");
		ctx.Equal(Math.Round(spot2.GetOrigin()[2]), sink.m_mZ.Get(markKey), "the new mark is at the new open spot (z)");
		cash = LF_MoneyUtils.GetCash(player);

		// No spot placed: nothing is sold and nothing is taken.
		LF_Places.Unregister(LF_BuyerRotation.PLACE_KIND, SPOT_A);
		LF_Places.Unregister(LF_BuyerRotation.PLACE_KIND, SPOT_B);
		LF_Places.Unregister(LF_BuyerRotation.PLACE_KIND, SPOT_C);
		LF_BuyerRotation.ForgetPeriod();
		LF_MapShares.Remove(markKey);
		ctx.Equal(LF_EInformantTip.NO_SPOT, LF_InformantTip.Ask(player, UID, start + 7 * period, WORLD_NOW, key), "with no buyer spot placed nothing is sold");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(player), "with no spot nothing is taken");
	}
}
