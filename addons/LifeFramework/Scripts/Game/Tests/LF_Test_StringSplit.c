// red-proof: expected 'Civil Registry, Form R-89' with the core alone; changing the core text of
// LF-Reg_Title back to the Everon form failed strings/split with "the core's own form title names no
// place (got 'Civil Registry of Everon, Form R-89')" (2026-11-14).

// tier: LOGIC
//! The core/pack string split (#325): flavour keys the core still shows keep neutral core text; the
//! pack's table overrides them when it is loaded. Passes on the core alone and with the pack.
class LF_Test_StringSplit : LF_Test
{
	override string GetName()
	{
		return "strings/split";
	}

	override void Run(LF_TestContext ctx)
	{
		// Every one of these is shown by core UI or quests, so none may come back as a raw key.
		array<string> shown = {
			"#LF-Reg_Title", "#LF-ID_CardTitle", "#LF-ATM_Header", "#LF-Org_LedgerTitle", "#LF-PoliceApp_Title",
			"#LF-CharSel_Title", "#EL-Keybinds_Title", "#LF-Sign_Generic_Title",
			"#LF-Quest_ArrivalRegister_Title", "#LF-Quest_ArrivalRegister_Desc", "#LF-Quest_ArrivalRegister_TurnIn",
			"#LF-Quest_ArrivalBoard_Title", "#LF-Quest_ArrivalBoard_Desc", "#LF-Quest_ArrivalWork_Title",
			"#LF-Quest_ArrivalWork_Desc", "#LF-Quest_ArrivalBank_Title", "#LF-Quest_ArrivalBank_Desc",
			"#LF-Quest_ArrivalDone_Title", "#LF-Quest_ArrivalDone_Desc", "#LF-Quest_ArrivalDone_TurnIn",
			"#LF-Dialogue_Clerk_Done", "#LF-Dialogue_Clerk_AboutTown", "#LF-NpcRole_ranger_What",
			"#LF-Dialogue_FuelClerk_AboutWhere"
		};
		foreach (string key : shown)
		{
			string text = WidgetManager.Translate(key);
			ctx.False(text == key || text.IsEmpty(), "the core resolves " + key + " (got '" + text + "')");
		}

		// A per-sign key is the pack's: it resolves only when the pack's table is loaded.
		bool packLoaded = WidgetManager.Translate("#LF-Sign_Mining_Title") != "#LF-Sign_Mining_Title";
		string form = WidgetManager.Translate("#LF-Reg_Title");
		if (packLoaded)
		{
			ctx.EqualStr("Civil Registry of Everon, Form R-89", form, "with the pack loaded the form names Everon");
			ctx.True(WidgetManager.Translate("#LF-Quest_ArrivalDone_TurnIn").Contains("Welcome to Everon"), "with the pack loaded the arrival copy welcomes to Everon");
		}
		else
		{
			ctx.EqualStr("Civil Registry, Form R-89", form, "the core's own form title names no place");
			ctx.False(WidgetManager.Translate("#LF-Quest_ArrivalDone_TurnIn").Contains("Everon"), "the core's arrival copy names no place");
		}
	}
}
