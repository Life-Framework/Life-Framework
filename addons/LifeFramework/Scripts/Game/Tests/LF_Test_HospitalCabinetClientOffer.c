// red-proof: RED-OK crime/hospital-cabinet-client-offer (tools/cli test --no-build --red LF_Test_HospitalCabinetClientOffer, 2026-11-18: "a closed, unarmed cabinet offers the breach to a client"). On main the client path read the server-only cabinet table and the helper did not exist.
//! The clinic cabinet offer a dedicated client draws (#3364). LF_Hospital.CanBreach reads the server-only
//! cabinet table, so every client saw "Breach cabinet" greyed as "already open". The client now asks the
//! replicated breach mirror, as LF_BreachArmAction does. The test runs on the host, so it asks the client
//! branch directly: the cabinet table is never read there.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HospitalCabinetClientOffer : LF_Test
{
	override string GetName()
	{
		return "crime/hospital-cabinet-client-offer";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity cabinetEntity = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HOSPITAL_CABINET);
		ctx.True(cabinetEntity != null, "Annex_Hospital_Cabinet is placed");
		if (!cabinetEntity)
			return;

		LF_HospitalCabinetComponent cabinet = LF_Component<LF_HospitalCabinetComponent>.Find(cabinetEntity);
		LF_BreachTargetComponent target = LF_Component<LF_BreachTargetComponent>.Find(cabinetEntity);
		ctx.NotNull(cabinet, "cabinet carries LF_HospitalCabinetComponent");
		ctx.NotNull(target, "cabinet is a breach target");
		if (!cabinet || !target)
			return;

		string id = cabinet.GetTargetId();
		string uid = "hospital-cabinet-client-offer";
		string policeUid = "hospital-cabinet-client-offer-police";
		LF_PlayerAccountManager.GetOrCreate(uid);
		LF_PlayerAccountManager.GetOrCreate(policeUid).SetFaction(LF_Faction.POLICE);
		LF_PlayerAccountManager.GetOrCreate(policeUid).SetOnDuty(true);

		// Start closed and unarmed whatever ran before.
		LF_Breach.Repair(id, policeUid);
		ctx.False(target.IsReplicatedOpen(), "the cabinet starts closed");

		bool closed = false;
		if (!RedProof())
			closed = LF_HospitalCabinetAction.OfferOpen(cabinet, true);
		ctx.True(closed, "a closed, unarmed cabinet offers the breach to a client");

		ctx.True(LF_Breach.Arm(id, uid), "zero-fuse arm opens the cabinet");
		ctx.True(target.IsReplicatedOpen(), "the open flag replicates");
		ctx.False(LF_HospitalCabinetAction.OfferOpen(cabinet, true), "an open cabinet is refused to a client");
		ctx.False(LF_HospitalCabinetAction.OfferOpen(cabinet, false), "an open cabinet is refused on the server");

		ctx.True(LF_Breach.Repair(id, policeUid), "police repair runs");
		ctx.True(LF_HospitalCabinetAction.OfferOpen(cabinet, false), "a repaired cabinet offers again on the server");
		ctx.False(LF_HospitalCabinetAction.OfferOpen(null, true), "no cabinet component offers nothing");
	}
}
