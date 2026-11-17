// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapPlacesKnown: RED PROOF OK (RedProof() leaves the player knowing nothing and skips the board and tourist marks, so the faded and board cases and the tourist-map marks fail). Green without it.
// tier: LOGIC
//! map/places-known: discovery on the map (#2242, everon-map.md section 4). A DISCOVER place is hidden for
//! a player who knows nothing, a faded pencil "?" once heard of, the full icon once visited, through the
//! real POI list and cluster rule; a town board card whose place is a map POI marks it heard of; the
//! clerk's arrival marks reach the DISCOVER places near the square and the tourist map reaches all of
//! them, never a role, public or underworld kind or a dark place; the open map rebuilds when the known
//! set changes. Fixture places only: no real Everon name.
class LF_Test_MapPlacesKnown : LF_Test
{
	protected static const string CHARACTER = "mpk-test-character";
	protected static const string KIND = "MpkKind";
	protected static const string PUBLIC_KIND = "MpkPublicKind";
	protected static const string NONE_KIND = "MpkNotAPoi";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/places-known";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_MapPoiConfig MakeConfig()
	{
		LF_MapPoiConfig config = new LF_MapPoiConfig();
		config.m_aKinds = {};
		config.m_aKinds.Insert(LF_MapPoiKind.Make(KIND, LF_EMapPoiVisibility.DISCOVER));
		config.m_aKinds.Insert(LF_MapPoiKind.Make(PUBLIC_KIND, LF_EMapPoiVisibility.PUBLIC));
		config.m_aKinds.Insert(LF_MapPoiKind.Make("MpkHidden", LF_EMapPoiVisibility.HIDDEN));
		config.m_aKinds.Insert(LF_MapPoiKind.Make("MpkRole", LF_EMapPoiVisibility.ROLE, "police"));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_MapPoiPlace> MakePlaces()
	{
		array<ref LF_MapPoiPlace> places = {};
		places.Insert(LF_MapPoiPlace.Make(KIND, "mpk-near", Vector(100, 0, 0), true));
		places.Insert(LF_MapPoiPlace.Make(KIND, "mpk-far", Vector(2500, 0, 0), true));
		places.Insert(LF_MapPoiPlace.Make(KIND, "mpk-dark", Vector(50, 0, 0), false));
		places.Insert(LF_MapPoiPlace.Make(PUBLIC_KIND, "mpk-public", Vector(60, 0, 0), true));
		places.Insert(LF_MapPoiPlace.Make("MpkHidden", "mpk-hidden", Vector(70, 0, 0), true));
		places.Insert(LF_MapPoiPlace.Make("MpkRole", "mpk-role", Vector(80, 0, 0), true));
		return places;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_MapKnownRow> saved = LF_MapKnown.ExportAll();
		LF_MapKnown.ApplyAll(null);
		LF_MapPoiConfig config = MakeConfig();
		RunStates(ctx, config);
		RunBoard(ctx, config);
		RunMarks(ctx, config);
		LF_MapKnown.ApplyAll(saved);
	}

	//------------------------------------------------------------------------------------------------
	//! Hidden, faded, full: the real list and the real cluster rule, for the three things a player may know.
	protected void RunStates(LF_TestContext ctx, LF_MapPoiConfig config)
	{
		array<ref LF_MapPoiPlace> places = {};
		places.Insert(LF_MapPoiPlace.Make(KIND, "mpk-shop", Vector(100, 0, 100), true));

		LF_MapPoiAudience newcomer = new LF_MapPoiAudience();
		array<ref LF_MapPoi> seen = {};
		LF_MapPois.Collect(config, newcomer, places, seen);
		ctx.Equal(0, seen.Count(), "a place nobody told the player of is hidden");
		ctx.True(LF_MapPlacesKnown.StateOf(LF_EMapPoiKnown.NONE) == LF_EMapPoiState.HIDDEN, "unknown draws hidden");

		LF_MapPoiAudience heard = new LF_MapPoiAudience();
		if (!RedProof())
			heard.SetKnown("mpk-shop", LF_EMapPoiKnown.HEARD);

		seen.Clear();
		LF_MapPois.Collect(config, heard, places, seen);
		ctx.Equal(1, seen.Count(), "a heard-of place is listed");
		if (seen.Count() == 1)
			ctx.True(seen[0].m_eState == LF_EMapPoiState.FADED, "a heard-of place is faded");

		ctx.True(LF_MapPlacesKnown.StateOf(LF_EMapPoiKnown.HEARD) == LF_EMapPoiState.FADED, "heard of draws faded");

		array<ref LF_MapClusterGroup> groups = {};
		LF_MapClusterRules.BuildFromPois(seen, 5, groups);
		ctx.Equal(1, groups.Count(), "the heard-of place is one icon at a close zoom");
		if (groups.Count() == 1)
			ctx.True(groups[0].m_eState == LF_EMapPoiState.FADED, "the icon stays faded");

		array<string> candidates = {};
		LF_MapPlacesKnown.Candidates(LF_EMapPoiState.FADED, KIND, "", "civic", candidates);
		ctx.True(candidates.Count() > 0 && candidates[0] == LF_MapPlacesKnown.HEARD_QUAD, "a faded place tries the pencil question mark first");
		candidates.Clear();
		LF_MapPlacesKnown.Candidates(LF_EMapPoiState.FULL, KIND, "", "civic", candidates);
		ctx.True(candidates.Find(LF_MapPlacesKnown.HEARD_QUAD) == -1, "a full place never draws the pencil mark");

		LF_MapPoiAudience visited = new LF_MapPoiAudience();
		visited.SetKnown("mpk-shop", LF_EMapPoiKnown.HEARD);
		visited.SetKnown("mpk-shop", LF_EMapPoiKnown.VISITED);
		visited.SetKnown("mpk-shop", LF_EMapPoiKnown.HEARD);
		seen.Clear();
		LF_MapPois.Collect(config, visited, places, seen);
		ctx.Equal(1, seen.Count(), "a visited place is listed");
		if (seen.Count() == 1)
			ctx.True(seen[0].m_eState == LF_EMapPoiState.FULL, "a visited place is full, and hearing again does not fade it");

		ctx.True(LF_MapPlacesKnown.StateOf(LF_EMapPoiKnown.VISITED) == LF_EMapPoiState.FULL, "visited draws full");

		// The open map rebuilds exactly when the known set changes.
		array<ref LF_MapPoi> none = {};
		array<ref LF_MapPoi> fadedList = {};
		array<ref LF_MapPoi> fullList = {};
		LF_MapPois.Collect(config, newcomer, places, none);
		LF_MapPois.Collect(config, heard, places, fadedList);
		LF_MapPois.Collect(config, visited, places, fullList);
		string hiddenSig = LF_MapPlacesKnown.Signature(none);
		string fadedSig = LF_MapPlacesKnown.Signature(fadedList);
		string fullSig = LF_MapPlacesKnown.Signature(fullList);
		ctx.True(hiddenSig != fadedSig, "hearing of a place changes what the open map has to draw");
		ctx.True(fadedSig != fullSig, "visiting a heard-of place changes what the open map has to draw");
		ctx.EqualStr(fullSig, LF_MapPlacesKnown.Signature(fullList), "an unchanged set has an unchanged signature");
	}

	//------------------------------------------------------------------------------------------------
	//! A board card read marks the card's place heard of, when that place is a map POI.
	protected void RunBoard(LF_TestContext ctx, LF_MapPoiConfig config)
	{
		LF_TownBoardConfig board = new LF_TownBoardConfig();
		board.m_aCards = {};
		board.m_aCards.Insert(MakeCard("mpk-a", KIND, "mpk-near"));
		board.m_aCards.Insert(MakeCard("mpk-b", NONE_KIND, "mpk-far"));
		board.m_aCards.Insert(MakeCard("mpk-c", KIND, ""));
		LF_TownBoard.UseConfig(board);

		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "mpk-near") == LF_EMapPoiKnown.NONE, "before the read the place is unknown");
		if (!RedProof())
		{
			LF_EMapKnownOutcome outcome = LF_TownBoardMapKnown.HearCard(config, CHARACTER, 0);
			ctx.True(outcome == LF_EMapKnownOutcome.HEARD, "reading a card of a map POI records heard of");
		}

		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "mpk-near") == LF_EMapPoiKnown.HEARD, "the card's place is heard of");
		ctx.True(LF_TownBoardMapKnown.HearCard(config, CHARACTER, 0) == LF_EMapKnownOutcome.KEPT, "reading the card again changes nothing");

		ctx.True(LF_TownBoardMapKnown.HearCard(config, CHARACTER, 1) == LF_EMapKnownOutcome.REFUSED_BAD, "a card whose place is not a map POI marks nothing");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "mpk-far") == LF_EMapPoiKnown.NONE, "that card's place stays unknown");
		ctx.True(LF_TownBoardMapKnown.HearCard(config, CHARACTER, 2) == LF_EMapKnownOutcome.REFUSED_BAD, "a card with no place marks nothing");
		ctx.True(LF_TownBoardMapKnown.HearCard(config, CHARACTER, 9) == LF_EMapKnownOutcome.REFUSED_BAD, "a slot with no card marks nothing");

		ctx.True(LF_TownBoardMapKnown.Admit(7, 1000), "the first ask is admitted");
		ctx.False(LF_TownBoardMapKnown.Admit(7, 1000 + LF_TownBoardMapKnown.MIN_GAP_MS - 1), "an ask inside the gap is not");
		ctx.True(LF_TownBoardMapKnown.Admit(7, 1000 + LF_TownBoardMapKnown.MIN_GAP_MS), "an ask after the gap is");
		LF_TownBoardMapKnown.ResetStatics();

		LF_TownBoard.UseConfig(null);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_TownBoardCard MakeCard(string id, string kind, string placeId)
	{
		LF_TownBoardCard card = new LF_TownBoardCard();
		card.m_sId = id;
		card.m_sTitle = "Title " + id;
		card.m_sPlaceKind = kind;
		card.m_sPlaceId = placeId;
		return card;
	}

	//------------------------------------------------------------------------------------------------
	//! The clerk's arrival marks (near the square) and the tourist map (everywhere).
	protected void RunMarks(LF_TestContext ctx, LF_MapPoiConfig config)
	{
		array<ref LF_MapPoiPlace> places = MakePlaces();
		vector square = Vector(0, 0, 0);

		array<string> near = {};
		ctx.Equal(1, LF_MapPlacesKnown.PlacesToHear(config, places, square, LF_MapPlacesKnown.ARRIVAL_RADIUS, near), "the clerk marks the one discover place near the square");
		ctx.True(near.Find("mpk-near") != -1, "the place on the square is marked");
		ctx.True(near.Find("mpk-far") == -1, "a place across the island is not");
		ctx.True(near.Find("mpk-dark") == -1, "a dark place is not");
		ctx.True(near.Find("mpk-public") == -1 && near.Find("mpk-hidden") == -1 && near.Find("mpk-role") == -1, "public, underworld and role places are not marked by a map");

		array<string> all = {};
		ctx.Equal(2, LF_MapPlacesKnown.PlacesToHear(config, places, square, 0, all), "the tourist map marks every live discover place");
		ctx.True(all.Find("mpk-far") != -1, "including one across the island");

		ctx.True(LF_MapPlacesKnown.IsArrivalDone("arrival_done"), "the arrival chain's last step ends with the marks");
		ctx.False(LF_MapPlacesKnown.IsArrivalDone("arrival_bank"), "an earlier step marks nothing");

		int fresh = 0;
		if (!RedProof())
			fresh = LF_MapPlacesKnown.HearAll(CHARACTER, all);

		ctx.Equal(1, fresh, "one of the two was already heard of from the board, so one is new");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "mpk-far") == LF_EMapPoiKnown.HEARD, "the tourist map's place is heard of");
		ctx.Equal(0, LF_MapPlacesKnown.HearAll(CHARACTER, all), "a second reading marks nothing new");

		LF_MapKnown.VisitAsk(1, CHARACTER, "mpk-near", true, 2, 0, 5000000);
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "mpk-near") == LF_EMapPoiKnown.VISITED, "visiting a heard-of place makes it visited");
		ctx.Equal(0, LF_MapPlacesKnown.HearAll(CHARACTER, all), "the tourist map never lowers a visited place");
		ctx.True(LF_MapKnown.KnownOf(CHARACTER, "mpk-near") == LF_EMapPoiKnown.VISITED, "the place stays visited");
	}
}
