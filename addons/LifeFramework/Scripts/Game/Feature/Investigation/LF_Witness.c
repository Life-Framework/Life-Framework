//------------------------------------------------------------------------------------------------
//! Witnesses to a crime (#266, #267). When a crime report is filed the server records who stood
//! within sight of it (players and town NPCs) and how far away. A witness then describes the
//! suspect from what the SERVER knows the suspect wore and drove at the crime, limited by how far
//! the witness stood: clothing only close, the vehicle farther, a plate fragment that shrinks with
//! distance. Nothing a client sends decides a description; a client only names which thing it
//! wants to say (clothing, vehicle, plate).
//!   payphone      : LF_WitnessPayphoneAction, one entry per thing seen
//! Statements are session-local, like the dispatch log, and show under the dispatch log on the
//! wanted board (police view).
//------------------------------------------------------------------------------------------------
enum LF_EWitnessField
{
	CLOTHING,
	VEHICLE,
	PLATE
}

enum LF_EWitnessResult
{
	OK,
	NO_SCENE,
	NOT_A_WITNESS,
	NOT_SEEN,
	ALREADY_GIVEN
}

//------------------------------------------------------------------------------------------------
//! One witness of a scene: a player uid, or "npc:<id>" for a town NPC, and how far away it stood.
class LF_WitnessEntry : Managed
{
	string m_sKey;
	bool m_bNpc;
	float m_fDistance;
}

//------------------------------------------------------------------------------------------------
//! A crime scene's server-side facts and who saw it.
class LF_WitnessScene : Managed
{
	int m_iReport;
	string m_sSuspectUid;
	string m_sOffence;
	string m_sClothing;
	string m_sVehicle;
	string m_sPlate;
	string m_sPlace;
	int m_iTick;
	ref array<ref LF_WitnessEntry> m_aWitnesses = {};
}

//------------------------------------------------------------------------------------------------
//! What one witness said about one report.
class LF_WitnessStatement : Managed
{
	int m_iReport;
	string m_sOffence;
	string m_sWitnessKey;
	bool m_bNpc;
	//! Who the board names: a localization key for anonymous callers and NPCs, else a name.
	string m_sLabel;
	string m_sClothing;
	string m_sVehicle;
	string m_sPlateFragment;
	int m_iSequence;
}

//------------------------------------------------------------------------------------------------
//! What a witness at a given distance could see.
class LF_WitnessPresets : Managed
{
	bool m_bClothing;
	bool m_bVehicle;
	int m_iPlateChars;
}

//------------------------------------------------------------------------------------------------
class LF_Witness
{
	static const string FEATURE = "Witness";
	static const string NPC_PREFIX = "npc:";
	static const string LABEL_ANONYMOUS = "#LF-Witness_Anonymous";
	static const string ENTRY_PREFIX = "S;";
	static const string ENTRY_SEPARATOR = ";";

