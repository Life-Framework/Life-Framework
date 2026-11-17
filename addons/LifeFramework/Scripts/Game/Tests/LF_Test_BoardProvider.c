// red-proof: 2026-11-16 `toolsi test --no-build --red LF_Test_BoardProvider` skipped registering the fake: RED-OK boards/provider (fails with the hook on, 4 assertions: no provider, no answer). Green with it (--only, with LF_Test_PoliceStation).
// tier: LOGIC
//! One board component (#1945): a fake provider's server answer reaches the board text the client
//! paints; an unknown kind answers nothing; a second read builds a new answer from the server's
//! state now (nothing cached past a read); the wanted board is registered as a kind and its answer
//! survives the flat RPC shape (Pack, Unpack) into the same poster lines.
class LF_BoardFakeProvider : LF_BoardProvider
{
	string m_sMessage;

	override string GetKind()
	{
		return "test-fake";
	}

	override LF_BoardAnswer Answer(int playerId)
	{
		LF_BoardAnswer answer = new LF_BoardAnswer();
		answer.m_bPrivileged = playerId == 7;
		answer.m_aTexts.Insert(m_sMessage);
		answer.m_aNumbers.Insert(playerId);
		return answer;
	}

	override array<ref LF_UIPart> Paint(notnull LF_BoardAnswer answer)
	{
		array<ref LF_UIPart> parts = {};
		string who = "public";
		if (answer.m_bPrivileged)
			who = "staff";

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Fake", who + ": " + answer.m_aTexts[0] + " #" + answer.m_aNumbers[0]));
		return parts;
	}
}

//------------------------------------------------------------------------------------------------
class LF_Test_BoardProvider : LF_Test
{
	override string GetName()
	{
		return "boards/provider";
	}

	//------------------------------------------------------------------------------------------------
	protected static string LineText(array<ref LF_UIPart> parts, string name)
	{
		foreach (LF_UIPart part : parts)
		{
			if (part.GetName() == name)
				return part.GetText();
		}

		return "<no part " + name + ">";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_BoardProviders.ResetStatics();
		LF_BoardFakeProvider fake = new LF_BoardFakeProvider();
		fake.m_sMessage = "two guards on the dock";
		if (!RedProof())
			LF_BoardProviders.Register(fake);

		// The server answers, the client paints: the fake's answer is the board text.
		ctx.True(LF_BoardProviders.Find("test-fake") == fake, "the fake provider is registered under its kind");
		LF_BoardAnswer first = LF_BoardProviders.Answer("test-fake", 7);
		ctx.True(first != null, "the server answers a read of a registered kind");
		if (!first)
			return;

		ctx.True(first.m_bPrivileged, "the answer carries the reader's standing");
		ctx.EqualStr("staff: two guards on the dock #7", LineText(LF_BoardProviders.Paint("test-fake", first), "Fake"), "the answer reaches the board text");

		LF_BoardAnswer publicRead = LF_BoardProviders.Answer("test-fake", 3);
		ctx.EqualStr("public: two guards on the dock #3", LineText(LF_BoardProviders.Paint("test-fake", publicRead), "Fake"), "another reader gets their own answer");

		// A kind nobody registered answers nothing and paints nothing.
		ctx.True(LF_BoardProviders.Answer("test-nobody", 7) == null, "an unknown kind gets no answer");
		ctx.True(LF_BoardProviders.Find("test-nobody") == null, "an unknown kind has no provider");
		ctx.Equal(0, LF_BoardProviders.Paint("test-nobody", first).Count(), "an unknown kind paints no lines");

		// Nothing is cached past a read: the server's state changed, so the next read says so, and the
		// answer already handed out is a snapshot that did not move.
		fake.m_sMessage = "the dock is clear";
		LF_BoardAnswer second = LF_BoardProviders.Answer("test-fake", 7);
		ctx.True(second != first, "a second read builds a new answer");
		ctx.EqualStr("staff: the dock is clear #7", LineText(LF_BoardProviders.Paint("test-fake", second), "Fake"), "the second read shows the state now");
		ctx.EqualStr("staff: two guards on the dock #7", LineText(LF_BoardProviders.Paint("test-fake", first), "Fake"), "an answer in hand is not rewritten by a later read");

		// The wanted board is a registered kind; its answer rides the flat arrays and paints the same posters.
		LF_BoardProvider wanted = LF_BoardProviders.Find(LF_WantedBoard.KIND);
		ctx.True(wanted != null, "the wanted board registers as a kind");
		ctx.True(LF_BoardProviders.Find("test-fake") == fake, "the fake stays registered beside the wanted kind");

		array<string> names = {"", "Ada Wren"};
		array<string> offences = {"Burglary", "Arson"};
		array<string> clothing = {"a|b", ""};
		array<string> vehicles = {"", "a red van"};
		array<string> places = {"Harbour", ""};
		array<int> levels = {2, 4};
		array<string> dispatches = {"entry-one", "entry-two"};
		LF_BoardAnswer packed = LF_WantedBoard.Pack(true, names, offences, clothing, vehicles, places, levels, dispatches);
		ctx.True(packed.m_bPrivileged, "the wanted answer carries the officer flag");
		ctx.Equal(2, packed.m_aNumbers.Count(), "one level per poster");
		ctx.Equal(2 * LF_WantedBoard.POSTER_FIELDS + 2, packed.m_aTexts.Count(), "the posters' texts, then the dispatch entries");

		array<string> outNames = {};
		array<string> outOffences = {};
		array<string> outClothing = {};
		array<string> outVehicles = {};
		array<string> outPlaces = {};
		array<int> outLevels = {};
		array<string> outDispatches = {};
		LF_WantedBoard.Unpack(packed, outNames, outOffences, outClothing, outVehicles, outPlaces, outLevels, outDispatches);
		ctx.Equal(2, outOffences.Count(), "two posters come back out of the answer");
		ctx.EqualStr("Ada Wren", outNames[1], "the name of the second poster");
		ctx.EqualStr("Arson", outOffences[1], "the offence of the second poster");
		ctx.EqualStr("a red van", outVehicles[1], "the vehicle of the second poster");
		ctx.EqualStr("Harbour", outPlaces[0], "the place of the first poster");
		ctx.Equal(4, outLevels[1], "the level of the second poster");
		ctx.Equal(2, outDispatches.Count(), "the dispatch entries follow the posters");
		ctx.EqualStr("entry-two", outDispatches[1], "the last dispatch entry");

		array<ref LF_UIPart> parts = LF_BoardProviders.Paint(LF_WantedBoard.KIND, packed);
		ctx.True(LineText(parts, "Poster0").Contains("Burglary"), "the first poster headline names the offence: " + LineText(parts, "Poster0"));
		ctx.True(LineText(parts, "Poster1").Contains("Ada Wren"), "the second poster headline names the suspect: " + LineText(parts, "Poster1"));
		ctx.True(LineText(parts, "Seen0").Contains("Harbour"), "an officer's poster carries the place");

		LF_BoardProviders.ResetStatics();
		ctx.True(LF_BoardProviders.Find("test-fake") == null, "a world-end reset forgets the fake");
		ctx.True(LF_BoardProviders.Find(LF_WantedBoard.KIND) != null, "and the wanted kind registers again on the next read");
		LF_BoardProviders.ResetStatics();
	}
}
