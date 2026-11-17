// red-proof: 2026-11-16, cli test --no-build --red LF_Test_MapPois: RED-OK (RedProof() forces every
//   case's visibility to PUBLIC and gives each scenario a fresh audience, so the DISCOVER, ROLE and
//   HIDDEN cases and the heard-of, visited and police scenarios fail). Green without it.
//! One case of the table, as the engine's class reader fills it: the keys are the member names.
class LF_Test_MapPoiCaseInput
{
	string visibility;
	string role;
	bool live;
	string known;
	ref array<string> roles;
};

//------------------------------------------------------------------------------------------------
class LF_Test_MapPoiCase
{
	string name;
	ref LF_Test_MapPoiCaseInput input;
	string expect;
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
//! map/pois: the POI visibility rule and the list it makes (#2230, everon-map.md section 1). The case
//! table Configs/Map/Rules/visibility.cases.json is the one source: this test runs it through
//! LF_MapPoiRules and LF_MapPois.Collect, and tools/validation/map-visibility-selftest.mjs runs it
//! through the node twin tools/map/rules/visibility.mjs, so the Enforce and node rules cannot drift.
//! Also: the core POI table loads, and a kind with no row never shows.
//! The engine cannot open an addon's loose .json (it is not a resource), so the table comes from
//! LF_MapPoiCasesData, generated from the json by `node tools/map/rules/visibility.mjs --gen`; the
//! node selftest fails when the copy is stale.
class LF_Test_MapPois : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/pois";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunCoreTable(ctx);

		SCR_JsonLoadContext table = LoadTable(ctx);
		if (!table)
			return;

