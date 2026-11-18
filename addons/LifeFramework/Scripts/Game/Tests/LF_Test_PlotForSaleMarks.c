// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_PlotForSaleMarks` printed `RED PROOF OK (all 1 listed test(s) fail with the hook on)` (the hook skips the sale and the owner clear, so the owned-plot assertions fail); `--only` passed with 23 assertions.
//! property/plot-for-sale-marks (#2984): a plot sign nobody owns registers in LF_Places as kind PlotForSale
//! (id = plot key), so the map draws a mark for it from its POI row and a town board card can point at the
//! nearest one; the mark goes once the plot is owned and comes back if the owner is cleared. No coordinate
//! or map name is involved: the placed signs are the plots.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlotForSaleMarks : LF_Test
{
	protected static const ResourceName SIGN = "{DA9D4FEB9BD67EA5}Prefabs/Props/Property/LF_PlotSign.et";
	protected static const string PLOT = "plot-a";
	protected static const string OTHER_PLOT = "plot-b";
	protected static const string OWNER_UID = "lf-test-plot-marks-owner";

	override string GetName()
	{
		return "property/plot-for-sale-marks";
	}

	//------------------------------------------------------------------------------------------------
	//! The owner change under test; the red proof skips it.
	protected void Sell(string plotKey, string ownerKey)
	{
		if (RedProof())
			return;

		LF_Properties.Create(plotKey);
		LF_Properties.SetOwner(plotKey, ownerKey);
	}

	//------------------------------------------------------------------------------------------------
	//! The plot ids the map would draw now: the PlotForSale places a POI row turns into marks.
	protected static void MapIds(notnull LF_MapPoiConfig config, notnull array<string> ids)
	{
		array<ref LF_MapPoiPlace> places = {};
		LF_MapPois.GatherKind(LF_PlotComponent.PLACE_KIND, places);
		array<ref LF_MapPoi> pois = {};
		LF_MapPois.Collect(config, new LF_MapPoiAudience(), places, pois);
		foreach (LF_MapPoi poi : pois)
		{
			if (poi.m_eState == LF_EMapPoiState.FULL)
				ids.Insert(poi.m_sId);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PropertyConfig.Reload();
		LF_Properties.ResetStatics();
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		ctx.NotNull(config, "the POI table loads");
		LF_MapPoiKind row;
		if (config)
			row = config.Find(LF_PlotComponent.PLACE_KIND);

		ctx.NotNull(row, "the core POI table has a row for plots for sale");
		if (row)
			ctx.True(row.m_eVisibility == LF_EMapPoiVisibility.PUBLIC, "every player sees it, no discovery needed");

		IEntity viewer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity sign;
		IEntity neighbour;
		if (viewer)
		{
			sign = LF_Utils.SpawnEntityPrefab(SIGN, viewer.GetOrigin() + "1 0 0");
			neighbour = LF_Utils.SpawnEntityPrefab(SIGN, viewer.GetOrigin() + "-1 0 0");
		}

		LF_PlotComponent plot;
		LF_PlotComponent other;
		if (sign && neighbour)
		{
			plot = LF_Component<LF_PlotComponent>.Find(sign);
			other = LF_Component<LF_PlotComponent>.Find(neighbour);
		}

		ctx.NotNull(plot, "the plot sign carries LF_PlotComponent");
		ctx.NotNull(other, "the neighbouring plot sign carries LF_PlotComponent");
		if (!config || !row || !plot || !other)
		{
			Cleanup(viewer, sign, neighbour);
			return;
		}

		plot.SetPlotKey(PLOT);
		other.SetPlotKey(OTHER_PLOT);

		// Both unowned: both are places of the kind and both are map marks.
		ctx.True(plot.IsListed(), "an unowned plot is listed");
		ctx.True(other.IsListed(), "the neighbouring unowned plot is listed");
		ctx.True(LF_Places.Find(LF_PlotComponent.PLACE_KIND, PLOT) == sign, "LF_Places finds the sign by its plot key");
		array<string> ids = {};
		MapIds(config, ids);
		ctx.True(ids.Find(PLOT) != -1, "the map draws a mark for the plot");
		ctx.True(ids.Find(OTHER_PLOT) != -1, "and one for its neighbour");

		// A card that names the kind points at the nearest plot (from the east, the sign at +1 m).
		LF_TownBoardCard card = new LF_TownBoardCard();
		card.m_sId = "plots";
		card.m_sPlaceKind = LF_PlotComponent.PLACE_KIND;
		card.m_sPlaceId = LF_TownBoard.PLACE_NEAREST;
		vector found;
		vector from = viewer.GetOrigin() + "20 0 0";
		ctx.True(LF_TownBoard.FindPlace(card, found, from), "a card for the kind finds a plot");
		ctx.True(vector.DistanceXZ(found, sign.GetOrigin()) < 0.1, "it is the one nearest the board");
		string heard;
		ctx.False(LF_TownBoardMapKnown.PoiPlaceOf(config, card, heard), "reading the card marks no single plot as heard of");

		// A sale: the mark goes, the neighbour's stays, and the card turns to the neighbour.
		Sell(PLOT, OWNER_UID);
		ctx.False(plot.IsListed(), "a sold plot is not listed");
		ctx.True(LF_Places.Find(LF_PlotComponent.PLACE_KIND, PLOT) == null, "LF_Places no longer holds it");
		ids.Clear();
		MapIds(config, ids);
		ctx.True(ids.Find(PLOT) == -1, "the map hides the mark once the plot is owned");
		ctx.True(ids.Find(OTHER_PLOT) != -1, "the neighbour's mark stays");
		ctx.True(other.IsListed(), "and the neighbour is still listed");
		ctx.True(LF_TownBoard.FindPlace(card, found, from), "the card still finds a plot");
		ctx.True(vector.DistanceXZ(found, neighbour.GetOrigin()) < 0.1, "now the one that is still for sale");

		// Both sold: nothing to point at.
		Sell(OTHER_PLOT, OWNER_UID);
		ctx.False(LF_TownBoard.FindPlace(card, found, from), "with every plot sold the card finds no place");

		// A cleared owner puts the plot back on the map.
		if (!RedProof())
		{
			LF_Properties.SetOwner(PLOT, "");
			LF_Properties.SetOwner(OTHER_PLOT, "");
		}

		ctx.True(plot.IsListed(), "a plot whose owner was cleared is listed again");
		ids.Clear();
		MapIds(config, ids);
		ctx.True(ids.Find(PLOT) != -1, "and drawn again");

		LF_Properties.ResetStatics();
		Cleanup(viewer, sign, neighbour);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity viewer, IEntity sign, IEntity neighbour)
	{
		LF_TestCharacterFixture.Delete(viewer);
		LF_TestCharacterFixture.Delete(sign);
		LF_TestCharacterFixture.Delete(neighbour);
	}
}
