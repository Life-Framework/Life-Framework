// red-proof: 2026-11-16, `tools\cli test --no-build --red LF_Test_EveronWelcomeGift` (RedProof skips the overlay apply): RED-OK, failed "the clerk's last step has a gift row"; green without the hook (11 assertions, pack leg worlds=LF_Everon1989_Test).
// tier: WORLD
//! #1781 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the clerk's last
//! arrival step hands over a 1989 welcome gift: a folded paper map, a packet of crackers and a bottle
//! of apple juice. The neutral core table ships no gift; the pack's overlay adds the row.
class LF_Test_EveronWelcomeGift : LF_Test
{
	static const ResourceName OVERLAY = "{A4E6130B9D2C7F85}Configs/Quests/LF_WelcomeGift_Everon1989.conf";
	static const ResourceName MAP = "{13772C903CB5E4F7}Prefabs/Items/Equipment/Maps/Map_Paper_01/PaperMap_01_folded.et";
	static const ResourceName FOOD = "{8F2C06D1F757C40F}Prefabs/Items/Food/LF_Crackers.et";
	static const ResourceName DRINK = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";

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

		ctx.True(table.Find("arrival_done") == null, "the core table alone gives no gift (the pack decides)");
		// The act: the running header's overlays layered over the core table.
		if (!RedProof())
			table.ApplyLayers();

		LF_WelcomeGiftRow gift = table.Find("arrival_done");
		ctx.NotNull(gift, "the clerk's last step has a gift row");
		if (!gift)
			return;

		ctx.Equal(3, gift.m_aItems.Count(), "the gift is a map, food and a drink");
		ctx.True(gift.m_aItems.Find(MAP) != -1, "the gift holds the folded paper map");
		ctx.True(gift.m_aItems.Find(FOOD) != -1, "the gift holds food");
		ctx.True(gift.m_aItems.Find(DRINK) != -1, "the gift holds a drink");
		ctx.EqualStr(string.Empty, LF_WelcomeGiftConfig.Problem(gift), "every gift item resolves to a prefab");

		// The live table the tracker reads gives the same items.
		LF_WelcomeGiftTable.ResetStatics();
		array<ResourceName> live = {};
		LF_WelcomeGiftTable.ItemsOf("arrival_done", live);
		ctx.Equal(3, live.Count(), "LF_WelcomeGiftTable hands the three items to the turn-in");
		LF_WelcomeGiftTable.ResetStatics();
	}
}
