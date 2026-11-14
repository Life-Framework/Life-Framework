//------------------------------------------------------------------------------------------------
//! One multiple-choice question of the rules orientation quiz (#915). Text fields take a #key or
//! plain text. The right answer is an index into the options and never leaves the server.
[BaseContainerProps()]
class LF_QuizQuestion
{
	[Attribute("", UIWidgets.Auto, "Stable id of the question, written in the log lines")]
	string m_sId;

	[Attribute("", UIWidgets.Auto, "The question: a #key or plain text")]
	string m_sQuestion;

	[Attribute("", UIWidgets.Auto, "The answers a player picks from, in the order shown: a #key or plain text each")]
	ref array<string> m_aOptions;

	[Attribute("0", UIWidgets.Auto, "Index of the right option, counted from 0", params: "0 7")]
	int m_iCorrect;

	[Attribute("", UIWidgets.Auto, "The rule, shown after a wrong answer: a #key or plain text")]
	string m_sExplain;
}

//------------------------------------------------------------------------------------------------
//! The rules orientation quiz switches (#915): Configs/Rules/LF_Quiz.conf, then each pack overlay
//! of this class, then the owner's `rulesQuiz` section. The questions are LF_QuizBank. Server only:
//! the server asks, scores and remembers; a client is sent the question text and never the answers.
[BaseContainerProps(configRoot: true)]
class LF_QuizConfig
{
	static const ResourceName CONFIG_PATH = "{5A3D7E19C2B40F86}Configs/Rules/LF_Quiz.conf";

	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "rulesQuiz";

	static const int DEFAULT_PASS_PERCENT = 80;
	static const int PASS_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int PASS_MAX = 100;	// lf-fixed: the schema limit, mirrors the attribute params

	static const int DEFAULT_MAX_ATTEMPTS = 3;
	static const int ATTEMPTS_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int ATTEMPTS_MAX = 20;	// lf-fixed: the schema limit, mirrors the attribute params

	[Attribute("1", UIWidgets.CheckBox, "Ask new players the rules quiz. Off lets everyone through")]
	bool m_bEnabled;

	[Attribute("0", UIWidgets.CheckBox, "Ask on every connect, not once per account. The pass is still remembered for the staff tools")]
	bool m_bRepeatOnConnect;

	[Attribute("80", UIWidgets.Auto, "Percent of the questions that must be right to pass", params: "1 100")]
	int m_iPassPercent;

	[Attribute("3", UIWidgets.Auto, "Failed attempts before the player is kicked. 0 lets them retry for ever", params: "0 20")]
	int m_iMaxAttempts;

