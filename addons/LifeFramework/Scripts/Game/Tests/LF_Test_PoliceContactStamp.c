// red-proof: Observed 2026-11-17 with `tools\cli test --no-build --red LF_Test_PoliceContactStamp`: RED PROOF OK, with the stamps skipped the cuffed and searched suspects have no contact age and rejoin in place (`expected 'jail', got 'in place'`). On main the test could not compile (`LF_Cuffing.StampContact`, `LF_ConfiscateAction.StampContact` undefined). Green after the change.
// tier: LOGIC
//! rejoin/police-contact-stamp (#2102, #1972, #1794): a completed cuffing and a person search stamp police
//! contact on the suspect, so a wanted player who logs out inside the contact window rejoins in jail. A cuffing
//! the restraint refused (RATE_LIMITED, NO_CUFFS, ...) is no contact. Tests use their own uids and never
//! reset the shared statics: they forget only the stamps they made.
class LF_Test_PoliceContactStamp : LF_Test
{
	override string GetName()
	{
		return "rejoin/police-contact-stamp";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_DisconnectConfig config = LF_DisconnectConfig.CreateDefault();
		string cuffedUid = "stamp-cuffed-uid";
		string searchedUid = "stamp-searched-uid";
		string refusedUid = "stamp-refused-uid";
		LF_PoliceContact.Forget(cuffedUid);
		LF_PoliceContact.Forget(searchedUid);
		LF_PoliceContact.Forget(refusedUid);

		ctx.Equal(-1, LF_PoliceContact.AgeSecondsNow(cuffedUid), "no contact before the cuffing");
		ctx.Equal(-1, LF_PoliceContact.AgeSecondsNow(searchedUid), "no contact before the search");

		// A completed cuffing is contact.
		if (!RedProof())
			ctx.True(LF_Cuffing.StampContact(cuffedUid, LF_ERestrain.OK), "a completed cuffing stamps contact");
		int cuffedAge = LF_PoliceContact.AgeSecondsNow(cuffedUid);
		ctx.True(LF_PoliceContact.InWindow(cuffedAge, config.m_iContactWindowSeconds), "the cuffed suspect is inside the contact window");
		ctx.EqualStr(LF_DisconnectRules.REJOIN_JAIL, LogoutRejoin(cuffedUid, config), "a wanted suspect who logs out after a cuffing rejoins in jail");

		// A person search is contact.
		if (!RedProof())
			LF_ConfiscateAction.StampContact(searchedUid);
		int searchedAge = LF_PoliceContact.AgeSecondsNow(searchedUid);
		ctx.True(LF_PoliceContact.InWindow(searchedAge, config.m_iContactWindowSeconds), "the searched suspect is inside the contact window");
		ctx.EqualStr(LF_DisconnectRules.REJOIN_JAIL, LogoutRejoin(searchedUid, config), "a wanted suspect who logs out after a search rejoins in jail");

		// A cuffing the restraint refused is no contact.
		ctx.False(LF_Cuffing.StampContact(refusedUid, LF_ERestrain.NO_CUFFS), "a refused cuffing stamps nothing");
		ctx.Equal(-1, LF_PoliceContact.AgeSecondsNow(refusedUid), "a refused cuffing leaves no stamp");

		LF_PoliceContact.Forget(cuffedUid);
		LF_PoliceContact.Forget(searchedUid);
		LF_PoliceContact.Forget(refusedUid);
	}

	//! What a wanted, uncuffed player who leaves now rejoins as.
	protected static string LogoutRejoin(string uid, LF_DisconnectConfig config)
	{
		LF_DisconnectState state = new LF_DisconnectState();
		state.m_sPlayerUid = uid;
		state.m_iWanted = 2;
		state.m_bCellPlaced = true;
		state.m_iContactAge = LF_PoliceContact.AgeSecondsNow(uid);
		return LF_DisconnectRules.RejoinFor(LF_DisconnectRules.Decide(state, config));
	}
}
