// red-proof: RED-OK 2026-11-16 `tools/cli test --no-build --red LF_Test_SearchSeizedList`: with the seizure skipped it failed `- the toast names the suspect`, `- the toast names the pistol`, `- the toast names the rifle`. Green without the hook.
// tier: WORLD
//! catalogue/search-seized-list (#2154, #1978): a police search tells the officer what it seized.
//! SearchAndSeize on a carrier with two unlawful kinds fills an out list of display names (a count
//! in front when more than one), and the toast text built from it names both; a long list is cut
//! to three with "and N more".
class LF_Test_SearchSeizedList : LF_Test
{
	protected static const string UID = "search-seized-list-a";

	override string GetName()
	{
		return "catalogue/search-seized-list";
	}

	//------------------------------------------------------------------------------------------------
	protected static string NameOf(ResourceName prefab)
	{
		UIInfo info = LF_UIInfoUtils.GetInfo(prefab);
		if (!info || info.GetName().IsEmpty())
			return string.Empty;

		return WidgetManager.Translate(info.GetName());
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig config = LF_Test_ItemCatalogue.MakeConfig();
		LF_WantedReports.ClearFor(UID);

		IEntity suspect = LF_TestCharacterFixture.Spawn(ctx);
		if (!suspect)
			return;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(suspect);
		ctx.True(inventory && LF_InventoryUtils.AddItem(inventory, LF_Test_ItemCatalogue.PISTOL), "the suspect carries a pistol");
		ctx.True(inventory && LF_InventoryUtils.AddItem(inventory, LF_Test_ItemCatalogue.RIFLE), "the suspect carries a rifle");

		LF_Test_CatalogueBuyer civilian = new LF_Test_CatalogueBuyer();
		array<string> offences = {};
		array<string> seizedList = {};
		int seized = 0;
		if (!RedProof())
			seized = LF_ItemSearch.SearchAndSeize(suspect, UID, civilian, config, offences, seizedList);
		ctx.Equal(2, seized, "both unlawful items are seized");
		ctx.Equal(2, seizedList.Count(), "the seized list has one line per kind");

		string pistolName = NameOf(LF_Test_ItemCatalogue.PISTOL);
		string rifleName = NameOf(LF_Test_ItemCatalogue.RIFLE);
		ctx.False(pistolName.IsEmpty(), "the pistol has a display name");
		ctx.False(rifleName.IsEmpty(), "the rifle has a display name");

		string text = LF_ItemSearch.SeizedText("Ivan Petrov", seizedList);
		ctx.True(text.Contains("Ivan Petrov"), "the toast names the suspect");
		ctx.True(!pistolName.IsEmpty() && text.Contains(pistolName), "the toast names the pistol");
		ctx.True(!rifleName.IsEmpty() && text.Contains(rifleName), "the toast names the rifle");

		// A kind seized more than once carries its count; a long list is cut to three.
		array<string> many = {"AKS-74U", "2x 30-round magazine", "Radio", "Lockpick", "Flare"};
		string cut = LF_ItemSearch.SeizedText("Ivan Petrov", many);
		ctx.True(cut.Contains("Radio"), "the third kind is listed");
		ctx.False(cut.Contains("Lockpick"), "the fourth kind is folded into the count");
		ctx.True(cut.Contains("2"), "the toast says how many more");
		array<string> none = {};
		ctx.Equal(0, LF_ItemSearch.SeizedText("Ivan Petrov", none).Length(), "nothing seized, no seized text");

		LF_WantedReports.ClearFor(UID);
		LF_TestCharacterFixture.Delete(suspect);
	}
}
