// LF_HudLogic.c: the rules behind the subtle HUD (LF_BetaHud): when the needs show, when a need
// reads low, how bright the money block is, how many wanted pips fill, and the /hud settings.
// Pure: no widgets, no world, so LF_Test_HudLogic pins every rule.

//------------------------------------------------------------------------------------------------
class LF_HudLogic
{
	static const string FEATURE = "Hud";

	//! Hunger and thirst run 0 (empty) to 100 (full). The needs line shows below SHOW_BELOW,
	//! reads red below LOW_BELOW and stays hidden while both are comfortably high.
	static const int NEED_SHOW_BELOW = 50;	// lf-fixed: half full is when a player starts to care
	static const int NEED_LOW_BELOW = 25;	// lf-fixed: the same quarter the survival penalty bites at
	//! Seconds the needs line stays up after eating or drinking raised a value.
	static const float NEED_RISE_SECONDS = 4.0;	// lf-float: UI seconds

	//! Seconds without a money change before the block dims, and how far it dims.
	static const float MONEY_IDLE_SECONDS = 5.0;	// lf-float: UI seconds
	static const float MONEY_DIM_OPACITY = 0.4;
	static const float MONEY_FULL_OPACITY = 0.95;
	//! Seconds a "+$100" delta stays before it fades.
	static const float DELTA_SECONDS = 3.0;	// lf-float: UI seconds

	static const int WANTED_PIPS = 5;
	//! Extra seconds the delta and the wanted caption stay, set only by the `hud ... hold` harness
	//! verb so a screenshot (several seconds to capture) catches them. 0 in play.
	protected static float s_fHarnessHold;

	//! The bank mirror before the server has written a balance.
	static const int BANK_UNKNOWN = int.MIN;
	//! Seconds the "Wanted" caption shows after the level rises.
	static const float WANTED_CAPTION_SECONDS = 3.0;	// lf-float: UI seconds

	//------------------------------------------------------------------------------------------------
	static float DeltaSeconds()
	{
		return DELTA_SECONDS + s_fHarnessHold;
	}

	//------------------------------------------------------------------------------------------------
	static float WantedCaptionSeconds()
	{
		return WANTED_CAPTION_SECONDS + s_fHarnessHold;
	}

