// LF_Credits.c — the credits notice (#481).
// The card a player reads on the notice beside the post office lists who and what Life Framework is
// built on. The lines come from docs/credits.md, the source the website's /credits page uses:
// tools/credits/gen.mjs writes LF_CreditsRows.generated.c and the string rows.
// Client side and static: the sign's Read action opens it through LF_SignCard, so there is no RPC
// and nothing to persist.

class LF_Credits
{
	//! The LF_Signs.conf entry and the m_sSignId of the placed notice.
	static const string SIGN_ID = "credits";
	//! Last line of the card: where the full list with licences is.
	static const string KEY_MORE = "#LF-Credits_More";

	//------------------------------------------------------------------------------------------------
	static bool IsCreditsSign(LF_SignDef sign)
	{
		return sign && sign.m_sId == SIGN_ID;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the string table has no text for a key: the engine hands the key back, with or
	//! without its '#'.
	static bool IsMissing(string key, string text)
	{
		if (text.IsEmpty())
			return true;

		if (text == key)
			return true;

		return "#" + text == key;
	}

	//------------------------------------------------------------------------------------------------
	//! The card: title, rule, a label and one body per section, the pointer to the full list, Close.
	//! A line whose key the string table lacks is logged and left out; the rest still shows.
	//! \param[in] kinds "H" or "R" per row; null takes the generated list.
	//! \param[in] keys the string key per row.
	static array<ref LF_UIPart> BuildParts(LF_SignDef sign, array<string> kinds = null, array<string> keys = null)
	{
		ref array<ref LF_UIPart> parts = {};
		if (!sign)
		{
			LF_Debug.Error("Signs", "credits card refused: no sign");
			return parts;
		}

		ref array<string> ownKinds = {};
		ref array<string> ownKeys = {};
		if (!kinds || !keys)
		{
			LF_CreditsRows.Fill(ownKinds, ownKeys);
			kinds = ownKinds;
			keys = ownKeys;
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", LF_SignCatalog.Resolve(sign.m_sTitle)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RULE, "TitleRule", ""));

		string heading;
		string body;
		int section;
		int missing;
		for (int i = 0; i < kinds.Count() && i < keys.Count(); i++)
		{
			string text = LF_SignCatalog.Resolve(keys[i]);
			if (IsMissing(keys[i], text))
			{
				LF_Debug.Error("Signs", "credits: the string table has no " + keys[i] + "; that line is left out");
				missing = missing + 1;
				continue;
			}

			if (kinds[i] == "H")
			{
				AddSection(parts, section, heading, body);
				section = section + 1;
				heading = text;
				body = "";
				continue;
			}

			if (!body.IsEmpty())
				body += "<br/>";

			body += text;
		}

		AddSection(parts, section, heading, body);

		string more = LF_SignCatalog.Resolve(KEY_MORE);
		if (!IsMissing(KEY_MORE, more))
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RICH, "More", more));

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", LF_SignCatalog.KEY_CLOSE));
		LF_Debug.Info("Signs", string.Format("credits card built: %1 section(s), %2 line(s) left out", section, missing));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! One section: its label in capitals and its lines. A section with no line is left out.
	protected static void AddSection(array<ref LF_UIPart> parts, int index, string heading, string body)
	{
		if (body.IsEmpty())
			return;

		string name = "Section" + index.ToString();
		if (!heading.IsEmpty())
		{
			heading.ToUpper();
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, name, heading));
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RICH, name + "Body", body));
	}
}
