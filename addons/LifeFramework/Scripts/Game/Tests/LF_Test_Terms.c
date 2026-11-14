// red-proof (caps, #1136): observed 2026-11-14 (test --only LF_Test_Terms): replaced `words.ToUpper();` in LF_TermBook.Placeholder with `words = words;`; got [LFTEST] FAIL strings/terms "an all-capitals name prints the words in capitals (expected 'POLICE first.', got 'police first.')". Restored; green.
// red-proof: Observed 2026-11-14 with the terms test run alone (no build, only LF_Test_Terms): replaced `words = Capitalise(words);` in LF_TermBook.Placeholder with `words = words;`; got [LFTEST] FAIL strings/terms with "a capital first letter capitalises the words (expected 'Police first.', got 'police first.')". Restored; green.

//------------------------------------------------------------------------------------------------
//! The terms layer (#958), with no world: placeholder resolution with each form, a pack overlay over
//! the core file (the real shipped overlay path), the owner's text over the pack, a per-language
//! owner value, a missing form that falls back and logs once, the stream an owner's text rides to
//! clients, and the one text path a toast, caption or board calls.
// tier: LOGIC
class LF_Test_Terms : LF_Test
{
	static const ResourceName OVERLAY = "{130366DC570DF561}Configs/Tests/LF_Test_TermsOverlay.conf";
	static const string EN = "en_us";
	static const string DE = "de_de";

	override string GetName()
	{
		return "strings/terms";
	}

