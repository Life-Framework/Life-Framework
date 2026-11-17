// red-proof: Observed 2026-11-17 `tools/cli test --no-build --only LF_Test_EvidenceBagHeld` on the unfixed LF_Evidence.Bag failed `investigation/bag-held` with `- an item a civilian holds again is not bagged`, `- the item stays with its holder`, `- an item carried away from its marker is not bagged`, `- the moved item stays where it is`; the loose-item case passed. Fixed: green.
// tier: WORLD
//! #2419: an officer bags a dropped item only while it still lies beside its marker. An item a
//! civilian picked back up (it has a parent) or carried away (far from its marker) is refused, the
//! row is dropped, and the item stays where it is.
class LF_Test_EvidenceBagHeld : LF_Test
{
	protected static const vector SCENE = "1000 0 1000";
	protected static const int T0 = 1000000;
	protected vector m_vSpot;

	override string GetName()
	{
		return "investigation/bag-held";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Evidence.ResetStatics();
		// The scene opens on the ground under SCENE: the radius check is 3D, so the terrain height counts.
		m_vSpot = LF_ScenePieces.GroundSpot(SCENE, 0.2);
		LF_Evidence.OpenScene(31, "evidence-held-uid", LF_WantedReports.OFFENCE_ROBBERY, "Test Square", m_vSpot, T0);

		RunLoose(ctx);
		RunHeld(ctx);
		RunMoved(ctx);

		LF_Evidence.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity Drop(LF_TestContext ctx, out IEntity item)
	{
		item = LF_Utils.SpawnEntityPrefab(LF_Contraband.ITEM, m_vSpot);
		ctx.NotNull(item, "a dropped item spawns");
		if (!item)
			return null;

		IEntity marker = LF_Evidence.OnDropped(item, T0 + 1000);
		ctx.NotNull(marker, "the item dropped in the scene gets a marker");
		return marker;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLoose(LF_TestContext ctx)
	{
		IEntity item;
		IEntity marker = Drop(ctx, item);
		if (!marker)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return;
		}

		IEntity bag = LF_Evidence.Bag(marker, null, T0 + 2000);
		ctx.NotNull(bag, "an item still lying by its marker is bagged");
		if (bag)
		{
			ctx.True(item.GetParent() == bag, "the loose item rides inside the bag");
			SCR_EntityHelper.DeleteEntityAndChildren(bag);
			return;
		}

		SCR_EntityHelper.DeleteEntityAndChildren(item);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHeld(LF_TestContext ctx)
	{
		IEntity item;
		IEntity marker = Drop(ctx, item);
		if (!marker)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return;
		}

		// The civilian picked it back up: it now has a holder.
		IEntity holder = LF_Utils.SpawnEntityPrefab(LF_Contraband.ITEM, LF_ScenePieces.GroundSpot(m_vSpot + "5 0 5", 0.2));
		ctx.NotNull(holder, "a holder spawns");
		if (holder)
			holder.AddChild(item, -1, EAddChildFlags.AUTO_TRANSFORM);

		ctx.True(LF_Evidence.Bag(marker, null, T0 + 2000) == null, "an item a civilian holds again is not bagged");
		ctx.Equal(0, LF_Evidence.EvidenceCount(), "the refused row is dropped");
		if (holder)
		{
			ctx.True(item.GetParent() == holder, "the item stays with its holder");
			SCR_EntityHelper.DeleteEntityAndChildren(holder);
			return;
		}

		SCR_EntityHelper.DeleteEntityAndChildren(item);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunMoved(LF_TestContext ctx)
	{
		IEntity item;
		IEntity marker = Drop(ctx, item);
		if (!marker)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return;
		}

		item.SetOrigin(item.GetOrigin() + "20 0 0");
		ctx.True(LF_Evidence.Bag(marker, null, T0 + 2000) == null, "an item carried away from its marker is not bagged");
		ctx.Equal(0, LF_Evidence.EvidenceCount(), "the moved item's row is dropped");
		ctx.True(item.GetParent() == null, "the moved item stays where it is");
		SCR_EntityHelper.DeleteEntityAndChildren(item);
	}
}
