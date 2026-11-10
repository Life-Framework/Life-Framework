// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_ReviveCprGraph`: a wrong GUID and path for LF_ReviveAnimPlayer.GRAPH failed it with `- the CPR graph loads`; restored; green.

//------------------------------------------------------------------------------------------------
//! medic/revive-cpr-graph (#512): the medic's CPR runs as a custom loiter on our attachment graph.
//! The graph, its instance and the three base-game CPR clips it names must load, and the loiter
//! data must drive the graph the way the player graph's NPC attachment expects (binding NPC, the
//! Gestures variable, no command), or the medic stands still while the server says "started".
// tier: WORLD
class LF_Test_ReviveCprGraph : LF_Test
{
	override string GetName()
	{
		return "medic/revive-cpr-graph";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(Resource.Load(LF_ReviveAnimPlayer.GRAPH).IsValid(), "the CPR graph loads");
		ctx.True(Resource.Load(LF_ReviveAnimPlayer.GRAPH_INSTANCE).IsValid(), "the CPR graph instance loads");
		ctx.True(Resource.Load("{53DD1932ED812D70}anims/anm/player/injury/p_cro_revive_CPR_in.anm").IsValid(), "the CPR in clip loads");
		ctx.True(Resource.Load("{0CCED80870DD8150}anims/anm/player/injury/p_cro_revive_CPR_loop.anm").IsValid(), "the CPR loop clip loads");
		ctx.True(Resource.Load("{226002617FC7B0AB}anims/anm/player/injury/p_cro_revive_CPR_out.anm").IsValid(), "the CPR out clip loads");

		SCR_LoiterCustomAnimData data = LF_ReviveAnimPlayer.CreateAnimData();
		ctx.EqualStr(SCR_CustomAnimData_Properties.BINDING_NAME_NPC, data.m_sGraphBindingName, "the CPR graph binds to the NPC attachment");
		ctx.EqualStr(LF_ReviveAnimPlayer.GRAPH, data.m_sGraphName, "the loiter names the CPR graph");
		ctx.EqualStr(LF_ReviveAnimPlayer.GRAPH_INSTANCE, data.m_sGraphInstanceName, "the loiter names the CPR instance");
		ctx.EqualStr("Gestures", data.m_sControlVariableName, "the Gestures variable drives the graph");
		ctx.Equal(560, data.m_iControlVariableValue, "Gestures 560 enters the CPR state");
		ctx.True(data.m_sCommandName.IsEmpty(), "no command: a command would bypass the control variable");
	}
}
