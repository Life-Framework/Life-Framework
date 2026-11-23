// red-proof: batched run `tools\cli test --no-build --red LF_Test_EveronRumors` (RED-OK line in the PR): RedProof() skips the load of the pack's rumor rows, so no row is found and every row, place, clip and crime check fails.
//! #4476 (W5.4 of the world chapter, #4170): under the Everon 1989 header (the pack leg of `tools\cli test`) the pack's
//! rumor rows are real. About thirty rows of `Configs/Residents/LF_Rumors_Everon1989.conf`: each row has a unique id and
//! points at a place id the island registers through `LF_Places` (the table below mirrors the `LF_PlaceComponent`
//! kind and id of `Worlds/MainWorld/MainWorld_Layers/Place_*.layer`; the test world holds no island markers, so the
//! ids are checked against that list), each line is spoken (its caption, sound event, length and clip on disk, through
//! the speaker's own recorded conversation), and every caption is US English with no French word. A crime row (N32)
//! carries the crime flag, is told only by bar staff or the informant, prints only what a passer-by could see (no
//! open case, no record), and the pool offers it to a friendly player and never to a neutral one or a gossip.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronRumors : LF_Test
{
	static const ResourceName ROWS = "{4A002D70729E375D}Configs/Residents/LF_Rumors_Everon1989.conf";
	static const int MIN_ROWS = 30;
	static const string CHARACTER = "ev-rumor-char";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/rumors";
	}

	//------------------------------------------------------------------------------------------------
	//! The island's places a rumor may name: kind and id, as the island layers register them.
	protected static void Places(array<string> kinds, array<string> ids)
	{
		array<string> table = {
			"Processor", "coalmans-brow.mine", "Workplace", "everon-sawmill.yard", "OrchardTrees", "saint-philippe.orchard",
			"Smokehouse", "saint-philippe-dock.smokehouse", "MarketTrader", "saint-philippe.market", "HarbourMaster", "saint-philippe.harbour",
			"ParcelDepot", "saint-philippe.parcel", "FishingSpot", "saint-philippe.pond", "Church", "saint-philippe.church",
			"FishingSpot", "fishermansbay.cove", "FishingSpot", "blacklake.shore", "WholesaleCounter", "saint-philippe-dock.wholesale",
			"Workplace", "saint-philippe.woodlot", "StoreCounter", "saint-philippe.seed-feed", "RangeLane", "saint-philippe.range",
			"MedicDesk", "saint-philippe.clinic", "PubBar", "chotain.pub", "ChessTable", "saint-philippe.chess",
			"VehicleLot", "saint-philippe.carlot", "RoadFuel", "roadfuel.fuel", "StoreCounter", "saint-philippe.hardware",
			"TaxiRank", "saint-philippe.taxi", "StillBarn", "stillbarn.1", "ChopYard", "chopyard.yard",
			"FenceYard", "saint-philippe.fence", "VehicleLot", "ne-compound.truckyard", "FarmCompound", "farmcompound.compound",
			"HilltopMast", "hilltopmast.a", "ScrapRecycler", "scrapyard.recycler", "Warehouse", "saint-philippe-dock.warehouse"
		};

		for (int i = 0; i + 1 < table.Count(); i += 2)
		{
			kinds.Insert(table[i]);
			ids.Insert(table[i + 1]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The places crime rows are about: the hidden ones the public map never shows.
	protected static void CrimePlaces(array<string> ids)
	{
		ids.InsertAll({
			"stillbarn.1", "chopyard.yard", "saint-philippe.fence", "ne-compound.truckyard",
			"farmcompound.compound", "hilltopmast.a", "scrapyard.recycler", "saint-philippe-dock.warehouse"
		});
	}

	//------------------------------------------------------------------------------------------------
	//! The recorded conversations the rows are spoken through: the dialogue config of each voice.
	protected static void Conversations(array<string> ids, array<ResourceName> paths)
	{
		ids.Insert("rumors_odilon");
		paths.Insert("{7210F7AE6E96724D}Configs/Dialogue/LF_Dialogue_RumorsOdilon.conf");
		ids.Insert("rumors_colin");
		paths.Insert("{8CD27CFE9BABBD00}Configs/Dialogue/LF_Dialogue_RumorsColin.conf");
		ids.Insert("rumors_bar");
		paths.Insert("{1DD15AB2CC99721A}Configs/Dialogue/LF_Dialogue_RumorsBar.conf");
		ids.Insert("rumors_informant");
		paths.Insert("{25BDC240277AD840}Configs/Dialogue/LF_Dialogue_RumorsInformant.conf");
	}

	//------------------------------------------------------------------------------------------------
	//! Spoken French that has no place in a US English caption; the full sign-word list is validate-us-english.
	protected static void FrenchWords(array<string> words)
	{
		words.InsertAll({
			"bonjour", "bonsoir", "salut", "merci", "monsieur", "madame", "mademoiselle", "mme", "mlle", "oui", "voila",
			"ami", "amis", "chere", "cher", "gendarmerie", "mairie", "boulangerie", "pharmacie", "banque", "poste", "ouvert",
			"ferme", "au revoir", "mon dieu", "n'est-ce pas", "enfin", "alors", "bien sur", "quelle", "peut-etre"
		});
	}

	//------------------------------------------------------------------------------------------------
	//! Words that would print a hidden fact: an open case, a record, a name on a file. Crime rumors say only what a
	//! passer-by could see (lights, a truck, saws on the road).
	protected static void HiddenFactWords(array<string> words)
	{
		words.InsertAll({
			"warrant", "wanted", "suspect", "suspects", "record", "records", "case", "cases", "investigation", "investigating",
			"evidence", "file", "charged", "arrested", "arrest", "convicted", "informant", "police report", "detective"
		});
	}

	//------------------------------------------------------------------------------------------------
	//! The lower-cased caption with its punctuation as spaces, padded, so a whole word is " word ".
	protected static string Plain(string text)
	{
		string plain = text;
		plain.ToLower();
		array<string> marks = { ".", ",", "!", "?", ";", ":", "\"", "(", ")", "-" };
		foreach (string mark : marks)
			plain.Replace(mark, " ");

		return " " + plain + " ";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasText(string key)
	{
		string text = WidgetManager.Translate(key);
		return !key.IsEmpty() && !text.IsEmpty() && text != key;
	}

	//------------------------------------------------------------------------------------------------
	//! The pack's rumor rows, loaded the way LF_RumorsConfig loads the core file. Null when they do not resolve.
	protected static LF_RumorsConfig LoadRows()
	{
		Resource container = BaseContainerTools.LoadContainer(ROWS);
		if (!container || !container.GetResource())
			return null;

		return LF_RumorsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Offered(array<ref LF_RumorDef> offers, string id)
	{
		foreach (LF_RumorDef def : offers)
		{
			if (def && def.m_sId == id)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every recorded line of every conversation: text key to the line, and the conversation it is in.
	protected static void CollectLines(array<string> ids, array<ResourceName> paths, map<string, ref LF_DialogueLine> byKey, map<string, string> owner, LF_TestContext ctx)
	{
		for (int i = 0; i < ids.Count(); i++)
		{
			LF_DialogueConfig config = LF_DialogueConfig.Layered(paths[i]);
			ctx.NotNull(config, ids[i] + ": the recorded conversation loads");
			if (!config || !config.m_aLines)
				continue;

			ctx.EqualStr(ids[i], config.m_sId, ids[i] + ": the conversation is its own");
			ctx.True(!config.m_sSoundProject.IsEmpty(), ids[i] + ": names an audio project");

			foreach (LF_DialogueLine line : config.m_aLines)
			{
				if (!line)
					continue;

				byKey.Set(line.m_sTextKey, line);
				owner.Set(line.m_sTextKey, ids[i]);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.NotNull(LF_MissionHeader.GetCurrent(), "the pack leg runs under an LF mission header");

		array<string> french = {};
		FrenchWords(french);
		array<string> hidden = {};
		HiddenFactWords(hidden);
		array<string> placeKinds = {};
		array<string> placeIds = {};
		Places(placeKinds, placeIds);
		array<string> crimePlaces = {};
		CrimePlaces(crimePlaces);
		array<string> convIds = {};
		array<ResourceName> convPaths = {};
		Conversations(convIds, convPaths);

		// The act: load the rows the way the core does. Red proof: no load, so there are no rows.
		LF_RumorsConfig config;
		if (!RedProof())
			config = LoadRows();

		ctx.NotNull(config, "the pack's rumor rows resolve");
		if (!config || !config.m_aRumors)
		{
			ctx.True(false, "the pack's rumor rows resolve");
			return;
		}

		ctx.True(config.m_aRumors.Count() >= MIN_ROWS, string.Format("the pack has at least %1 rumors (%2)", MIN_ROWS, config.m_aRumors.Count()));
		ctx.True(config.m_aCrimeGivers && config.m_aCrimeGivers.Find("bar") != -1 && config.m_aCrimeGivers.Find("informant") != -1, "the crime givers are bar staff and the informant");

		map<string, ref LF_DialogueLine> lineByKey = new map<string, ref LF_DialogueLine>();
		map<string, string> ownerByKey = new map<string, string>();
		CollectLines(convIds, convPaths, lineByKey, ownerByKey, ctx);

		set<string> seenIds = new set<string>();
		set<string> seenKeys = new set<string>();
		int crimeRows;
		int gossipRows;
		foreach (LF_RumorDef def : config.m_aRumors)
		{
			if (!def)
				continue;

			string where = def.m_sId;
			ctx.True(!def.m_sId.IsEmpty() && !seenIds.Contains(def.m_sId), where + ": has a unique id");
			seenIds.Insert(def.m_sId);

			// Place: a real island place, found by id in the list the layers register.
			int placeAt = placeIds.Find(def.m_sPlaceId);
			ctx.True(placeAt != -1, where + ": its place " + def.m_sPlaceId + " is an island place");
			bool crimePlace = crimePlaces.Find(def.m_sPlaceId) != -1;
			ctx.True(def.m_bCrime == crimePlace, where + ": the crime flag matches its place");

			// Givers: the row says who tells it.
			ctx.True(def.m_aGivers && def.m_aGivers.Count() > 0, where + ": names who tells it");
			if (def.m_aGivers)
			{
				foreach (string giver : def.m_aGivers)
				{
					bool crimeGiver = giver == "bar" || giver == "informant";
					if (def.m_bCrime)
						ctx.True(crimeGiver, where + ": a crime row is told only by bar staff or the informant (" + giver + ")");
					else
						ctx.True(giver == "gossip" || giver == "bar" || giver.IndexOf("gossip_") == 0, where + ": a plain row is told by a gossip or the bar (" + giver + ")");
				}
			}

			if (def.m_bCrime)
				crimeRows++;
			else if (def.m_aGivers && def.m_aGivers.Find("gossip") != -1)
				gossipRows++;

			// Voice: the line is recorded in a speaker's own conversation, with its clip on disk.
			ctx.True(!seenKeys.Contains(def.m_sLineKey), where + ": has its own line");
			seenKeys.Insert(def.m_sLineKey);
			string spoken = def.m_sLineKey;
			LF_DialogueLine line;
			ctx.True(lineByKey.Find(spoken, line) && line, where + ": its line is recorded");
			if (!line)
				continue;

			string speaker = ownerByKey.Get(spoken);
			ctx.True(!line.m_sEvent.IsEmpty(), where + ": has a sound event");
			ctx.True(line.m_fSeconds > 0, where + ": has a length");
			string clip = "$LF_Everon1989:Sounds/Dialogue/" + speaker + "/" + speaker + "_" + line.m_sId + ".wav";
			Resource wav = Resource.Load(clip);
			ctx.True(wav && wav.IsValid(), where + ": has its clip on disk");

			// Words: US English, and a crime row prints only a public fact.
			ctx.True(HasText(def.m_sLineKey), where + ": has its caption");
			string caption = Plain(WidgetManager.Translate(def.m_sLineKey));
			foreach (string word : french)
				ctx.False(caption.Contains(" " + word + " "), where + ": no French word (" + word + ")");

			if (def.m_bCrime)
			{
				foreach (string fact : hidden)
					ctx.False(caption.Contains(" " + fact + " "), where + ": prints no hidden fact (" + fact + ")");
			}
		}

		ctx.True(crimeRows >= 6, string.Format("the pack has crime rumors (%1)", crimeRows));
		ctx.True(gossipRows >= 12, string.Format("the gossips have rumors to tell (%1)", gossipRows));

		// The pool applies the crime rule to the pack's rows (N32).
		LF_MapKnown.ResetStatics();
		LF_RumorsConfig.ResetStatics();
		LF_RumorsConfig.UseForTest(config);
		LF_RumorState state = new LF_RumorState();

		array<ref LF_RumorDef> offers = LF_RumorPool.Offers(CHARACTER, "gossip", LF_ResidentMemory.TIER_REGULAR, state);
		ctx.True(offers.Count() >= 12, string.Format("a gossip has rumors for a regular (%1)", offers.Count()));
		foreach (LF_RumorDef offered : offers)
			ctx.False(offered.m_bCrime, "a gossip never tells a crime rumor: " + offered.m_sId);

		offers = LF_RumorPool.Offers(CHARACTER, "bar", LF_ResidentMemory.TIER_NEUTRAL, state);
		ctx.True(offers.Count() > 0, "the bar tells a neutral player its plain rumors");
		ctx.False(Offered(offers, "bar_still"), "the bar keeps the crime rumor from a neutral player");

		offers = LF_RumorPool.Offers(CHARACTER, "bar", LF_ResidentMemory.TIER_FRIENDLY, state);
		ctx.True(Offered(offers, "bar_still"), "the bar tells a friendly player the still barn");
		ctx.False(Offered(offers, "inf_trucks"), "the bar does not tell the informant's rumors");

		offers = LF_RumorPool.Offers(CHARACTER, "informant", LF_ResidentMemory.TIER_FRIENDLY, state);
		ctx.True(Offered(offers, "inf_trucks"), "the informant tells a friendly player the truck yard");
		offers = LF_RumorPool.Offers(CHARACTER, "informant", LF_ResidentMemory.TIER_WARY, state);
		ctx.True(offers.Count() == 0, "the informant tells a wary player nothing");

		LF_RumorsConfig.ResetStatics();
		LF_MapKnown.ResetStatics();
	}
}
