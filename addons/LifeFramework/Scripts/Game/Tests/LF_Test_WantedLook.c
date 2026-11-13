// red-proof: made LF_WantedLook.GuidOf take the brace (`Substring(0, GUID_LENGTH)`) and ran
// `tools\cli test --no-build --only LF_Test_WantedLook` (2026-11-13). Observed:
//   [LFTEST]   - the GUID of a {GUID}path name (expected '9B5BB216CC7FF18E', got '{9B5BB216CC7FF18')
//   [LFTEST]   - the character GUID alone loads its prefab: {{9B5BB216CC7FF18}
// Restored it and the test passed (12 assertions).

// tier: WORLD
//! The portrait's look (LF_WantedLook): a dressed character becomes its prefab GUID and the GUIDs
//! of what its loadout slots hold, with no path on the wire; every GUID alone loads its prefab
//! (the client spawns from it); the look parses back slot for slot; a malformed name gives no GUID.
class LF_Test_WantedLook : LF_Test
{
	override string GetName()
	{
		return "police/wanted-look";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("9B5BB216CC7FF18E", LF_WantedLook.GuidOf(LF_TestCharacterFixture.CHARACTER_PREFAB), "the GUID of a {GUID}path name");
		ctx.EqualStr("", LF_WantedLook.GuidOf("Prefabs/NoGuid.et"), "a name without a GUID gives none");
		ctx.EqualStr("", LF_WantedLook.GuidOf("{SHORT}x.et"), "a malformed GUID gives none");
		ctx.True(LF_WantedLook.ResourceOf("1234").IsEmpty(), "a short id is no resource");

		IEntity suspect = LF_TestCharacterFixture.Spawn(ctx);
		if (!suspect)
			return;

		string look = LF_WantedLook.Capture(suspect);
		ctx.True(look.StartsWith("9B5BB216CC7FF18E"), "the look starts with the character prefab's GUID: " + look);
		ctx.False(look.Contains("{") || look.Contains("/") || look.Contains(";"), "the look carries no path, brace or field separator");

		ResourceName character;
		array<int> slots = {};
		array<ResourceName> items = {};
		ctx.True(LF_WantedLook.Parse(look, character, slots, items), "the look parses back");
		ctx.True(items.Count() >= 1, "the worn jacket is in the look");
		Resource characterResource = Resource.Load(character);
		ctx.True(characterResource && characterResource.IsValid(), "the character GUID alone loads its prefab: " + character);
		bool allLoad = true;
		foreach (ResourceName item : items)
		{
			Resource itemResource = Resource.Load(item);
			if (!itemResource || !itemResource.IsValid())
				allLoad = false;
		}

		ctx.True(allLoad, "every item GUID alone loads its prefab");
		bool jacket;
		foreach (ResourceName item : items)
		{
			if (item == "{" + LF_WantedLook.GuidOf(LF_TestCharacterFixture.JACKET_PREFAB) + "}")
				jacket = true;
		}

		ctx.True(jacket, "the jacket the fixture put on is one of the items");
		ctx.False(LF_WantedLook.Parse("", character, slots, items), "an empty look draws nothing");
		LF_TestCharacterFixture.Delete(suspect);
	}
}
