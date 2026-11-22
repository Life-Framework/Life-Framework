// red-proof: 2026-11-16, `tools\cli test --no-build --red LF_Test_EveronWelcomeGift` (RedProof skips the overlay apply): RED-OK, failed "the clerk's last step has a gift row"; green without the hook (11 assertions, pack leg worlds=LF_Everon1989_Test).
//! #1781 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the clerk hands
//! the 1989 welcome pack with the papers (arrival_register): a baguette, a saucisson and a bottle of water,
//! and the last step hands nothing more (the island arrival route, LF_Test_EveronArrivalRoute). The
//! folded paper map is no longer in it: a newcomer starts with one (LF_Test_EveronStartingMap), so the gift
//! must not hand a second. The neutral core table ships no gift; the pack's overlay adds the row.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronWelcomeGift : LF_Test
{
	static const ResourceName OVERLAY = "{A4E6130B9D2C7F85}Configs/Quests/LF_WelcomeGift_Everon1989.conf";
	static const ResourceName MAP = "{13772C903CB5E4F7}Prefabs/Items/Equipment/Maps/Map_Paper_01/PaperMap_01_folded.et";
	static const ResourceName FOOD = "{787AA46A1D92C7F7}Prefabs/Items/Food/LF_Baguette.et";
	static const ResourceName MEAT = "{A2A09084A440CB11}Prefabs/Items/Food/LF_Saucisson.et";
	static const ResourceName DRINK = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/welcome-gift";
	}

	//------------------------------------------------------------------------------------------------
	//! The core table as shipped, before any overlay.
	protected LF_WelcomeGiftConfig LoadCore()
	{
		Resource container = BaseContainerTools.LoadContainer(LF_WelcomeGiftConfig.CONFIG_PATH);
		if (!container || !container.IsValid() || !container.GetResource())
			return null;

		LF_WelcomeGiftConfig config = LF_WelcomeGiftConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (config && !config.m_aGifts)
			config.m_aGifts = new array<ref LF_WelcomeGiftRow>();

		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The leg boots under the pack header, and that header lists the gift overlay.
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the Everon welcome gift overlay");

		LF_WelcomeGiftConfig table = LoadCore();
		ctx.NotNull(table, "the core welcome gift table loads");
		if (!table)
			return;

		ctx.True(table.Find("arrival_register") == null, "the core table alone gives no gift (the pack decides)");
		// The act: the running header's overlays layered over the core table.
		if (!RedProof())
			table.ApplyLayers();

		LF_WelcomeGiftRow gift = table.Find("arrival_register");
		ctx.NotNull(gift, "the clerk's registration hands the welcome pack");
		if (!gift)
			return;

		ctx.Equal(3, gift.m_aItems.Count(), "the welcome pack is bread, a sausage and a drink");
		ctx.True(gift.m_aItems.Find(MEAT) != -1, "the pack holds a sausage");
		ctx.True(gift.m_aItems.Find(MAP) == -1, "the gift holds no second paper map (the starting kit has the one)");
		ctx.True(gift.m_aItems.Find(FOOD) != -1, "the gift holds food");
		ctx.True(gift.m_aItems.Find(DRINK) != -1, "the gift holds a drink");
		ctx.EqualStr(string.Empty, LF_WelcomeGiftConfig.Problem(gift), "every gift item resolves to a prefab");

		// The live table the tracker reads gives the same items.
		LF_WelcomeGiftTable.ResetStatics();
		array<ResourceName> live = {};
		LF_WelcomeGiftTable.ItemsOf("arrival_register", live);
		ctx.Equal(3, live.Count(), "LF_WelcomeGiftTable hands the three items to the turn-in");
		array<ResourceName> atEnd = {};
		LF_WelcomeGiftTable.ItemsOf("arrival_done", atEnd);
		ctx.Equal(0, atEnd.Count(), "the last step hands nothing more: the pack came with the papers");
		LF_WelcomeGiftTable.ResetStatics();
	}
}
