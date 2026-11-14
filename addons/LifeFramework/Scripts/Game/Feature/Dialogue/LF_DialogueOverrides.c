//------------------------------------------------------------------------------------------------
//! Dev caption overrides (#970): change a dialogue line or a reply and see it in the running
//! session, without a restart.
//!
//! The engine caches a `.conf` (Resource.Load "gets it from cache") and reads the string table once,
//! so neither can be read again in a running game. A caption is a localization key, so this layer sits
//! in front of the translation: the dev file `$profile:LifeFramework/dev/dialogue.json` on the server
//! maps a key to the text to show instead,
//!   {"captions": {"#LF-Dialogue_Clerk_Greet": "Good morning. Papers, please."}}
//! `reload dialogue` (LF_ContentReloader_Dialogue) reads it, logs what changed and the config stream
//! sends the map to every client (a `devDialogue` section, written only while the map is not empty,
//! so a payload with no overrides is byte for byte what it was). The scene asks Text(key) for every
//! caption, reply and speaker name it shows.
//!
//! Dev only: the file is read only where LF_ContentReload.DevAllowed (dev tools on in the game mode,
//! or a harness build). A live server never reads it. The text is not spoken: the voice is the
//! recording; a caption that differs from it is for iterating on wording before the pipeline reruns.
//! Structure (nodes, replies, hooks) lives in the `.conf` and needs a restart.
class LF_DialogueOverrides
{
	static const string PATH = "$profile:LifeFramework/dev/dialogue.json";
	static const string SECTION = "devDialogue";
	static const string KEY_CAPTIONS = "captions";

	//! The key's replacement text. Server: what the last reload read. Client: what the stream carried.
	protected static ref map<string, string> s_mCaptions = new map<string, string>();

