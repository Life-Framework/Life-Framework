// red-proof: with the core-name check in LF_PackConfigSections.Problem disabled, config/pack-section failed `a section named like a core section is refused, even in the pack's own namespace` and `SUMMARY tier=only passed=0 failed=1` (observed 2026-11-14); restored, green.
// tier: LOGIC
//! A pack adds namespaced owner-config sections (#553). The pack declares them in its manifest as
//! data; the owner's config.json sets them; a bad field keeps its default; a name that clashes
//! with a core section or breaks the namespace rule is ignored; the core never reads the values.
class LF_Test_ConfigPackSection : LF_Test
{
	override string GetName()
	{
		return "config/pack-section";
	}

	protected static LF_PackConfigField Field(string key, string type, string def, string min = "", string max = "")
	{
		LF_PackConfigField field = new LF_PackConfigField();
		field.m_sKey = key;
		field.m_sType = type;
		field.m_sDefault = def;
		field.m_sMin = min;
		field.m_sMax = max;
		return field;
	}

	protected static LF_PackConfigSection Section(string name)
	{
		LF_PackConfigSection section = new LF_PackConfigSection();
		section.m_sName = name;
		section.m_aFields = {};
		section.m_aFields.Insert(Field("festivalDay", "int", "3", "1", "7"));
		section.m_aFields.Insert(Field("rainChance", "float", "0.25", "0", "1"));
		section.m_aFields.Insert(Field("chainsEnabled", "bool", "true"));
		section.m_aFields.Insert(Field("harbourName", "string", "Saint Philippe"));
		return section;
	}

	protected static void Install(array<ref LF_PackConfigSection> sections, string packId = "LF_Example")
	{
		LF_PackManifest manifest = new LF_PackManifest();
		manifest.m_sPackId = packId;
		manifest.m_aConfigSections = sections;
		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetManifest(manifest);
		LF_ConfigService.SetPackSource(source);
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		ctx.EqualStr("example", LF_PackConfigSections.NamespaceOf("LF_Example"), "the namespace is the pack id without LF_, lower case");

		array<ref LF_PackConfigSection> sections = {};
		sections.Insert(Section("example"));
		sections.Insert(Section("exampleChains"));
		Install(sections);

		// Merges: both pack sections are known keys, an unrelated key is not, core keys stay known.
		ctx.True(LF_ConfigService.IsKnownTopLevelKey("example"), "a pack section is a known top-level key");
		ctx.True(LF_ConfigService.IsKnownTopLevelKey("exampleChains"), "a second pack section is known");
		ctx.True(LF_ConfigService.IsKnownTopLevelKey("shops"), "a core section stays known");
		ctx.False(LF_ConfigService.IsKnownTopLevelKey("bogus"), "an undeclared key is unknown");

		// Defaults without an owner layer.
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test");
		LF_PackConfigValues values = LF_PackConfigSections.Read("example");
		ctx.NotNull(values, "a declared pack section reads");
		if (!values)
		{
			LF_ConfigService.Reset();
			return;
		}
		ctx.Equal(3, values.GetInt("festivalDay"), "an int field falls back to its declared default");
		ctx.EqualFloat(0.25, values.GetFloat("rainChance"), 0.001, "a float field falls back to its declared default");
		ctx.True(values.GetBool("chainsEnabled"), "a bool field falls back to its declared default");
		ctx.EqualStr("Saint Philippe", values.GetString("harbourName"), "a string field falls back to its declared default");

		// The owner sets a valid pack section; an unknown key is warned and the rest applies.
		LF_ConfigService.LoadOwnerString("{\"example\":{\"festivalDay\":5,\"chainsEnabled\":false,\"harbourName\":\"Morton\",\"mystery\":1}}", "test");
		values = LF_PackConfigSections.Read("example");
		ctx.Equal(5, values.GetInt("festivalDay"), "the owner's int applies");
		ctx.False(values.GetBool("chainsEnabled"), "the owner's bool applies");
		ctx.EqualStr("Morton", values.GetString("harbourName"), "the owner's string applies");
		ctx.EqualFloat(0.25, values.GetFloat("rainChance"), 0.001, "a field the owner omits keeps its default");
		ctx.Equal(1, values.m_iUnknown, "an unknown key in a pack section is warned");
		ctx.Equal(0, values.m_iRejected, "a valid section rejects nothing");

		// A bad pack field falls back to its default; the other fields still apply.
		LF_ConfigService.LoadOwnerString("{\"example\":{\"festivalDay\":99,\"rainChance\":\"wet\",\"chainsEnabled\":false}}", "test");
		values = LF_PackConfigSections.Read("example");
		ctx.Equal(3, values.GetInt("festivalDay"), "an int above its max keeps the default");
		ctx.EqualFloat(0.25, values.GetFloat("rainChance"), 0.001, "a string where a number belongs keeps the default");
		ctx.False(values.GetBool("chainsEnabled"), "a good field beside two bad ones still applies");
		ctx.Equal(2, values.m_iRejected, "each bad field is rejected on its own");

		// A pack section can never take a core section's name, or leave its namespace.
		// A pack whose namespace is a core section's name still cannot take that name.
		sections = {};
		sections.Insert(Section("shops"));
		Install(sections, "LF_Shops");
		LF_ConfigService.LoadOwnerString("{\"shops\":{\"defaultBuyPrice\":60}}", "test");
		ctx.True(LF_PackConfigSections.Read("shops") == null, "a section named like a core section is refused, even in the pack's own namespace");
		ctx.True(LF_ConfigService.IsKnownTopLevelKey("shops"), "the core section is still the core's");

		sections = {};
		sections.Insert(Section("other"));
		sections.Insert(Section("exampleextra"));
		sections.Insert(Section("example"));
		Install(sections);
		ctx.True(LF_PackConfigSections.Read("other") == null, "a section outside the pack namespace is refused");
		ctx.True(LF_PackConfigSections.Read("exampleextra") == null, "a section that does not continue the namespace with a capital is refused");
		ctx.NotNull(LF_PackConfigSections.Read("example"), "a good section beside refused ones stays");
		ctx.False(LF_ConfigService.IsKnownTopLevelKey("other"), "a refused pack section is not a known key");

		// A section with a bad default is refused whole.
		sections = {};
		LF_PackConfigSection broken = Section("example");
		broken.m_aFields.Insert(Field("limit", "int", "ten"));
		sections.Insert(broken);
		Install(sections);
		ctx.False(LF_PackConfigSections.IsPackSection("example"), "a section whose default is not its type is refused");

		// No pack: no pack sections, and an owner key with a pack's name is an unknown top-level key.
		LF_ConfigService.SetPackSource(null);
		ctx.False(LF_ConfigService.IsKnownTopLevelKey("example"), "without the pack the section is unknown");
		ctx.Equal(1, LF_ConfigService.WarnUnknownSections("{\"example\":{}}", "test"), "the owner key warns when no pack declares it");

		LF_ConfigService.Reset();
	}
}