		RunCases(ctx, table);
		RunScenarios(ctx, table);
	}

	//------------------------------------------------------------------------------------------------
	//! The core table loads and holds the rows the design names; a kind with no row is not a POI.
	protected void RunCoreTable(LF_TestContext ctx)
	{
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		ctx.NotNull(config, "the core POI table loads");
		if (!config)
			return;

		LF_MapPoiKind bank = config.Find("Bank");
		ctx.NotNull(bank, "the core table has a Bank row");
		if (bank)
		{
			ctx.True(bank.m_eVisibility == LF_EMapPoiVisibility.PUBLIC, "Bank is public");
			ctx.EqualStr("civic", bank.m_sCategory, "Bank is civic");
		}

		LF_MapPoiKind work = config.Find("Workplace");
		ctx.True(work && work.m_eVisibility == LF_EMapPoiVisibility.DISCOVER, "a workplace is a discover kind");

		LF_MapPoiKind armoury = config.Find("PoliceArmoury");
		ctx.True(armoury && armoury.m_eVisibility == LF_EMapPoiVisibility.ROLE && armoury.m_sRole == "police", "the police armoury is a police role kind");

		ctx.True(config.Find("BankVault") == null, "a vault is not a POI: its kind has no row");
		ctx.True(config.Find("") == null, "an empty kind is not a POI");
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_JsonLoadContext LoadTable(LF_TestContext ctx)
	{
		SCR_JsonLoadContext json = new SCR_JsonLoadContext(false);
		bool parsed = json.ImportFromString(LF_MapPoiCasesData.Json());
		ctx.True(parsed, "the shared case table is valid JSON");
		if (!parsed)
			return null;

		return json;
	}

	//------------------------------------------------------------------------------------------------
	//! Each case: one visibility, one place state, one player. The cases are the loader format of
	//! tools/map/rules/index.mjs: [{ name, input, expect }].
	protected void RunCases(LF_TestContext ctx, SCR_JsonLoadContext json)
	{
		array<ref LF_Test_MapPoiCase> cases = {};
		ctx.True(json.ReadValue("cases", cases), "the table's cases read");
		ctx.True(cases.Count() >= 12, "the table holds the full case set");
		foreach (LF_Test_MapPoiCase tableCase : cases)
		{
			if (!tableCase || !tableCase.input)
			{
				ctx.Fail("a case has no input");
				continue;
			}

			LF_EMapPoiVisibility visibility = LF_MapPoiRules.ParseVisibility(tableCase.input.visibility);
			if (RedProof())
				visibility = LF_EMapPoiVisibility.PUBLIC;

			LF_EMapPoiState state = LF_MapPoiRules.Resolve(visibility, tableCase.input.role, tableCase.input.live, LF_MapPoiRules.ParseKnown(tableCase.input.known), tableCase.input.roles);
			ctx.EqualStr(tableCase.expect, LF_MapPoiRules.StateName(state), "case: " + tableCase.name);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The visible list for fixture places of three kinds, for a fresh player, one who heard of a
	//! place, and a police player (LF_MapPois.Collect over the table's own kinds and places).
	protected void RunScenarios(LF_TestContext ctx, SCR_JsonLoadContext json)
	{
		json.StartObject("lists");

		LF_MapPoiConfig config = new LF_MapPoiConfig();
		config.m_aKinds = new array<ref LF_MapPoiKind>();
		array<string> kindNames = {};
		LF_ConfigReader.ListKeys(json, "kinds", kindNames);
		ctx.Equal(4, kindNames.Count(), "the fixture lists four kinds");
		json.StartObject("kinds");
		foreach (string kindName : kindNames)
		{
			string visibilityWord;
			string role;
			json.StartObject(kindName);
			json.ReadValue("visibility", visibilityWord);
			json.ReadValue("role", role);
			json.EndObject();
			config.m_aKinds.Insert(LF_MapPoiKind.Make(kindName, LF_MapPoiRules.ParseVisibility(visibilityWord), role));
		}
		json.EndObject();

		array<ref LF_MapPoiPlace> places = {};
		array<string> placeIds = {};
		LF_ConfigReader.ListKeys(json, "places", placeIds);
		ctx.Equal(6, placeIds.Count(), "the fixture lists six places");
		json.StartObject("places");
		foreach (string placeId : placeIds)
		{
			string kind;
			bool live;
			json.StartObject(placeId);
			json.ReadValue("kind", kind);
			json.ReadValue("live", live);
			json.EndObject();
			places.Insert(LF_MapPoiPlace.Make(kind, placeId, "0 0 0", live));
		}
		json.EndObject();

		array<string> scenarioNames = {};
		LF_ConfigReader.ListKeys(json, "scenarios", scenarioNames);
		ctx.Equal(5, scenarioNames.Count(), "the fixture lists five audiences");
		json.StartObject("scenarios");
		foreach (string scenarioName : scenarioNames)
		{
			LF_MapPoiAudience audience = new LF_MapPoiAudience();
			string rolesWord;
			string expectWord;
			json.StartObject(scenarioName);
			json.ReadValue("roles", rolesWord);
			json.ReadValue("expect", expectWord);

			array<string> roles = {};
			rolesWord.Split(",", roles, true);
			foreach (string role : roles)
			{
				audience.AddRole(role);
			}

			array<string> known = {};
			LF_ConfigReader.ListKeys(json, "known", known);
			json.StartObject("known");
			foreach (string knownId : known)
			{
				string knownWord;
				json.ReadValue(knownId, knownWord);
				audience.SetKnown(knownId, LF_MapPoiRules.ParseKnown(knownWord));
			}
			json.EndObject();
			json.EndObject();

			if (RedProof())
				audience = new LF_MapPoiAudience();

			array<ref LF_MapPoi> visible = {};
			LF_MapPois.Collect(config, audience, places, visible);
			string got;
			foreach (int i, LF_MapPoi poi : visible)
			{
				if (i > 0)
					got += ",";
				got += poi.m_sId + ":" + LF_MapPoiRules.StateName(poi.m_eState);
			}
			ctx.EqualStr(expectWord, got, "scenario: " + scenarioName);
		}
		json.EndObject();

		json.EndObject();
	}
}
