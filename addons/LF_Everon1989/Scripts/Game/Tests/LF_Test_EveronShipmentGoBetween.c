// red-proof: RedProof() skips the spawn, so `Hugo Valmy spawns` fails (and the checks that need his body are skipped); batched run `tools\cli test --no-build --red LF_Test_EveronShipmentGoBetween`. Without the new pack files the dialogue id does not resolve, the resident row is missing and the spawn finds no prefab.
//! #3804 (The shipment, #1036; cast row `shipment_go_between` of the story sheet #3438): under the Everon 1989
//! header (the pack leg of `tools\cli test`) Hugo Valmy, the go-between, is a dressed character with a talk action,
//! his dialogue id resolves through the pack's index folder overlay, the talk is playable and captioned, and its
//! replies follow the shipment quests (the sell intro once the crate is opened, the sold and the reported answers).
//! He is a resident: a row that works the pub (`CafeTable`) and sits on its bench, found through the residents
//! config, never by a town coordinate.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronShipmentGoBetween : LF_Test
{
	static const ResourceName HUGO = "{3804A00000000001}Configs/Npcs/LF_Npc_Townsfolk_ShipmentGoBetween.et";
	static const string RESIDENT_ID = "shipment_go_between";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/shipment-go-between";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasTalkAction(IEntity npc)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(npc.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return false;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (LF_DialogueTalkAction.Cast(action))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_DialogueOption FindReply(LF_DialogueNode node, string next, string condition, string param)
	{
		if (!node || !node.m_aOptions)
			return null;

		foreach (LF_DialogueOption option : node.m_aOptions)
		{
			if (option.m_sNext == next && option.m_sCondition == condition && option.m_sParam == param)
				return option;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckDialogue(LF_TestContext ctx, LF_DialogueConfig config)
	{
		ctx.EqualStr(RESIDENT_ID, config.m_sId, "the dialogue is the go-between's");
		ctx.EqualStr("Hugo Valmy", WidgetManager.Translate(config.m_sSpeakerKey), "the speaker is Hugo Valmy");

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(0, problems.Count(), "the go-between's dialogue is playable");

		// Captions first: every line and every reply has its text (the voices come with #3444).
		foreach (LF_DialogueLine each : config.m_aLines)
		{
			string caption = WidgetManager.Translate(each.m_sTextKey);
			ctx.True(caption != each.m_sTextKey && !caption.IsEmpty(), each.m_sId + " has its caption in the string table");
		}

		foreach (LF_DialogueNode node : config.m_aNodes)
		{
			if (!node.m_aOptions)
				continue;

			foreach (LF_DialogueOption option : node.m_aOptions)
			{
				string text = WidgetManager.Translate(option.m_sTextKey);
				ctx.True(text != option.m_sTextKey && !text.IsEmpty(), "reply " + option.m_sTextKey + " has its text");
			}
		}

		// The replies follow the shipment quests; each condition hook has its class.
		LF_DialogueNode start = LF_DialogueFlow.FindNode(config, config.m_sStart);
		ctx.NotNull(start, "the conversation has a start");
		LF_DialogueOption crate = FindReply(start, "sell", "QuestTurnedIn", "shipment_count");
		LF_DialogueOption sold = FindReply(start, "sold", "QuestTurnedIn", "shipment_sold");
		LF_DialogueOption reported = FindReply(start, "threat", "QuestTurnedIn", "shipment_report");
		ctx.NotNull(crate, "the greeting offers the sell intro once the crate is opened (shipment_count)");
		ctx.NotNull(sold, "the greeting offers the sold answer once the sample is sold (shipment_sold)");
		ctx.NotNull(reported, "the greeting offers the threat once the sample is reported (shipment_report)");
		ctx.True(LF_DialogueHook.Find("QuestTurnedIn") != null, "the QuestTurnedIn condition has a class");

		LF_DialogueNode sell = LF_DialogueFlow.FindNode(config, "sell");
		ctx.NotNull(sell, "the sell intro is a node");
		if (sell)
		{
			LF_DialogueLine sellLine = LF_DialogueFlow.FindLine(config, sell.m_sLine);
			ctx.True(sellLine && WidgetManager.Translate(sellLine.m_sTextKey).IndexOf("buyer") != -1, "the sell intro speaks of the buyer");
		}

		LF_DialogueNode threat = LF_DialogueFlow.FindNode(config, "threat");
		ctx.NotNull(threat, "the threat is a node");
		if (threat)
		{
			LF_DialogueLine threatLine = LF_DialogueFlow.FindLine(config, threat.m_sLine);
			ctx.True(threatLine && WidgetManager.Translate(threatLine.m_sTextKey).IndexOf("coast road") != -1, "the threat names the coast road gang's road");
		}

		LF_DialogueNode bye = LF_DialogueFlow.FindNode(config, "bye");
		ctx.NotNull(bye, "the conversation has a goodbye");
		if (bye)
			ctx.True(!bye.m_aOptions || bye.m_aOptions.IsEmpty(), "the goodbye ends the talk");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckResident(LF_TestContext ctx)
	{
		LF_ResidentsConfig config = LF_ResidentsConfig.Get();
		ctx.NotNull(config, "the residents config loads under the pack");
		if (!config)
			return;

		LF_ResidentDef hugo;
		foreach (LF_ResidentDef def : config.m_aResidents)
		{
			if (def && def.m_sId == RESIDENT_ID)
				hugo = def;
		}

		ctx.NotNull(hugo, "Hugo Valmy is a resident of the pack (resident id " + RESIDENT_ID + ")");
		if (!hugo)
			return;

		ctx.EqualStr("CafeTable", hugo.m_sWorkKind, "he works the village pub (the café place)");
		ctx.EqualStr(HUGO, hugo.m_sPrefab, "his resident row spawns his own prefab");
		ctx.EqualStr("Hugo Valmy", WidgetManager.Translate("#LF-Resident_shipment_go_between_Name"), "his resident name key resolves");

		// A haunt: at least one timetable line has him at the pub, sitting on its bench.
		bool pub;
		foreach (LF_ResidentEntryDef line : hugo.m_aTimetable)
		{
			if (line.m_sPlace == LF_ResidentTimetable.ROLE_WORK && line.m_sAnchor == "bench" && line.m_sActivity == "sit")
				pub = true;
		}

		ctx.True(pub, "his day has him seated on the pub's bench");
		ctx.True(hugo.m_aTimetable.Count() >= 4, "his day has at least four lines");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");

		// The act: spawn the go-between as the pack ships him. Red proof: no spawn.
		IEntity npc;
		if (!RedProof())
			npc = LF_Utils.SpawnEntityPrefab(HUGO, "0 -50 0");

		ctx.True(npc != null, "Hugo Valmy spawns");
		if (!npc)
			return;

		ctx.NotNull(npc.FindComponent(LF_DialogueComponent), "Hugo Valmy has a dialogue component");
		ctx.True(HasTalkAction(npc), "Hugo Valmy has a talk action");

		// A dressed 1989 man: a jacket, trousers, boots and a cap on him, not the default naked body.
		int worn;
		string wornNames;
		IEntity child = npc.GetChildren();
		while (child)
		{
			if (child.FindComponent(BaseLoadoutClothComponent))
			{
				worn++;
				wornNames += LF_Utils.GetPrefabName(child) + " ";
			}

			child = child.GetSibling();
		}

		ctx.True(worn >= 4, "Hugo Valmy wears a jacket, trousers, boots and a cap (wears: " + wornNames + ")");

		// The id the component names resolves through the pack's index folder overlay.
		string path = LF_DialogueRegistry.Resolve(RESIDENT_ID);
		ctx.True(!path.IsEmpty(), "his dialogue id " + RESIDENT_ID + " resolves through the pack index");

		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(npc);
		ctx.NotNull(config, "Hugo Valmy has a playable dialogue");
		SCR_EntityHelper.DeleteEntityAndChildren(npc);
		if (config)
			CheckDialogue(ctx, config);

		CheckResident(ctx);
	}
}
