// red-proof: [LFTEST] RED-OK townboard/board-world (fails with the hook on, 39 assertions) (tools/cli test --no-build --red, 2026-11-16)
// tier: WORLD
//! The town board prop: one card context over each face slot (where LF_TownBoardFace draws the card),
//! each carrying the read action for its own slot, an empty slot offering nothing; and a card pointed
//! at a placed sign finds it through LF_Places and reads a distance and a direction from the board.
class LF_Test_TownBoardWorld : LF_Test
{
	static const ResourceName PREFAB = "{EE587A113AF85FC5}Prefabs/Props/Signs/LF_TownBoard.et";

	override string GetName()
	{
		return "townboard/board-world";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_TownBoardConfig config = new LF_TownBoardConfig();
		config.m_aCards = {};
		for (int i = 0; i < 3; i++)
		{
			LF_TownBoardCard card = new LF_TownBoardCard();
			card.m_sId = "c" + i.ToString();
			card.m_sTitle = "Card " + i.ToString();
			card.m_sPlaceKind = LF_SignComponent.PLACE_KIND;
			card.m_sPlaceId = "post_office";
			config.m_aCards.Insert(card);
		}

		LF_TownBoard.UseConfig(config);
		IEntity board = LF_Utils.SpawnEntityPrefab(PREFAB, "0 -50 0");
		ctx.True(board != null, "the town board spawns");
		if (!board)
		{
			LF_TownBoard.UseConfig(null);
			return;
		}

		LF_TownBoardComponent face = LF_TownBoardComponent.Cast(board.FindComponent(LF_TownBoardComponent));
		ctx.True(face != null, "the board draws a face");
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(board.FindComponent(BaseActionsManagerComponent));
		ctx.True(manager != null, "the board has an actions manager");
		if (face && manager)
		{
			array<UserActionContext> contexts = {};
			manager.GetContextList(contexts);
			int cards = 0;
			int shown = 0;
			foreach (UserActionContext context : contexts)
			{
				if (!context.GetContextName().StartsWith("LF_TownCard"))
					continue;

				array<BaseUserAction> actions = {};
				context.GetActionsList(actions);
				LF_TownBoardCardAction read;
				if (actions.Count() == 1)
					read = LF_TownBoardCardAction.Cast(actions[0]);

				ctx.True(read != null, "context " + context.GetContextName() + " carries one card read action");
				if (!read)
					continue;

				cards++;
				int slot = read.GetSlot();
				ctx.EqualStr("LF_TownCard" + slot.ToString(), context.GetContextName(), "the action reads its own context's slot");
				vector mat[4];
				context.GetTransformationModel(mat);
				vector expected = face.GetFaceOffset() + LF_TownBoardFace.SlotCentreMeters(slot, face.GetFaceScale());
				float off = vector.Distance(Vector(mat[3][0], mat[3][1], 0), Vector(expected[0], expected[1], 0));
				ctx.True(off < 0.02, string.Format("card %1's context sits on its card (%2 m off)", slot, off));
				ctx.True(mat[3][2] < face.GetFaceOffset()[2], "card " + slot.ToString() + "'s context is in front of the cork");
				if (read.CanBeShownScript(null))
					shown++;
			}

			ctx.Equal(LF_TownBoard.MAX_CARDS, cards, "a context for every slot");
			ctx.Equal(3, shown, "three cards offer three reads; the empty slots offer nothing");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(board);

		// The card's place is the post office sign, found by component.
		vector place;
		bool found = false;
		if (!RedProof())
			found = LF_TownBoard.FindPlace(LF_TownBoard.CardAt(0), place);

		ctx.True(found, "a card pointed at the post office sign finds it through LF_Places");
		if (found)
		{
			vector from = place + Vector(0, 0, -100);
			string where = LF_TownBoard.WhereText(LF_TownBoard.CardAt(0), from);
			ctx.True(where.Contains("100") && where.Contains(LF_TownBoard.Text("#LF-TownBoard_Dir_N")), "100 m south of it, the card reads 100 m north: " + where);
			ctx.True(LF_Waypoint.Mark("c0", LF_TownBoard.PlaceName(LF_TownBoard.CardAt(0)), place), "the place can be marked");
			ctx.True(LF_Waypoint.Arrive(place + Vector(2, 0, 2)), "standing at the sign arrives");
		}

		LF_Waypoint.ResetStatics();
		LF_TownBoard.UseConfig(null);
	}
}
