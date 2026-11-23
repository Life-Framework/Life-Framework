// LF_LevelUpCardView.c: the level-up card on the local player's HUD (#4404). A small branded card, made
// from the layout at run time like the other HUD readouts, shown a few seconds and removed. It takes no
// input and no focus. A second level-up replaces the card on screen. Client only; a dedicated server
// draws nothing.

class LF_LevelUpCardView
{
	static const ResourceName LAYOUT = "{6B1F8E42A07D3C95}UI/Layouts/HUD/LF_LevelUpCard.layout";
	static const int SHOW_MS = 5000;	// lf-fixed: a short read, the length of a toast

	protected static Widget s_wRoot;
	protected static bool s_bTimer;
	protected static bool s_bWarnedSilent;
	protected static string s_sLast;

	//------------------------------------------------------------------------------------------------
	//! "shown" or "none", for the harness and logs.
	static string State()
	{
		if (s_wRoot)
			return "shown";

		return "none";
	}

	//------------------------------------------------------------------------------------------------
	//! What the last card said, "MINER 7", for the harness.
	static string Last()
	{
		return s_sLast;
	}

	//------------------------------------------------------------------------------------------------
	static void Show(string job, int levelBefore, int level)
	{
		if (!GetGame() || RplSession.Mode() == RplMode.Dedicated)
			return;

		LF_LevelUpCardLines lines = LF_LevelUpCard.Build(job, levelBefore, level);
		if (!lines)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Remove();
		s_wRoot = workspace.CreateWidgets(LAYOUT);
		if (!s_wRoot)
		{
			LF_Debug.Error(LF_LevelUpCard.FEATURE, "the level-up card layout did not load: " + LAYOUT);
			return;
		}

		SetText("Trade", lines.m_sTradeName);
		SetText("Tier", lines.TierWord());
		SetText("Level", lines.LevelLine());
		string opens = lines.OpensLine();
		SetText("Opens", opens);
		Widget opensRow = s_wRoot.FindAnyWidget("Opens");
		if (opensRow)
			opensRow.SetVisible(!opens.IsEmpty());

		s_sLast = string.Format("%1 %2", job, level);
		LF_Debug.Info(LF_LevelUpCard.FEATURE, "level-up card shown: " + s_sLast);
		PlaySting();
		if (GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(Remove, SHOW_MS, false);
			s_bTimer = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	static void Remove()
	{
		if (s_bTimer && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Remove);

		s_bTimer = false;
		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();

		s_wRoot = null;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Remove();
		s_bWarnedSilent = false;
		s_sLast = string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetText(string name, string text)
	{
		TextWidget widget = TextWidget.Cast(s_wRoot.FindAnyWidget(name));
		if (widget)
			widget.SetText(text);
		else
			LF_Debug.Error(LF_LevelUpCard.FEATURE, "level-up card widget missing from the layout: " + name);
	}

	//------------------------------------------------------------------------------------------------
	//! Plays the `level_up` sound slot at the local player. A slot with no sound (the core ships none, the
	//! pack fills it) stays silent and says so once.
	protected static void PlaySting()
	{
		ResourceName project;
		if (!LF_SoundSlots.IsUnfilled(LF_LevelUpCard.SOUND_SLOT))
			project = LF_SoundSlots.Get(LF_LevelUpCard.SOUND_SLOT);

		IEntity character = LF_Utils.GetLocalCharacter();
		if (project.IsEmpty() || !character)
		{
			if (!s_bWarnedSilent)
				LF_Debug.Warn(LF_LevelUpCard.FEATURE, "no sound for the level_up slot yet: the level-up card is silent");

			s_bWarnedSilent = true;
			return;
		}

		vector mat[4];
		character.GetWorldTransform(mat);
		AudioSystem.PlayEvent(project, "SOUND_LEVEL_UP", mat);
		LF_Debug.Info("Sound", string.Format("slot %1 played: %2", LF_LevelUpCard.SOUND_SLOT, project));
	}
}

//------------------------------------------------------------------------------------------------
class LF_LevelUpCardView_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LevelUpCardView";
	}

	override void Apply()
	{
		LF_LevelUpCardView.ResetStatics();
	}
}
