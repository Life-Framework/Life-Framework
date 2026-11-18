// red-proof: 2026-11-18, cli test --no-build --red LF_Test_MapZoomEndCache: RED PROOF OK (RedProof() skips the palette Build, so every style's same-records check fails: expected 31, got 0). Green without it.

//------------------------------------------------------------------------------------------------
//! map/zoom-end-cache (#2397, epic #2223): the two caches that take a map zoom end from 8 to 12 ms down.
//! The palette gives the colour the uncached `LF_MapAreasDraw.ColorOf` gives (themed or built-in) and parses a
//! token once; an area built through it has the same records as one built without it. The icon state tells a
//! widget whether its pick, its fade or its count changed since the last style, so a rebuild writes only that.
//! Pure script: no widget, no map.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapZoomEndCache : LF_Test
{
	protected static const float VIEW_W = 1280;
	protected static const float VIEW_H = 720;

	override string GetName()
	{
		return "map/zoom-end-cache";
	}

	override void Run(LF_TestContext ctx)
	{
		RunPalette(ctx);
		RunSameRecords(ctx);
		RunIconState(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPalette(LF_TestContext ctx)
	{
		string themed = "map-area-safe-fill=#102030;map-area-safe-line=#80FF0000";
		LF_MapAreaPalette palette = new LF_MapAreaPalette(themed);
		ctx.Equal(LF_MapAreasDraw.ColorOf("map-area-safe-fill", 15, themed), palette.ColorOf("map-area-safe-fill", 15), "a themed fill colour is the uncached one");
		ctx.Equal(LF_MapAreasDraw.ColorOf("map-area-safe-line", 100, themed), palette.ColorOf("map-area-safe-line", 100), "a themed line colour with its own alpha is the uncached one");
		ctx.Equal(LF_MapAreasDraw.ColorOf("map-area-lawless-fill", 25, themed), palette.ColorOf("map-area-lawless-fill", 25), "a token the theme does not name keeps the built-in");
		int first = palette.ColorOf("map-area-safe-fill", 15);
		int second = palette.ColorOf("map-area-safe-fill", 15);
		ctx.Equal(first, second, "asking twice gives the same colour");
		ctx.True(palette.ColorOf("map-area-safe-fill", 15) != palette.ColorOf("map-area-safe-fill", 100), "the same token at another alpha is another colour");

		// A palette is bound to its theme string: a new pass with a new theme sees the new colour.
		LF_MapAreaPalette other = new LF_MapAreaPalette("map-area-safe-fill=#FFFFFF");
		ctx.True(other.ColorOf("map-area-safe-fill", 15) != palette.ColorOf("map-area-safe-fill", 15), "another theme string gives another colour");
	}

	//------------------------------------------------------------------------------------------------
	//! Every style, built with and without a shared palette: the same number of records, with the same colours.
	protected void RunSameRecords(LF_TestContext ctx)
	{
		string themed = "map-area-lawless-line=#00FF00";
		LF_MapAreaPalette palette = new LF_MapAreaPalette(themed);
		for (int style = 0; style < 6; style++)
		{
			LF_MapArea area = new LF_MapArea();
			area.m_sName = "fixture-" + style.ToString();
			area.m_eStyle = style;
			area.m_sLabel = "Fixture";
			LF_MapAreas.CircleOutline(Vector(0, 0, 0), 300, LF_MapAreas.DEFAULT_SEGMENTS, area.m_aOutline);
			array<vector> screen = {};
			foreach (vector world : area.m_aOutline)
				screen.Insert(Vector(VIEW_W / 2 + world[0] * 0.4, VIEW_H / 2 - world[2] * 0.4, 0));

			array<ref LF_MapAreaCmd> plain = {};
			array<ref LF_MapAreaCmd> shared = {};
			int plainCount = LF_MapAreasDraw.Build(area, screen, 0.4, VIEW_W, VIEW_H, themed, "Fixture", plain);
			int sharedCount;
			if (!RedProof())
				sharedCount = LF_MapAreasDraw.Build(area, screen, 0.4, VIEW_W, VIEW_H, themed, "Fixture", shared, palette);

			ctx.Equal(plainCount, sharedCount, "style " + style.ToString() + ": the same number of records through a palette");
			if (plain.Count() != shared.Count())
				continue;

			bool same = true;
			foreach (int at, LF_MapAreaCmd cmd : plain)
			{
				if (cmd.m_iColor != shared[at].m_iColor || cmd.m_eKind != shared[at].m_eKind || cmd.m_aVertices.Count() != shared[at].m_aVertices.Count())
					same = false;
			}

			ctx.True(same, "style " + style.ToString() + ": the same kinds, colours and vertex counts through a palette");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RunIconState(LF_TestContext ctx)
	{
		LF_MapIconState state = new LF_MapIconState();
		LF_MapIconPick pick = new LF_MapIconPick();
		pick.m_sQuad = "cat_civic";
		ctx.True(state.PickChanged(pick), "a fresh icon has shown nothing: its pick changed");
		ctx.True(state.CountChanged(0), "a fresh icon has set no count: a single place changes it");
		ctx.False(state.FadedChanged(false), "a fresh icon is at full opacity: not faded changes nothing");
		ctx.True(state.FadedChanged(true), "a fresh icon becoming faded changes it");

		state.m_Pick = pick;
		state.m_iCount = 0;
		state.m_bFaded = true;
		ctx.False(state.PickChanged(pick), "the same pick again changes nothing");
		ctx.False(state.CountChanged(0), "the same count again changes nothing");
		ctx.False(state.FadedChanged(true), "the same fade again changes nothing");

		LF_MapIconPick next = new LF_MapIconPick();
		next.m_sQuad = "cat_civic";
		ctx.True(state.PickChanged(next), "another pick object (a new lookup) is a change, even for the same name");
		ctx.True(state.CountChanged(7), "a stamp of seven changes the count");
		ctx.True(state.FadedChanged(false), "a place that is no longer faded changes the opacity");

		LF_MapPoi place = new LF_MapPoi();
		LF_MapPoi other = new LF_MapPoi();
		ctx.False(state.SamePlace(place, LF_EMapPoiState.FULL), "a fresh icon shows no place yet");
		state.m_Poi = place;
		state.m_ePoiState = LF_EMapPoiState.FULL;
		ctx.True(state.SamePlace(place, LF_EMapPoiState.FULL), "the same place in the same state needs no restyle");
		ctx.False(state.SamePlace(place, LF_EMapPoiState.FADED), "the same place heard of instead of visited does");
		ctx.False(state.SamePlace(other, LF_EMapPoiState.FULL), "another place does");
		ctx.False(state.SamePlace(null, LF_EMapPoiState.FULL), "a stamp (no single place) always restyles");
	}
}
