// red-proof: Observed 2026-11-09 with `toolsi test --no-build --only LF_Test_ServiceCalls`: LF_ServiceCalls.Receives matching the job with `!=` failed comms/routing with `- the receivers are the medics` and `- nobody on duty gives no answer`. Restored; green.
//------------------------------------------------------------------------------------------------
//! comms/routing (#248): a medic call reaches only on-duty medics, an unknown service is rejected,
//! and nobody on duty gives "no answer".
// tier: LOGIC
class LF_Test_ServiceCalls : LF_Test
{
	override string GetName()
	{
		return "comms/routing";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_ServiceDef> services = {};
		services.Insert(LF_ServiceDef.Make("police", "#p", "duty:police", "radio,board"));
		services.Insert(LF_ServiceDef.Make("medic", "#m", "job:4", "pager"));

		array<ref LF_ServiceCandidate> players = {};
		players.Insert(LF_ServiceCandidate.Make(1, true, LF_EJobType.UNEMPLOYED));
		players.Insert(LF_ServiceCandidate.Make(2, false, LF_EJobType.MEDIC));
		players.Insert(LF_ServiceCandidate.Make(3, false, LF_EJobType.TAXI));
		players.Insert(LF_ServiceCandidate.Make(4, false, LF_EJobType.MEDIC));

		LF_ServiceRoute medic = LF_ServiceCalls.Route(services, "medic", players);
		ctx.True(medic.IsAnswered(), "a medic call with medics on duty is answered");
		ctx.Equal(2, medic.m_aReceivers.Count(), "a medic call reaches both medics");
		ctx.True(medic.m_aReceivers.Find(2) != -1 && medic.m_aReceivers.Find(4) != -1, "the receivers are the medics");
		ctx.EqualStr("pager", medic.m_sChannels, "the medic call uses the pager channel");

		LF_ServiceRoute police = LF_ServiceCalls.Route(services, "police", players);
		ctx.Equal(1, police.m_aReceivers.Count(), "a police call reaches only the on-duty officer");
		ctx.EqualStr("radio,board", police.m_sChannels, "the police call lists its channels");

		LF_ServiceRoute unknown = LF_ServiceCalls.Route(services, "plumber", players);
		ctx.False(unknown.m_bKnown, "an unknown service is rejected");
		ctx.False(unknown.IsAnswered(), "an unknown service is never answered");

		array<ref LF_ServiceCandidate> nobody = {};
		nobody.Insert(LF_ServiceCandidate.Make(3, false, LF_EJobType.TAXI));
		LF_ServiceRoute none = LF_ServiceCalls.Route(services, "medic", nobody);
		ctx.True(none.m_bKnown, "a known service with nobody on duty is still known");
		ctx.False(none.IsAnswered(), "nobody on duty gives no answer");
	}
}
