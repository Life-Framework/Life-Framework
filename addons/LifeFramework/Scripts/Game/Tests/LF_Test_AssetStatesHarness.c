// red-proof: Parse without the itemgive branch fails "itemgive with a prefab parses"; Run with the
// usage check removed fails "a bare verb is refused for its usage".
// tier: LOGIC
class LF_Test_AssetStatesHarness : LF_Test
{
	override string GetName()
	{
		return "agent-harness/asset-states-verbs";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_AgentCommand cmd;
		ctx.True(LF_AgentCommand.Parse("31 itemgive {DE0B6FEBDCECC6FF}Prefabs/Items/Roleplay/Handcuffs.et", cmd), "itemgive with a prefab parses");
		ctx.EqualStr("itemgive", cmd.Verb, "itemgive verb");
		ctx.EqualStr("{DE0B6FEBDCECC6FF}Prefabs/Items/Roleplay/Handcuffs.et", cmd.Arg1, "itemgive prefab");
		ctx.False(LF_AgentCommand.Parse("32 itemdrop", cmd), "itemdrop without a prefab is refused");
		ctx.EqualStr("need_prefab", cmd.Error, "itemdrop refusal reason");
		ctx.True(LF_AgentCommand.Parse("33 heldxf", cmd), "heldxf parses");

		ctx.True(LF_AssetStatesHarness.IsItemVerb("itemgive {DE0B6FEBDCECC6FF}Prefabs/Items/Roleplay/Handcuffs.et"), "itemgive goes to the states harness");
		ctx.True(LF_AssetStatesHarness.IsItemVerb("itemdrop {DE0B6FEBDCECC6FF}Prefabs/Items/Roleplay/Handcuffs.et"), "itemdrop goes to the states harness");
		ctx.True(LF_AssetStatesHarness.IsItemVerb("itemwear {BE19A15F1073E7BD}Prefabs/Characters/HeadGear/Hat_Knitted_01/Hat_Knitted_01_black.et"), "itemwear goes to the states harness");
		ctx.True(LF_AssetStatesHarness.IsItemVerb("itemstrip {DE0B6FEBDCECC6FF}Prefabs/Items/Roleplay/Handcuffs.et"), "itemstrip goes to the states harness");
		ctx.True(LF_AgentCommand.Parse("34 itemwear list", cmd) && cmd.Arg1 == "list", "itemwear list parses");
		ctx.EqualStr("error no_character", LF_AssetStatesHarness.Run(null, "itemwear list"), "no character, nothing worn");
		ctx.False(LF_AssetStatesHarness.IsItemVerb("scanner"), "other stand-ins stay where they are");
		ctx.EqualStr("error usage_item_verb_prefab", LF_AssetStatesHarness.Run(null, "itemgive"), "a bare verb is refused for its usage");
		ctx.EqualStr("error no_character", LF_AssetStatesHarness.Run(null, "itemgive {DE0B6FEBDCECC6FF}Prefabs/Items/Roleplay/Handcuffs.et"), "no character, no item");
	}
}