	override void Run(LF_TestContext ctx)
	{
		CheckPlaceholders(ctx);
		CheckForms(ctx);
		CheckLayers(ctx);
		CheckOwnerShapes(ctx);
		CheckFallback(ctx);
		CheckStream(ctx);
		CheckTextPath(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_TermBook CoreBook()
	{
		LF_TermBook book = new LF_TermBook();
		book.Add("police", "police");
		book.Add("org", "organisation", "organisations");
		book.SetForm("currency", "singular", "dollar");
		book.SetForm("currency", "sym", "$");
		return book;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPlaceholders(LF_TestContext ctx)
	{
		LF_TermBook book = CoreBook();
		ctx.EqualStr("Call the police.", book.Expand("Call the {police}.", EN), "a placeholder becomes the term's words");
		ctx.EqualStr("Police first.", book.Expand("{Police} first.", EN), "a capital first letter capitalises the words");
		ctx.EqualStr("POLICE first.", book.Expand("{POLICE} first.", EN), "an all-capitals name prints the words in capitals");
		ctx.EqualStr("organisations", book.Expand("{org:pl}", EN), "a form after a colon picks that form");
		ctx.EqualStr("Organisations", book.Expand("{Org:pl}", EN), "a capital and a form combine");
		ctx.EqualStr("$40", book.Expand("{currency:sym}40", EN), "an extra form such as the symbol resolves");
		ctx.EqualStr("A {nobody} here", book.Expand("A {nobody} here", EN), "an unknown term stays as written");
		ctx.EqualStr("{ 12 } and {GUID-1}", book.Expand("{ 12 } and {GUID-1}", EN), "braces that are not a placeholder stay as written");
		ctx.EqualStr("open {police", book.Expand("open {police", EN), "a brace that never closes stays as written");
		ctx.EqualStr("no braces", book.Expand("no braces", EN), "text without braces is untouched");
		ctx.EqualStr("police police", book.Expand("{police} {police}", EN), "the same term twice resolves twice");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckForms(LF_TestContext ctx)
	{
		ctx.True(LF_TermBook.IsLanguageCode("de_de"), "de_de is a language code");
		ctx.True(LF_TermBook.IsLanguageCode("pt_br"), "pt_br is a language code");
		ctx.False(LF_TermBook.IsLanguageCode("plural"), "a form name is not a language code");
		ctx.False(LF_TermBook.IsLanguageCode("de-de"), "a dash is not the owner file's spelling");
		ctx.EqualStr("de_de", LF_TermBook.NormalizeLanguage("de-DE"), "a language code is lower case with an underscore");
		ctx.EqualStr(LF_TermBook.FORM_PLURAL, LF_TermBook.CanonicalForm("pl"), "pl is shorthand for plural");
		ctx.EqualStr("Ärger", LF_TermBook.Capitalise("Ärger"), "a first letter that is not a-z is left to the translator");
	}

	//------------------------------------------------------------------------------------------------
	//! Core, then the pack overlay (the real LF_ConfigOverlay path over a .conf), then the owner.
	protected void CheckLayers(LF_TestContext ctx)
	{
		LF_TermBook book = CoreBook();
		Resource resource = BaseContainerTools.LoadContainer(OVERLAY);
		ctx.True(resource && resource.IsValid() && resource.GetResource(), "the test pack overlay loads");
		if (!resource || !resource.GetResource())
			return;

		LF_ConfigClaims claims = new LF_ConfigClaims();
		int changed = LF_ConfigOverlay.ApplyContainerList(new LF_TermList(book.GetRecords()), resource.GetResource().ToBaseContainer(), "m_aTerms", OVERLAY, claims);
		book.ClearCache();
		ctx.Equal(3, changed, "the overlay patches two terms and adds one");
		ctx.EqualStr("Call the Sheriff.", book.Expand("Call the {police}.", EN), "the pack's word replaces the core's");
		ctx.EqualStr("Gangs", book.Expand("{Org:pl}", EN), "a pack form replaces the core's form");
		ctx.EqualStr("Gang", book.Expand("{org}", EN), "a pack form replaces the core's singular");
		ctx.EqualStr("$", book.Expand("{currency:sym}", EN), "a term the pack does not touch keeps the core's words");

		LF_ConfigService.Reset();
		ctx.True(LF_ConfigService.LoadOwnerString("{\"terms\":{\"police\":\"Marshal\"}}", "test"), "the owner document loads");
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection("terms");
		ctx.True(reader != null, "the terms section opens");
		if (reader)
			ctx.Equal(2, book.ApplyOwner(reader, "test"), "plain owner text sets the singular and the plural");
		LF_ConfigService.Reset();
		ctx.EqualStr("Call the Marshal.", book.Expand("Call the {police}.", EN), "the owner's text beats the pack's");
		ctx.EqualStr("Marshal", book.Expand("{police:pl}", EN), "plain owner text is the plural too");
		ctx.EqualStr("Gang", book.Expand("{org}", EN), "a term the owner leaves alone keeps the pack's words");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckOwnerShapes(LF_TestContext ctx)
	{
		LF_TermBook book = CoreBook();
		string json = "{\"terms\":{\"police\":{\"en_us\":\"Sheriff\",\"de_de\":{\"singular\":\"Sheriff\",\"plural\":\"Sheriffs\"}},\"org\":{\"singular\":\"Gang\",\"plural\":\"Gangs\"},\"nobody\":\"x\",\"currency\":5}}";
		LF_ConfigService.Reset();
		ctx.True(LF_ConfigService.LoadOwnerString(json, "test"), "the owner document with per-language text loads");
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection("terms");
		if (reader)
			book.ApplyOwner(reader, "test");
		LF_ConfigService.Reset();

		ctx.EqualStr("Sheriff", book.Expand("{police}", EN), "a per-language owner value applies to that language");
		ctx.EqualStr("Sheriff", book.Expand("{police}", DE), "the German value applies to German");
		ctx.EqualStr("Sheriffs", book.Expand("{police:pl}", DE), "a per-language form object sets its forms");
		ctx.EqualStr("police", book.Expand("{police}", "fr_fr"), "a language the owner left out keeps the shipped words");
		ctx.EqualStr("Gangs", book.Expand("{org:pl}", "fr_fr"), "owner forms with no language apply to every language");
		ctx.EqualStr("dollar", book.Expand("{currency}", EN), "a number where text belongs is refused and the shipped words stay");
		ctx.EqualStr("{nobody}", book.Expand("{nobody}", EN), "an owner key that names no term adds nothing");
		ctx.True(LF_TermBook.LanguageMatches("de_de", "de"), "a bare language code covers its regions");
		ctx.False(LF_TermBook.LanguageMatches("de_de", "fr"), "another language is not covered");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckFallback(LF_TestContext ctx)
	{
		LF_TermBook book = CoreBook();
		ctx.EqualStr("police", book.Expand("{police:acc}", EN), "a form the term lacks falls back to its singular");
		ctx.Equal(1, book.GetWarnCount(), "the missing form is logged once");
		book.Expand("{police:acc} and {police:acc}", EN);
		ctx.Equal(1, book.GetWarnCount(), "the same missing form is not logged again");
		book.Expand("{ghost}", EN);
		book.Expand("{ghost}", EN);
		ctx.Equal(2, book.GetWarnCount(), "an unknown term is logged once");
	}

	//------------------------------------------------------------------------------------------------
	//! The server's owner text reaches a client's book through the same reader.
	protected void CheckStream(LF_TestContext ctx)
	{
		LF_TermBook server = CoreBook();
		server.SetOwner("police", "singular", "", "Sheriff");
		server.SetOwner("org", "plural", DE, "Banden");
		string payload = LF_ConfigStream.BuildPayload(null, true, null, null, null, null, null, false, server);
		ctx.True(payload.Contains("\"terms\""), "an owner's terms ride in the stream");
		ctx.False(LF_ConfigStream.BuildPayload(null, false, null, null, null, null, null, false, CoreBook()).Contains("\"terms\""), "a server with no owner text sends no terms section");

		SCR_JsonLoadContext ctxJson = new SCR_JsonLoadContext(false);
		ctx.True(ctxJson.ImportFromString(payload), "the payload parses");
		LF_TermBook client = CoreBook();
		client.ApplyOwner(new LF_ConfigReader(ctxJson, "terms"), "stream");
		ctx.EqualStr("Sheriff", client.Expand("{police}", EN), "the client reads the server's every-language text");
		ctx.EqualStr("Banden", client.Expand("{org:pl}", DE), "the client reads the server's per-language text");
		ctx.EqualStr("organisations", client.Expand("{org:pl}", EN), "the client keeps the shipped words elsewhere");
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped core file, the string table and the one text path a toast, caption or board calls.
	protected void CheckTextPath(LF_TestContext ctx)
	{
		LF_Terms.Reload();
		LF_TermBook core = LF_Terms.Get();
		ctx.True(core.Count() >= 4, "the core terms file loads its terms");
		array<LF_TermRecord> terms = {};
		core.GetTerms(terms);
		foreach (LF_TermRecord term : terms)
			ctx.True(!core.Resolve(term.m_sId, LF_TermBook.FORM_SINGULAR, EN).IsEmpty(), "core term " + term.m_sId + " has English words");

		string shipped = LF_Terms.Text("#LF-Ticket_NowWanted");
		ctx.True(shipped.Contains("police"), "the shipped text reads the core word: " + shipped);
		ctx.False(shipped.Contains("{"), "no placeholder is left in shown text: " + shipped);
		ctx.EqualStr("Earned $50 for work!", LF_Terms.Text("#EL-Job_Earned", "50"), "a parameter fills %1 after the terms");
		ctx.True(LF_Terms.Text("#LF-Bill_Received", "{police}").Contains("{police}"), "a parameter that looks like a placeholder is not expanded");
		ctx.EqualStr("$1,500", LF_MoneyFormat.FormatMoney(1500), "money reads the currency symbol term");

		ctx.EqualStr("POLICE TICKET", LF_Terms.Text("#LF-Ticket_CardTitle"), "a migrated all-capitals key reads as before");
		ctx.EqualStr("Miner", LF_Terms.Expand("{job:miner}"), "the job term names a job by its id");
		ctx.EqualStr("Fines", LF_Terms.Expand("{Fine:pl}"), "the fine term has a plural");
		ctx.EqualStr("Organization", LF_Terms.Expand("{Org}"), "the org term ships");
		ctx.EqualStr("Only on-duty police can confiscate", LF_Terms.Shown("#EL-Confiscate_OnDutyOnly"), "a bare key shown by the UI kit is translated and expanded");

		LF_TermBook renamed = LF_Terms.Get();
		renamed.SetOwner("police", "singular", "", "Sheriff");
		renamed.SetOwner("org", "singular", "", "gang");
		renamed.SetOwner("currency", "sym", "", "L");
		ctx.True(LF_Terms.Text("#LF-Ticket_NowWanted").Contains("Sheriff"), "text that names a term follows the owner's word");
		ctx.EqualStr("SHERIFF TICKET", LF_Terms.Text("#LF-Ticket_CardTitle"), "an all-capitals key follows the owner's word");
		ctx.EqualStr("Only on-duty Sheriff can confiscate", LF_Terms.Shown("#EL-Confiscate_OnDutyOnly"), "an action reason follows the owner's word");
		ctx.True(LF_Terms.Text("#LF-Org_Left").Contains("gang"), "a migrated organisation key follows the org term");
		ctx.EqualStr("L1,500", LF_MoneyFormat.FormatMoney(1500), "the money format follows the owner's symbol");
		ctx.EqualStr("-L20", LF_MoneyFormat.FormatMoney(-20), "a negative amount keeps the minus before the symbol");
		LF_Terms.Reload();
	}
}
