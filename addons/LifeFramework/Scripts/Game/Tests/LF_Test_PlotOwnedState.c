// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_PlotOwnedState` printed `RED PROOF OK (all 1 listed test(s) fail with the hook on)` (the hook skips the sale, the save load and the owner clear, so every owned assertion fails); `--only` then passed.
//! property/plot-owned-state (#2780): a plot sign carries one replicated flag, "somebody owns this plot",
//! so a client (which has no property book) hides "Buy plot" on an owned plot instead of offering it and
//! being refused. The server refreshes the flag whenever the book changes (LF_Properties.SetOwner and
//! ApplyAll) and when a sign comes into the world after the sale (what a late joiner is sent). The Buy
//! action reads only that flag: with the book emptied, as on a client, an owned sign still hides it.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlotOwnedState : LF_Test
{
	protected static const ResourceName SIGN = "{DA9D4FEB9BD67EA5}Prefabs/Props/Property/LF_PlotSign.et";
	protected static const string PLOT = "plot-a";
	protected static const string OTHER_PLOT = "plot-b";
	protected static const string OWNER_UID = "lf-test-plot-state-owner";

	override string GetName()
	{
		return "property/plot-owned-state";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlotBuyAction BuyAction(IEntity sign)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(sign.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_PlotBuyAction buy = LF_PlotBuyAction.Cast(action);
			if (buy)
				return buy;
		}

		return null;
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
	override void Run(LF_TestContext ctx)
	{
		LF_PropertyConfig.Reload();
		LF_Properties.ResetStatics();
		ctx.True(LF_PropertyConfig.PlotPrice(PLOT) > 0, "the example plot has a price");
		ctx.True(LF_PropertyConfig.PlotPrice(OTHER_PLOT) > 0, "the neighbouring plot has a price");

		IEntity viewer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity sign;
		IEntity neighbour;
		IEntity late;
		if (viewer)
		{
			sign = LF_Utils.SpawnEntityPrefab(SIGN, viewer.GetOrigin() + "1 0 0");
			neighbour = LF_Utils.SpawnEntityPrefab(SIGN, viewer.GetOrigin() + "-1 0 0");
		}

		ctx.NotNull(sign, "the plot sign spawns");
		ctx.NotNull(neighbour, "the neighbouring plot sign spawns");
		LF_PlotComponent plot;
		LF_PlotComponent other;
		LF_PlotBuyAction action;
		if (sign && neighbour)
		{
			plot = LF_Component<LF_PlotComponent>.Find(sign);
			other = LF_Component<LF_PlotComponent>.Find(neighbour);
			action = BuyAction(sign);
		}

		ctx.NotNull(plot, "the sign carries LF_PlotComponent");
		ctx.NotNull(other, "the neighbouring sign carries LF_PlotComponent");
		ctx.NotNull(action, "the sign has the Buy plot action");
		if (!viewer || !plot || !other || !action)
		{
			Cleanup(viewer, sign, neighbour, late);
			return;
		}

		plot.SetPlotKey(PLOT);
		other.SetPlotKey(OTHER_PLOT);
		LF_PlotBuyAction otherAction = BuyAction(neighbour);
		ctx.NotNull(otherAction, "the neighbouring sign has the Buy plot action");
		if (!otherAction)
		{
			Cleanup(viewer, sign, neighbour, late);
			return;
		}

		// Nobody owns it: the plot is for sale and the action shows.
		ctx.False(plot.IsOwned(), "a plot nobody owns is not flagged owned");
		ctx.True(plot.IsForSale(), "and it is for sale");
		ctx.True(action.CanBeShownScript(viewer), "the Buy action shows on a free plot");

		// A sale: the server refreshes the sign, the action hides, the neighbour is untouched.
		Sell(PLOT, OWNER_UID);
		ctx.True(plot.IsOwned(), "the sold plot's sign is flagged owned");
		ctx.False(plot.IsForSale(), "an owned plot is not for sale");
		ctx.False(action.CanBeShownScript(viewer), "the Buy action hides on an owned plot");
		ctx.False(other.IsOwned(), "the neighbouring plot's sign is not flagged");
		ctx.True(otherAction.CanBeShownScript(viewer), "the neighbouring plot still shows Buy");

		// A client has no property book: its sign must hide Buy from the replicated flag alone.
		LF_Properties.ResetStatics();
		ctx.EqualStr("", LF_Properties.GetOwnerKey(PLOT), "the book is empty, as on a client");
		ctx.True(plot.IsOwned(), "the flag still says owned");
		ctx.False(action.CanBeShownScript(viewer), "so the client view hides Buy on the owned plot");

		// A sign placed after the sale (what a late joiner is sent with the entity) starts owned.
		Sell(PLOT, OWNER_UID);
		late = LF_Utils.SpawnEntityPrefab(SIGN, viewer.GetOrigin() + "0 0 1");
		LF_PlotComponent latePlot;
		LF_PlotBuyAction lateAction;
		if (late)
		{
			latePlot = LF_Component<LF_PlotComponent>.Find(late);
			lateAction = BuyAction(late);
		}

		ctx.NotNull(latePlot, "a sign spawned after the sale carries LF_PlotComponent");
		if (latePlot && lateAction)
		{
			latePlot.SetPlotKey(PLOT);
			ctx.True(latePlot.IsOwned(), "a sign that comes into the world after the sale is owned from the start");
			ctx.False(lateAction.CanBeShownScript(viewer), "and hides Buy");
		}

		// A save load replaces the book: the signs already in the world follow it, both ways.
		array<ref LF_PropertyRecord> rows = new array<ref LF_PropertyRecord>();
		LF_Properties.ApplyAll(rows);
		ctx.False(plot.IsOwned(), "a loaded save with no owner for the plot clears the flag");
		ctx.True(action.CanBeShownScript(viewer), "and Buy shows again");
		LF_PropertyRecord row = LF_PropertyRecord.Create(PLOT);
		row.m_sOwnerKey = OWNER_UID;
		rows.Insert(row);
		if (!RedProof())
			LF_Properties.ApplyAll(rows);

		ctx.True(plot.IsOwned(), "a loaded save with an owner sets the flag");
		ctx.False(action.CanBeShownScript(viewer), "and Buy hides");

		// Clearing the owner puts the plot back on sale.
		if (!RedProof())
			LF_Properties.SetOwner(PLOT, "");

		ctx.False(plot.IsOwned(), "a cleared owner clears the flag");
		ctx.True(action.CanBeShownScript(viewer), "and the plot is for sale again");

		LF_Properties.ResetStatics();
		Cleanup(viewer, sign, neighbour, late);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity viewer, IEntity sign, IEntity neighbour, IEntity late)
	{
		LF_TestCharacterFixture.Delete(viewer);
		LF_TestCharacterFixture.Delete(sign);
		LF_TestCharacterFixture.Delete(neighbour);
		LF_TestCharacterFixture.Delete(late);
	}
}