	protected static ref array<ref LF_WitnessScene> s_aScenes;
	protected static ref array<ref LF_WitnessStatement> s_aStatements;
	protected static int s_iSequence;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aScenes = null;
		s_aStatements = null;
		s_iSequence = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_WitnessScene> Scenes()
	{
		if (!s_aScenes)
			s_aScenes = {};

		return s_aScenes;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_WitnessStatement> Statements()
	{
		if (!s_aStatements)
			s_aStatements = {};

		return s_aStatements;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: plate characters a witness at this distance reads. Falls by one per step, none past the
	//! maximum range.
	static int PlateCharsSeen(float distance, int plateLength)
	{
		if (plateLength <= 0 || distance > LF_WitnessConfig.PlateMaxRange())
			return 0;

		int lost = Math.Floor(distance / LF_WitnessConfig.PlateStep());
		return Math.Max(0, plateLength - lost);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: what a witness this far from the suspect could see.
	static LF_WitnessPresets PresetsFor(float distance, int plateLength)
	{
		LF_WitnessPresets presets = new LF_WitnessPresets();
		presets.m_bClothing = distance <= LF_WitnessConfig.ClothingRange();
		presets.m_bVehicle = distance <= LF_WitnessConfig.VehicleRange();
		presets.m_iPlateChars = PlateCharsSeen(distance, plateLength);
		return presets;
	}

	//------------------------------------------------------------------------------------------------
	static string NpcKey(IEntity npc)
	{
		if (!npc)
			return string.Empty;

		return NPC_PREFIX + string.Format("%1", npc.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! The registration of the vehicle the suspect sits in, read on the server; empty on foot.
	static string PlateOf(IEntity suspect)
	{
		if (!suspect)
			return string.Empty;

		IEntity vehicle = CompartmentAccessComponent.GetVehicleIn(suspect);
		if (!vehicle)
			return string.Empty;

		LF_LicensePlateManagerComponent plates = LF_Component<LF_LicensePlateManagerComponent>.Find(vehicle);
		if (!plates)
			return string.Empty;

		return plates.m_Registration;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a crime report was filed. Records the scene and everyone in sight of the suspect.
	static LF_WitnessScene RecordCrime(int report, string suspectUid, string offence, IEntity suspect, string clothing, string vehicle, string place)
	{
		if (!Replication.IsServer() || !suspect)
			return null;

		LF_WitnessScene scene = OpenScene(report, suspectUid, offence, clothing, vehicle, PlateOf(suspect), place, System.GetTickCount());
		CollectPlayers(scene, suspect);
		LF_Debug.Info(FEATURE, string.Format("report #%1: %2 witness(es) in sight (plate '%3')", report, scene.m_aWitnesses.Count(), scene.m_sPlate));
		return scene;
	}

	//------------------------------------------------------------------------------------------------
	//! The seam tests drive: a scene with ready facts and no witnesses yet.
	static LF_WitnessScene OpenScene(int report, string suspectUid, string offence, string clothing, string vehicle, string plate, string place, int nowTick)
	{
		LF_WitnessScene scene = new LF_WitnessScene();
		scene.m_iReport = report;
		scene.m_sSuspectUid = suspectUid;
		scene.m_sOffence = offence;
		scene.m_sClothing = clothing;
		scene.m_sVehicle = vehicle;
		scene.m_sPlate = plate;
		scene.m_sPlace = place;
		scene.m_iTick = nowTick;

		array<ref LF_WitnessScene> scenes = Scenes();
		scenes.Insert(scene);
		while (scenes.Count() > LF_WitnessConfig.MaxScenes())
		{
			scenes.RemoveOrdered(0);
		}

		return scene;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a witness at a distance when within its sight range. Returns true when recorded.
	static bool AddWitness(notnull LF_WitnessScene scene, string key, bool npc, float distance)
	{
		float range = LF_WitnessConfig.SightRange();
		if (npc)
			range = LF_WitnessConfig.NpcSightRange();

		if (key.IsEmpty() || key == scene.m_sSuspectUid || distance > range || FindEntry(scene, key))
			return false;

		LF_WitnessEntry entry = new LF_WitnessEntry();
		entry.m_sKey = key;
		entry.m_bNpc = npc;
		entry.m_fDistance = distance;
		scene.m_aWitnesses.Insert(entry);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_WitnessEntry FindEntry(notnull LF_WitnessScene scene, string key)
	{
		foreach (LF_WitnessEntry entry : scene.m_aWitnesses)
		{
			if (entry.m_sKey == key)
				return entry;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectPlayers(notnull LF_WitnessScene scene, IEntity suspect)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity body = players.GetPlayerControlledEntity(playerId);
			if (!body || body == suspect || !LF_PlayerInteraction.IsAlive(body))
				continue;

			AddWitness(scene, LF_Utils.GetPlayerUID(playerId), false, vector.Distance(body.GetOrigin(), suspect.GetOrigin()));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The newest open scene this witness stood in sight of, or null.
	static LF_WitnessScene SceneFor(string witnessKey, int nowTick)
	{
		array<ref LF_WitnessScene> scenes = Scenes();
		int windowMs = LF_WitnessConfig.WindowSeconds() * 1000;
		for (int i = scenes.Count() - 1; i >= 0; i--)
		{
			LF_WitnessScene scene = scenes[i];
			if (nowTick - scene.m_iTick > windowMs)
				continue;

			if (FindEntry(scene, witnessKey))
				return scene;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_WitnessStatement FindStatement(notnull LF_WitnessScene scene, notnull LF_WitnessEntry entry)
	{
		foreach (LF_WitnessStatement existing : Statements())
		{
			if (existing.m_iReport == scene.m_iReport && existing.m_sWitnessKey == entry.m_sKey)
				return existing;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! A statement is kept only once the witness said something (Keep), so a refused ask leaves no row.
	protected static LF_WitnessStatement NewStatement(notnull LF_WitnessScene scene, notnull LF_WitnessEntry entry)
	{
		LF_WitnessStatement statement = new LF_WitnessStatement();
		statement.m_iReport = scene.m_iReport;
		statement.m_sOffence = scene.m_sOffence;
		statement.m_sWitnessKey = entry.m_sKey;
		statement.m_bNpc = entry.m_bNpc;
		statement.m_sLabel = LabelFor(entry);
		return statement;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Keep(notnull LF_WitnessStatement statement)
	{
		array<ref LF_WitnessStatement> statements = Statements();
		s_iSequence++;
		statement.m_iSequence = s_iSequence;
		statements.Insert(statement);
		while (statements.Count() > LF_WitnessConfig.MaxStatements())
		{
			statements.RemoveOrdered(0);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static string LabelFor(notnull LF_WitnessEntry entry)
	{
		if (LF_WitnessConfig.Anonymous())
			return LABEL_ANONYMOUS;

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts)
		{
			LF_PlayerAccount account = accounts.GetFromCache(entry.m_sKey);
			if (account && account.GetActiveCharacter())
				return account.GetActiveCharacter().GetFullName();
		}

		return LABEL_ANONYMOUS;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the witness says one thing it could see. Anything out of its range is refused.
	static LF_EWitnessResult Give(LF_WitnessScene scene, string witnessKey, LF_EWitnessField field, string channel)
	{
		if (!scene)
			return LF_EWitnessResult.NO_SCENE;

		LF_WitnessEntry entry = FindEntry(scene, witnessKey);
		if (!entry)
			return LF_EWitnessResult.NOT_A_WITNESS;

		LF_WitnessStatement statement = FindStatement(scene, entry);
		bool fresh = statement == null;
		if (fresh)
			statement = NewStatement(scene, entry);

		LF_EWitnessResult result = Fill(scene, entry, statement, field);
		if (result == LF_EWitnessResult.OK)
		{
			if (fresh)
				Keep(statement);

			Announce(scene, entry, statement, channel);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EWitnessResult Fill(notnull LF_WitnessScene scene, notnull LF_WitnessEntry entry, notnull LF_WitnessStatement statement, LF_EWitnessField field)
	{
		LF_WitnessPresets presets = PresetsFor(entry.m_fDistance, scene.m_sPlate.Length());
		switch (field)
		{
			case LF_EWitnessField.CLOTHING:
			{
				if (!presets.m_bClothing || scene.m_sClothing.IsEmpty())
					return LF_EWitnessResult.NOT_SEEN;

				if (!statement.m_sClothing.IsEmpty())
					return LF_EWitnessResult.ALREADY_GIVEN;

				statement.m_sClothing = scene.m_sClothing;
				return LF_EWitnessResult.OK;
			}

			case LF_EWitnessField.VEHICLE:
			{
				if (!presets.m_bVehicle || scene.m_sVehicle.IsEmpty())
					return LF_EWitnessResult.NOT_SEEN;

				if (!statement.m_sVehicle.IsEmpty())
					return LF_EWitnessResult.ALREADY_GIVEN;

				statement.m_sVehicle = scene.m_sVehicle;
				return LF_EWitnessResult.OK;
			}

			case LF_EWitnessField.PLATE:
			{
				if (presets.m_iPlateChars <= 0)
					return LF_EWitnessResult.NOT_SEEN;

				if (!statement.m_sPlateFragment.IsEmpty())
					return LF_EWitnessResult.ALREADY_GIVEN;

				statement.m_sPlateFragment = scene.m_sPlate.Substring(0, Math.Min(presets.m_iPlateChars, scene.m_sPlate.Length()));
				return LF_EWitnessResult.OK;
			}
		}

		return LF_EWitnessResult.NOT_SEEN;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Announce(notnull LF_WitnessScene scene, notnull LF_WitnessEntry entry, notnull LF_WitnessStatement statement, string channel)
	{
		LF_Debug.Info(FEATURE, string.Format("report #%1: statement by %2 via %3: clothing='%4' vehicle='%5' plate='%6'", scene.m_iReport, entry.m_sKey, channel, statement.m_sClothing, statement.m_sVehicle, statement.m_sPlateFragment));
		string witnessUid;
		if (!entry.m_bNpc)
			witnessUid = entry.m_sKey;

		LF_EventBus.Publish(LF_OnWitnessStatement.Create(scene.m_iReport, witnessUid, entry.m_bNpc, channel, statement.m_sClothing, statement.m_sVehicle, statement.m_sPlateFragment));
	}

	//------------------------------------------------------------------------------------------------
	static int StatementCount()
	{
		return Statements().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The statements on a report, oldest first.
	static void CollectFor(int report, notnull array<LF_WitnessStatement> outStatements)
	{
		outStatements.Clear();
		foreach (LF_WitnessStatement statement : Statements())
		{
			if (statement.m_iReport == report)
				outStatements.Insert(statement);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: appends statements as board entries, newest first, after whatever is in outEntries.
	//! "S;offence;clothing;vehicle;plate fragment;label", resolved into words on the reading client.
	static void AppendBoardEntries(int maxLines, notnull array<string> outEntries)
	{
		array<ref LF_WitnessStatement> statements = Statements();
		int added;
		for (int i = statements.Count() - 1; i >= 0; i--)
		{
			if (maxLines > 0 && added >= maxLines)
				break;

			LF_WitnessStatement statement = statements[i];
			if (statement.m_sClothing.IsEmpty() && statement.m_sVehicle.IsEmpty() && statement.m_sPlateFragment.IsEmpty())
				continue;

			outEntries.Insert(string.Format("S;%1;%2;%3;%4;%5", statement.m_sOffence, statement.m_sClothing, statement.m_sVehicle, statement.m_sPlateFragment, statement.m_sLabel));
			added++;
		}
	}

	//------------------------------------------------------------------------------------------------
	static bool IsBoardEntry(string entry)
	{
		return entry.StartsWith(ENTRY_PREFIX);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one statement entry as a line. A malformed entry reads as an empty string.
	static string BoardLine(string entry)
	{
		array<string> fields = {};
		entry.Split(ENTRY_SEPARATOR, fields, false);
		if (fields.Count() < 6)
		{
			LF_Debug.Error(FEATURE, string.Format("witness board entry dropped: '%1'", entry));
			return string.Empty;
		}

		string description = LF_WantedBoard.PosterDescription(fields[2], fields[3], string.Empty);
		if (!fields[4].IsEmpty())
		{
			if (!description.IsEmpty())
				description += " ";

			description += WidgetManager.Translate("#LF-Witness_BoardPlate", fields[4]);
		}

		return WidgetManager.Translate("#LF-Witness_BoardLine", LF_WantedBoard.ResolveKey(fields[5]), LF_WantedBoard.ResolveKey(fields[1]), description);
	}
}