	//------------------------------------------------------------------------------------------------
	//! The text to show for a caption key: the override when there is one, else the key itself (the
	//! widget translates a `#key`).
	static string Text(string key)
	{
		string text;
		if (s_mCaptions && s_mCaptions.Find(key, text))
			return text;

		return key;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the key has an override.
	static bool Has(string key)
	{
		return s_mCaptions && s_mCaptions.Contains(key);
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		if (!s_mCaptions)
			return 0;

		return s_mCaptions.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The overrides, for the config stream to write. Never null.
	static map<string, string> GetCaptions()
	{
		if (!s_mCaptions)
			s_mCaptions = new map<string, string>();

		return s_mCaptions;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every override (world end, tests).
	static void Reset()
	{
		s_mCaptions = new map<string, string>();
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the `captions` object of a document whose cursor is at the object that holds it. A value
	//! that is not text is skipped with a warning; one bad value never costs the others. Never a VME.
	//! \param[in] ctx A load context with the holding object open.
	//! \param[in] source Where the text came from, for log lines.
	//! \param[out] into The key to text pairs read.
	//! \return Pairs read.
	static int ReadCaptions(notnull SCR_JsonLoadContext ctx, string source, notnull map<string, string> into)
	{
		LF_EConfigJsonKind kind = LF_ConfigReader.KindOf(ctx, KEY_CAPTIONS);
		if (kind == LF_EConfigJsonKind.ABSENT)
			return 0;

		if (kind != LF_EConfigJsonKind.OBJECT)
		{
			LF_Debug.Error(LF_Dialogue.FEATURE, string.Format("%1: \"%2\" is not an object of key to text; no overrides read", source, KEY_CAPTIONS));
			return 0;
		}

		array<string> keys = {};
		if (!LF_ConfigReader.ListKeys(ctx, KEY_CAPTIONS, keys))
			return 0;

		if (!ctx.StartObject(KEY_CAPTIONS))
			return 0;

		int read;
		foreach (string key : keys)
		{
			string text;
			if (!ctx.ReadValue(key, text))
			{
				LF_Debug.Warn(LF_Dialogue.FEATURE, string.Format("%1: %2 is not text; skipped", source, key));
				continue;
			}

			into.Set(key, text);
			read++;
		}
		ctx.EndObject();
		return read;
	}

	//------------------------------------------------------------------------------------------------
	//! Parses the dev file's text.
	//! \param[in] json The document.
	//! \param[in] source Where it came from, for log lines.
	//! \param[out] into The pairs read (left empty when the text is not JSON).
	//! \return False when the text is not valid JSON.
	static bool Parse(string json, string source, notnull map<string, string> into)
	{
		into.Clear();
		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext(false);
		if (!ctx.ImportFromString(json))
		{
			LF_Debug.Error(LF_Dialogue.FEATURE, string.Format("%1 is not valid JSON; the overrides stay as they were", source));
			return false;
		}

		ReadCaptions(ctx, source, into);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the overrides and says what changed, one line per key.
	//! \return A summary like "2 added, 1 changed, 0 removed".
	static string Replace(notnull map<string, string> next)
	{
		map<string, string> before = GetCaptions();
		int added;
		int changed;
		int removed;
		foreach (string nextKey, string nextText : next)
		{
			string old;
			if (!before.Find(nextKey, old))
			{
				added++;
				LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("reload: %1 added [%2]", nextKey, nextText));
			}
			else if (old != nextText)
			{
				changed++;
				LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("reload: %1 changed [%2] -> [%3]", nextKey, old, nextText));
			}
		}
		foreach (string oldKey, string oldText : before)
		{
			if (!next.Contains(oldKey))
			{
				removed++;
				LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("reload: %1 removed", oldKey));
			}
		}

		map<string, string> kept = new map<string, string>();
		foreach (string keepKey, string keepText : next)
		{
			kept.Set(keepKey, keepText);
		}
		s_mCaptions = kept;
		return string.Format("%1 added, %2 changed, %3 removed, %4 override(s) now", added, changed, removed, s_mCaptions.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: reads the dev file again. A missing file means no overrides; a file that does not
	//! parse leaves the overrides as they were.
	//! \return What changed, for the reload log and the harness reply.
	static string ReloadFromFile(string path = PATH)
	{
		map<string, string> next = new map<string, string>();
		if (!FileIO.FileExists(path))
		{
			LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("reload: no %1; no overrides", path));
			return Replace(next);
		}

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			LF_Debug.Error(LF_Dialogue.FEATURE, string.Format("reload: cannot open %1; the overrides stay as they were", path));
			return "unreadable, unchanged";
		}

		string text;
		string line;
		while (handle.ReadLine(line) >= 0)
		{
			text += line + "\n";
		}
		handle.Close();

		if (!Parse(text, path, next))
			return "not JSON, unchanged";

		return Replace(next);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the stream section's content: the `captions` object.
	//! \param[in] ctx A save context with the `devDialogue` object open.
	static void WriteStream(notnull SCR_JsonSaveContext ctx, notnull map<string, string> captions)
	{
		ctx.StartObject(KEY_CAPTIONS);
		foreach (string captionKey, string captionText : captions)
		{
			ctx.WriteValue(captionKey, captionText);
		}
		ctx.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: takes the overrides from the last accepted config stream, or none when it has no
	//! `devDialogue` section. The server keeps the map it read itself.
	static void ApplyReceived()
	{
		if (Replication.IsServer())
			return;

		ApplyPayload(LF_ConfigStream.GetReceivedPayload());
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the overrides with the `devDialogue` section of a stream payload; a payload with no
	//! such section (or an empty one) clears them.
	//! \return What changed.
	static string ApplyPayload(string payload)
	{
		map<string, string> next = new map<string, string>();
		if (!payload.IsEmpty())
		{
			SCR_JsonLoadContext ctx = new SCR_JsonLoadContext(false);
			if (ctx.ImportFromString(payload) && LF_ConfigReader.KindOf(ctx, SECTION) == LF_EConfigJsonKind.OBJECT && ctx.StartObject(SECTION))
			{
				ReadCaptions(ctx, "stream", next);
				ctx.EndObject();
			}
		}

		int had = Count();
		string summary = Replace(next);
		if (next.Count() > 0 || had > 0)
			LF_Debug.Info(LF_Dialogue.FEATURE, string.Format("stream: dialogue overrides %1", summary));
		return summary;
	}
}
