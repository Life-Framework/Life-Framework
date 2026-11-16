// red-proof: Observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_LogoutRule`: RED PROOF OK, with the contact stamp skipped the wanted-within-window case fails (`the decision records the police contact`, `it carries the leaving-custody charge (expected 1, got 0)`). Before the contact window existed the test could not compile (`m_iContactAge`, `LF_PoliceContact`, `RejoinFor` undefined). Green after the change.
// tier: LOGIC
//! Combat-logout rule (#1972, #1794): where a logout rejoins. Cuffed goes to jail; wanted within
//! the police contact window goes to jail like a cuffed logout and is a flagged logout; wanted
//! after the window rejoins in place; downed goes to the hospital; anything else is in place.
class LF_Test_LogoutRule : LF_Test
{
	override string GetName()
	{
		return "rejoin/logout-rule";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_DisconnectConfig config = LF_DisconnectConfig.CreateDefault();
		ctx.Equal(120, config.m_iContactWindowSeconds, "the shipped contact window is 2 minutes");

		// Cuffed: jail.
		LF_DisconnectState cuffed = State(true, false, 0, true);
		cuffed.m_iContactAge = -1;
		LF_DisconnectDecision d = LF_DisconnectRules.Decide(cuffed, config);
		ctx.EqualStr(LF_DisconnectRules.REJOIN_JAIL, LF_DisconnectRules.RejoinFor(d), "a cuffed logout rejoins in jail");

		// Wanted, police contact 30 s ago: jail, flagged, a logout charge like a cuffed leaver.
		LF_PoliceContact.Reset();
		int now = 1000000;
		if (!RedProof())
			LF_PoliceContact.StampAt("rule-uid", "chase report", now);
		LF_DisconnectState wanted = State(false, false, 2, true);
		wanted.m_iContactAge = LF_PoliceContact.AgeSeconds("rule-uid", now + 30000);
		if (RedProof())
			ctx.Equal(-1, wanted.m_iContactAge, "red proof: no contact stamped");
		ctx.Equal(30, wanted.m_iContactAge, "the stamp is 30 s old");
		d = LF_DisconnectRules.Decide(wanted, config);
		ctx.EqualStr(LF_DisconnectRules.REJOIN_JAIL, LF_DisconnectRules.RejoinFor(d), "wanted within the contact window rejoins in jail");
		ctx.True(d.m_bContact, "the decision records the police contact");
		ctx.True(d.IsFlagged(), "a contact logout is a flagged logout");
		ctx.Equal(1, d.m_iWantedAdd, "it carries the leaving-custody charge");
		ctx.True(d.Describe().Contains("contact"), "the outcome text names the contact");

		// Wanted, contact 3 minutes ago: the window is over, rejoin in place.
		wanted.m_iContactAge = LF_PoliceContact.AgeSeconds("rule-uid", now + 180000);
		d = LF_DisconnectRules.Decide(wanted, config);
		ctx.EqualStr(LF_DisconnectRules.REJOIN_IN_PLACE, LF_DisconnectRules.RejoinFor(d), "wanted after the window rejoins in place");
		ctx.False(d.m_bContact, "no contact after the window");
		ctx.True(d.m_bKeepWarrant, "the warrant still stays");

		// Wanted with no contact at all: in place.
		wanted.m_iContactAge = LF_PoliceContact.AgeSeconds("nobody", now);
		ctx.Equal(-1, wanted.m_iContactAge, "no stamp is age -1");
		ctx.EqualStr(LF_DisconnectRules.REJOIN_IN_PLACE, LF_DisconnectRules.RejoinFor(LF_DisconnectRules.Decide(wanted, config)), "wanted with no contact rejoins in place");

		// The owner's window: 0 turns the contact rule off, a longer one reaches further.
		wanted.m_iContactAge = 30;
		config.m_iContactWindowSeconds = 0;
		ctx.EqualStr(LF_DisconnectRules.REJOIN_IN_PLACE, LF_DisconnectRules.RejoinFor(LF_DisconnectRules.Decide(wanted, config)), "window 0: contact never jails");
		config.m_iContactWindowSeconds = 600;
		wanted.m_iContactAge = 300;
		ctx.EqualStr(LF_DisconnectRules.REJOIN_JAIL, LF_DisconnectRules.RejoinFor(LF_DisconnectRules.Decide(wanted, config)), "a 10 minute window reaches a 5 minute old contact");
		config.m_iContactWindowSeconds = 120;

		// A contact with no warrant is nothing: only a wanted player is chased.
		LF_DisconnectState clean = State(false, false, 0, true);
		clean.m_iContactAge = 10;
		d = LF_DisconnectRules.Decide(clean, config);
		ctx.False(d.m_bContact, "contact without a warrant is no contact");
		ctx.EqualStr(LF_DisconnectRules.REJOIN_IN_PLACE, LF_DisconnectRules.RejoinFor(d), "a clean player rejoins in place");

		// Downed: hospital; stunned or owner-off: in place.
		LF_DisconnectState downed = State(false, true, 0, true);
		downed.m_iContactAge = -1;
		ctx.EqualStr(LF_DisconnectRules.REJOIN_HOSPITAL, LF_DisconnectRules.RejoinFor(LF_DisconnectRules.Decide(downed, config)), "a downed logout rejoins at the hospital");
		downed.m_bStunned = true;
		ctx.EqualStr(LF_DisconnectRules.REJOIN_IN_PLACE, LF_DisconnectRules.RejoinFor(LF_DisconnectRules.Decide(downed, config)), "a stunned logout rejoins in place");
		downed.m_bStunned = false;
		config.m_bDownedDies = false;
		ctx.EqualStr(LF_DisconnectRules.REJOIN_IN_PLACE, LF_DisconnectRules.RejoinFor(LF_DisconnectRules.Decide(downed, config)), "downedDies off: in place");
		config.m_bDownedDies = true;

		// Cuffed and downed: the sentence wins, the jail put-back runs after the hospital spawn.
		LF_DisconnectState both = State(true, true, 1, true);
		both.m_iContactAge = -1;
		ctx.EqualStr(LF_DisconnectRules.REJOIN_JAIL, LF_DisconnectRules.RejoinFor(LF_DisconnectRules.Decide(both, config)), "cuffed and downed rejoins in jail");

		// Jail with no cell falls back to the fine: nothing to rejoin in.
		wanted.m_iContactAge = 30;
		wanted.m_bCellPlaced = false;
		ctx.EqualStr(LF_DisconnectRules.REJOIN_IN_PLACE, LF_DisconnectRules.RejoinFor(LF_DisconnectRules.Decide(wanted, config)), "no cell: the contact logout is fined, rejoin in place");
		wanted.m_bCellPlaced = true;

		// A serving prisoner stays in jail by the sentence, not by a second rule.
		LF_DisconnectState serving = State(false, false, 1, true);
		serving.m_bServing = true;
		serving.m_iContactAge = 10;
		d = LF_DisconnectRules.Decide(serving, config);
		ctx.False(d.m_bContact, "a prisoner gets no contact rule on top of the sentence");

		// Only an officer's identification is contact.
		ctx.True(LF_PoliceContact.IsContactSource("ID check"), "an ID check is contact");
		ctx.True(LF_PoliceContact.IsContactSource("caught in the act"), "a caught-in-the-act report is contact");
		ctx.False(LF_PoliceContact.IsContactSource("unpaid ticket"), "an unpaid ticket has no officer there");
		ctx.False(LF_PoliceContact.IsContactSource("leaving a store with unpaid goods"), "a till walk-out has no officer there");

		// The owner section reads the window.
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"disconnect\":{\"contactWindowSeconds\":45}}", "test");
		LF_DisconnectConfig.Reload();
		ctx.Equal(45, LF_DisconnectConfig.Get().m_iContactWindowSeconds, "the owner's contact window applies");
		LF_ConfigService.Reset();
		LF_DisconnectConfig.Reload();
		LF_PoliceContact.Reset();
	}

	protected static LF_DisconnectState State(bool cuffed, bool downed, int wanted, bool cell)
	{
		LF_DisconnectState state = new LF_DisconnectState();
		state.m_sPlayerUid = "rule-uid";
		state.m_bCuffed = cuffed;
		state.m_bDowned = downed;
		state.m_iWanted = wanted;
		state.m_bCellPlaced = cell;
		return state;
	}
}