	protected static ref LF_QuizConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered config, or null (logged once) when the file does not resolve.
	static LF_QuizConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_Quiz.FEATURE, string.Format("rules quiz config does not resolve, the quiz is off: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_QuizConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(LF_Quiz.FEATURE, string.Format("rules quiz config has an unexpected root class, the quiz is off: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() layers the files again (/configreload, world end).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_QuizConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the pack overlays in load order, then the owner's "rulesQuiz" section, over this
	//! instance, which holds the core `.conf`.
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_QuizConfig, SECTION, resources, names);
		foreach (int i, Resource resource : resources)
		{
			ApplyPackContainer(resource.GetResource().ToBaseContainer(), names[i]);
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("rulesQuiz: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! One pack overlay: a field the file writes replaces the value.
	void ApplyPackContainer(notnull BaseContainer root, string source)
	{
		if (root.IsVariableSetDirectly("m_bEnabled"))
			root.Get("m_bEnabled", m_bEnabled);

		if (root.IsVariableSetDirectly("m_bRepeatOnConnect"))
			root.Get("m_bRepeatOnConnect", m_bRepeatOnConnect);

		if (root.IsVariableSetDirectly("m_iPassPercent"))
			root.Get("m_iPassPercent", m_iPassPercent);

		if (root.IsVariableSetDirectly("m_iMaxAttempts"))
			root.Get("m_iMaxAttempts", m_iMaxAttempts);
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "rulesQuiz" section field by field; a key left out keeps the layer below.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"enabled", "repeatOnConnect", "passPercent", "maxAttempts"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadBool("enabled", m_bEnabled);
		reader.ReadBool("repeatOnConnect", m_bRepeatOnConnect);
		reader.ReadInt("passPercent", m_iPassPercent, PASS_MIN, PASS_MAX);
		reader.ReadInt("maxAttempts", m_iMaxAttempts, ATTEMPTS_MIN, ATTEMPTS_MAX);
	}

	//------------------------------------------------------------------------------------------------
	static bool Enabled()
	{
		LF_QuizConfig config = Get();
		if (!config)
			return false;

		return config.m_bEnabled;
	}

	//------------------------------------------------------------------------------------------------
	static bool RepeatOnConnect()
	{
		LF_QuizConfig config = Get();
		if (!config)
			return false;

		return config.m_bRepeatOnConnect;
	}

	//------------------------------------------------------------------------------------------------
	static int PassPercent()
	{
		LF_QuizConfig config = Get();
		if (!config)
			return DEFAULT_PASS_PERCENT;

		return Math.ClampInt(config.m_iPassPercent, PASS_MIN, PASS_MAX);
	}

	//------------------------------------------------------------------------------------------------
	//! Failed attempts before a kick; 0 = never.
	static int MaxAttempts()
	{
		LF_QuizConfig config = Get();
		if (!config)
			return DEFAULT_MAX_ATTEMPTS;

		return Math.ClampInt(config.m_iMaxAttempts, ATTEMPTS_MIN, ATTEMPTS_MAX);
	}
}

//------------------------------------------------------------------------------------------------
//! The questions of the rules orientation quiz (#915): Configs/Rules/LF_QuizBank.conf ships none
//! (the core decides how the quiz works, the pack decides what this server's rules are). A pack
//! overlay of this class that lists questions replaces the list; a later overlay replaces an
//! earlier one. Not an owner section: the rules are the pack's copy, in its own language tables.
[BaseContainerProps(configRoot: true)]
class LF_QuizBank
{
	static const ResourceName CONFIG_PATH = "{8E41B6D07A2C9F35}Configs/Rules/LF_QuizBank.conf";

	[Attribute()]
	ref array<ref LF_QuizQuestion> m_aQuestions;

	protected static ref LF_QuizBank s_Bank;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered bank, or null (logged once) when the file does not resolve.
	static LF_QuizBank Get()
	{
		if (s_bLoadAttempted)
			return s_Bank;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_Quiz.FEATURE, string.Format("quiz question bank does not resolve, the quiz is off: %1", CONFIG_PATH));
			return null;
		}

		s_Bank = LF_QuizBank.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Bank)
			LF_Debug.Error(LF_Quiz.FEATURE, string.Format("quiz question bank has an unexpected root class, the quiz is off: %1", CONFIG_PATH));
		else
			s_Bank.ApplyLayers();

		return s_Bank;
	}

	//------------------------------------------------------------------------------------------------
	static void Reload()
	{
		s_Bank = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `bank` the one Get returns until Reload.
	static void UseForTest(LF_QuizBank bank)
	{
		s_Bank = bank;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Each pack overlay of this class in load order: a question list replaces the list.
	void ApplyLayers()
	{
		if (!m_aQuestions)
			m_aQuestions = new array<ref LF_QuizQuestion>();

		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_QuizBank, "rulesQuiz", resources, names);
		foreach (int i, Resource resource : resources)
		{
			ApplyPackContainer(resource.GetResource().ToBaseContainer(), names[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	void ApplyPackContainer(notnull BaseContainer root, string source)
	{
		BaseContainerList entries = root.GetObjectArray("m_aQuestions");
		if (!entries || entries.Count() == 0)
			return;

		array<ref LF_QuizQuestion> questions = new array<ref LF_QuizQuestion>();
		for (int i = 0; i < entries.Count(); i++)
		{
			BaseContainer entry = entries.Get(i);
			if (!entry)
				continue;

			LF_QuizQuestion question = LF_QuizQuestion.Cast(BaseContainerTools.CreateInstanceFromContainer(entry));
			if (question)
				questions.Insert(question);
		}

		m_aQuestions = questions;
		LF_Debug.Info(LF_Quiz.FEATURE, string.Format("pack overlay %1 set %2 quiz question(s)", source, questions.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! The questions a player can be asked: a question with no text, fewer than two options, a blank
	//! option or a right answer outside the options is logged and left out.
	static void GetQuestions(notnull array<LF_QuizQuestion> outQuestions)
	{
		outQuestions.Clear();
		LF_QuizBank bank = Get();
		if (!bank || !bank.m_aQuestions)
			return;

		foreach (LF_QuizQuestion question : bank.m_aQuestions)
		{
			string refusal = LF_Quiz.RefuseQuestion(question);
			if (!refusal.IsEmpty())
			{
				LF_Debug.Error(LF_Quiz.FEATURE, string.Format("quiz question skipped: %1", refusal));
				continue;
			}

			outQuestions.Insert(question);
		}
	}
}