	//------------------------------------------------------------------------------------------------
	static void SetHarnessHold(float seconds)
	{
		s_fHarnessHold = Math.Max(seconds, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the needs line should be on screen.
	static bool NeedsVisible(int hunger, int thirst, float secondsSinceRise)
	{
		if (hunger < NEED_SHOW_BELOW || thirst < NEED_SHOW_BELOW)
			return true;

		return secondsSinceRise >= 0 && secondsSinceRise < NEED_RISE_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when one need reads low (red tint).
	static bool NeedLow(int value)
	{
		return value < NEED_LOW_BELOW;
	}

	//------------------------------------------------------------------------------------------------
	//! A need as the HUD prints it: whole points, clamped to 0..100.
	static string NeedText(int value)
	{
		return LF_Utils.ClampInt(value, 0, 100).ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The money block's target opacity. Forced (inventory or map open) is always full.
	static float MoneyOpacity(float idleSeconds, bool forced)
	{
		if (forced || idleSeconds < MONEY_IDLE_SECONDS)
			return MONEY_FULL_OPACITY;

		return MONEY_DIM_OPACITY;
	}

	//------------------------------------------------------------------------------------------------
	//! The delta line for a money change: "+$100", "-$18", empty for no change.
	static string DeltaText(int oldAmount, int newAmount)
	{
		return LF_MoneyFormat.FormatDelta(newAmount - oldAmount);
	}

	//------------------------------------------------------------------------------------------------
	//! The bank amount (a debt prints as -$150), or empty while the balance is not known yet: the
	//! mirror holds BANK_UNKNOWN until the server writes one, a value no balance can take.
	static string BankAmountText(int bank)
	{
		if (bank == BANK_UNKNOWN)
			return "";

		return LF_MoneyFormat.FormatMoney(bank);
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many of the WANTED_PIPS badges are filled.
	static int WantedPips(int level)
	{
		return LF_Utils.ClampInt(level, 0, WANTED_PIPS);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the wanted change deserves the pulse and the caption (it went up).
	static bool WantedRose(int oldLevel, int newLevel)
	{
		return WantedPips(newLevel) > WantedPips(oldLevel);
	}
}


//------------------------------------------------------------------------------------------------
//! Which HUD parts this player wants. Kept on this client in $profile (LF_HudPrefs.txt, one line
//! such as "brand=1 money=1 wanted=0 needs=1"); the /hud chat command changes it. All on by default.
class LF_HudPrefs : Managed
{
	static const string FILE = "$profile:LF_HudPrefs.txt";
	static const string PART_BRAND = "brand";
	static const string PART_MONEY = "money";
	static const string PART_WANTED = "wanted";
	static const string PART_NEEDS = "needs";

	protected bool m_bBrand = true;
	protected bool m_bMoney = true;
	protected bool m_bWanted = true;
	protected bool m_bNeeds = true;

	protected static ref LF_HudPrefs s_Instance;

	//------------------------------------------------------------------------------------------------
	//! The local player's prefs, read once from the profile.
	static LF_HudPrefs Get()
	{
		if (s_Instance)
			return s_Instance;

		s_Instance = new LF_HudPrefs();
		if (!FileIO.FileExists(FILE))
			return s_Instance;

		FileHandle file = FileIO.OpenFile(FILE, FileMode.READ);
		if (!file)
		{
			LF_Debug.Warn(LF_HudLogic.FEATURE, "could not read " + FILE + ", every HUD part on");
			return s_Instance;
		}

		string line;
		file.ReadLine(line);
		file.Close();
		s_Instance.FromLine(line);
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void Save()
	{
		FileHandle file = FileIO.OpenFile(FILE, FileMode.WRITE);
		if (!file)
		{
			LF_Debug.Warn(LF_HudLogic.FEATURE, "could not write " + FILE);
			return;
		}

		file.WriteLine(ToLine());
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	static array<string> Parts()
	{
		return {PART_BRAND, PART_MONEY, PART_WANTED, PART_NEEDS};
	}

	//------------------------------------------------------------------------------------------------
	bool IsOn(string part)
	{
		switch (part)
		{
			case PART_BRAND: return m_bBrand;
			case PART_MONEY: return m_bMoney;
			case PART_WANTED: return m_bWanted;
			case PART_NEEDS: return m_bNeeds;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return False for a part name the HUD does not have.
	bool Set(string part, bool on)
	{
		switch (part)
		{
			case PART_BRAND: m_bBrand = on; return true;
			case PART_MONEY: m_bMoney = on; return true;
			case PART_WANTED: m_bWanted = on; return true;
			case PART_NEEDS: m_bNeeds = on; return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! "brand=1 money=1 wanted=0 needs=1"
	string ToLine()
	{
		string line = "";
		foreach (int i, string part : Parts())
		{
			if (i > 0)
				line += " ";

			string flag = "0";
			if (IsOn(part))
				flag = "1";

			line += part + "=" + flag;
		}

		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads ToLine's format; an unknown or missing part keeps its current value.
	void FromLine(string line)
	{
		array<string> pairs = {};
		line.Split(" ", pairs, true);
		foreach (string pair : pairs)
		{
			array<string> kv = {};
			pair.Split("=", kv, false);
			if (kv.Count() == 2)
				Set(kv[0], kv[1] == "1");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! "brand on, money on, wanted off, needs on" for the command reply.
	string Describe()
	{
		string text = "";
		foreach (int i, string part : Parts())
		{
			if (i > 0)
				text += ", ";

			string state = "off";
			if (IsOn(part))
				state = "on";

			text += part + " " + state;
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies "/hud <part|all> <on|off>". Pure apart from the fields it sets.
	//! \return The localization key of the reply; changed is true when a field moved.
	string Apply(string data, out bool changed)
	{
		changed = false;
		string text = data.Trim();
		text.ToLower();
		array<string> words = {};
		text.Split(" ", words, true);
		if (words.Count() != 2)
			return "#LF-Hud_CommandUsage";

		bool on;
		if (words[1] == "on")
			on = true;
		else if (words[1] != "off")
			return "#LF-Hud_CommandUsage";

		array<string> parts = {};
		if (words[0] == "all")
			parts = Parts();
		else if (Parts().Find(words[0]) >= 0)
			parts.Insert(words[0]);
		else
			return "#LF-Hud_CommandUsage";

		foreach (string part : parts)
		{
			if (IsOn(part) != on)
				changed = true;

			Set(part, on);
		}

		return "#LF-Hud_CommandDone";
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Instance = null;
		LF_HudLogic.SetHarnessHold(0);
	}
}

//------------------------------------------------------------------------------------------------
class LF_HudPrefs_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_HudPrefs";
	}

	override void Apply()
	{
		LF_HudPrefs.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The /hud chat command: "/hud money off", "/hud all on". Client only, asks nothing of the server.
[BaseContainerProps(configRoot: true)]
modded class SCR_ChatPanelManager
{
	//------------------------------------------------------------------------------------------------
	override protected void InitDefaultChatCommands()
	{
		super.InitDefaultChatCommands();
		ChatCommandInvoker invoker = GetCommandInvoker("hud");
		if (invoker)
			invoker.Insert(LF_OnHudCommand);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnHudCommand(SCR_ChatPanel panel, string data)
	{
		LF_HudPrefs prefs = LF_HudPrefs.Get();
		bool changed;
		string reply = prefs.Apply(data, changed);
		if (changed)
		{
			prefs.Save();
			LF_Debug.Info(LF_HudLogic.FEATURE, "hud parts now " + prefs.ToLine());
		}

		ShowHelpMessage(WidgetManager.Translate(reply, prefs.Describe()));
	}
}
