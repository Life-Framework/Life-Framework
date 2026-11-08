// LF_SignCatalog.c — sign text from Configs/Signs/LF_Signs.conf.
//
// A field that starts with # is a localization key. Anything else is shown as
// written, so a server owner can retitle a sign in config without touching the
// string table. An empty section is left off the card.

//------------------------------------------------------------------------------------------------
//! One sign. LF_SignComponent.m_sSignId names it.
[BaseContainerProps()]
class LF_SignDef
{
	[Attribute("", UIWidgets.EditBox, "Sign id, matched by LF_SignComponent.m_sSignId")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "Title. Shown in the prompt as Read: <title>")]
	string m_sTitle;

	[Attribute("", UIWidgets.EditBox, "What this place is")]
	string m_sWhat;

	[Attribute("", UIWidgets.EditBox, "How to use it, with the exact action names")]
	string m_sHow;

	[Attribute("", UIWidgets.EditBox, "Items, faction, or people it needs")]
	string m_sNeeds;

	[Attribute("", UIWidgets.EditBox, "Cooldowns, wanted level, other rules")]
	string m_sRules;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_SignConfig
{
	[Attribute("", UIWidgets.Object, "Signs")]
	ref array<ref LF_SignDef> m_aSigns;
}

//------------------------------------------------------------------------------------------------
class LF_SignCatalog
{
	static const ResourceName CONFIG_PATH = "{FFBF3D8E308E7E7E}Configs/Signs/LF_Signs.conf";

	static const string KEY_READ = "#LF-Sign_Read";
	static const string KEY_WHAT = "#LF-Sign_What";
	static const string KEY_HOW = "#LF-Sign_How";
	static const string KEY_NEEDS = "#LF-Sign_Needs";
	static const string KEY_RULES = "#LF-Sign_Rules";
	static const string KEY_CLOSE = "#LF-Sign_Close";

	protected static ref LF_SignConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! Loads a sign config. Logs and returns null when it does not resolve.
	static LF_SignConfig Load(ResourceName path)
	{
		Resource container = BaseContainerTools.LoadContainer(path);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			EL_Debug.Error("Signs", "sign config does not resolve: " + path);
			return null;
		}

		LF_SignConfig config = LF_SignConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
			EL_Debug.Error("Signs", "sign config has an unexpected root class: " + path);

		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureLoaded()
	{
		if (s_bLoadAttempted)
			return;

		s_bLoadAttempted = true;
		s_Config = Load(CONFIG_PATH);
	}

	//------------------------------------------------------------------------------------------------
	//! The sign with this id in LF_Signs.conf, or null.
	static LF_SignDef Find(string id)
	{
		EnsureLoaded();
		return FindIn(s_Config, id);
	}

	//------------------------------------------------------------------------------------------------
	static LF_SignDef FindIn(LF_SignConfig config, string id)
	{
		if (id.IsEmpty() || !config || !config.m_aSigns)
			return null;

		foreach (LF_SignDef sign : config.m_aSigns)
		{
			if (sign && sign.m_sId == id)
				return sign;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! A sign is usable when it has a title. A sign with no title would read "Read: ".
	static bool IsUsable(LF_SignDef sign)
	{
		return sign && !sign.m_sTitle.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsKey(string value)
	{
		return value.Length() > 1 && value.Get(0) == "#";
	}

	//------------------------------------------------------------------------------------------------
	//! A #key is translated. Anything else is returned as written.
	static string Resolve(string value)
	{
		if (!IsKey(value))
			return value;

		return WidgetManager.Translate(value);
	}

	//------------------------------------------------------------------------------------------------
	//! The prompt text: Read: <title>.
	static string ReadName(LF_SignDef sign)
	{
		if (!sign)
			return string.Empty;

		return WidgetManager.Translate(KEY_READ, Resolve(sign.m_sTitle));
	}

	//------------------------------------------------------------------------------------------------
	//! Title, then one line per filled section in the order What, How, Needs, Rules, then Close.
	static array<ref LF_UIPart> BuildParts(LF_SignDef sign)
	{
		ref array<ref LF_UIPart> parts = {};
		if (!sign)
			return parts;

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", Resolve(sign.m_sTitle)));
		AddSection(parts, "What", KEY_WHAT, sign.m_sWhat);
		AddSection(parts, "How", KEY_HOW, sign.m_sHow);
		AddSection(parts, "Needs", KEY_NEEDS, sign.m_sNeeds);
		AddSection(parts, "Rules", KEY_RULES, sign.m_sRules);
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", KEY_CLOSE));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddSection(array<ref LF_UIPart> parts, string name, string headingKey, string value)
	{
		string body = Resolve(value);
		body.TrimInPlace();
		if (body.IsEmpty())
			return;

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, name, Resolve(headingKey) + ": " + body));
	}
}
