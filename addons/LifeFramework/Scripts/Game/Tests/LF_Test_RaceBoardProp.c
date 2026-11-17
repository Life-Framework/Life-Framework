// red-proof: batched `tools\cli test --no-build --red LF_Test_RaceBoardProp` (the hook skips the spawn, so the read action and the title are missing); the first run before RaceRecordBoard.et existed failed "record board prefab loads".
// tier: WORLD
//! #2098: the record board prop at the race track's start line. RaceRecordBoard.et carries the read
//! action the "racerecords" board kind names and a printed title, and what it reads is the course the
//! start sign enters: a time recorded on that course comes back in the board's answer.
class LF_Test_RaceBoardProp : LF_Test
{
	protected static const ResourceName BOARD = "{3A0DF994654216AF}Prefabs/Props/Roleplay/Racing/RaceRecordBoard.et";
	protected static const string COURSE = "test_track";

	override string GetName()
	{
		return "racing/board-prop";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource resource = Resource.Load(BOARD);
		ctx.True(resource && resource.IsValid(), "record board prefab loads");
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity board;
		if (!RedProof())
			board = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);

		ctx.NotNull(board, "record board spawns");
		if (!board)
			return;

		ctx.NotNull(LF_Component<LF_SignComponent>.Find(board), "the board prints its title");

		int reads = 0;
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(board);
		if (actions)
		{
			array<BaseUserAction> actionList = {};
			actions.GetActionsList(actionList);
			foreach (BaseUserAction action : actionList)
			{
				if (LF_RaceBoardReadAction.Cast(action))
					reads++;
			}
		}

		ctx.Equal(1, reads, "the board carries one record board read action");

		// The action the prop carries is the one the kind asks the reader to stand at.
		LF_BoardProvider provider = LF_BoardProviders.Find(LF_RaceBoard.KIND);
		ctx.NotNull(provider, "the racerecords kind is registered");
		if (provider)
			ctx.EqualStr("LF_RaceBoardReadAction", provider.GetReadAction(), "the kind names the read action the prop carries");

		// What it reads: a record on the course the start sign enters is in the answer.
		array<ref LF_RaceRecordRow> before = LF_RaceRecords.ExportAll();
		LF_RaceRecords.ApplyAll(null);
		LF_RaceRecords.Offer(COURSE, "board-prop-uid", "Anna Sold", 81000);
		LF_BoardAnswer answer = LF_RaceBoard.BuildAnswer();
		ctx.Equal(1, answer.m_aNumbers.Count(), "one record reads back");
		if (answer.m_aTexts.Count() > 0)
			ctx.EqualStr(COURSE, answer.m_aTexts[0], "the record is on the start sign's course");

		LF_RaceRecords.ApplyAll(before);
		SCR_EntityHelper.DeleteEntityAndChildren(board);
	}
}
