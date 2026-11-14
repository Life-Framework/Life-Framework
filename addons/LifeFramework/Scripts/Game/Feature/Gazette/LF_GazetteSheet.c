// LF_GazetteSheet.c - the printed sheet of the town paper (#792). Client only.
// Built from UI kit parts (LF_UIScreen), so it takes the 1989 theme from LF_UITheme: no new
// .layout, no colours or fonts here (docs/style-guide.md "Screens"). Every text is a story key
// rendered on this machine, so the reader's language and the server's terms apply.

class LF_GazetteSheet
{
	static const float READ_DISTANCE = 4; // lf-fixed: the reach of a wall stand; the placed action site is the real gate
	static const string READ_ACTION = "LF_GazetteReadAction";

	protected static ref LF_UIScreen s_Screen;
	protected static EntityID s_PendingStand;

	//------------------------------------------------------------------------------------------------
	//! Client: the parts of the sheet (no UI), so a test reads what a player would see.
	//! \param packed The edition's rows as LF_GazetteRow.Pack made them; number 0 means no edition yet.
	static array<ref LF_UIPart> BuildParts(string paperKey, int number, array<string> packed)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", LF_Terms.Text(paperKey)));
		if (number <= 0 || !packed || packed.IsEmpty())
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Empty", LF_Terms.Text("#LF-Gazette_Empty")));
		}
		else
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.HEADING, "Edition", LF_Terms.Text("#LF-Gazette_Edition", number.ToString())));
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.RULE, "Rule", string.Empty));
			foreach (int index, string entry : packed)
			{
				LF_GazetteRow row = LF_GazetteRow.Unpack(entry);
				if (!row)
				{
					LF_Debug.Error(LF_Gazette.FEATURE, string.Format("gazette row %1 dropped: it does not unpack", index));
					continue;
				}

				parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, string.Format("Story%1", index), LF_Gazette.RenderRow(row)));
			}
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-Sign_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	static void SetPendingStand(EntityID stand)
	{
		s_PendingStand = stand;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server's answer arrived; open the sheet.
	static LF_UIScreen Open(string paperKey, int number, array<string> packed)
	{
		Hide();
		s_Screen = LF_UIScreen.Open(BuildParts(paperKey, number, packed));
		if (!s_Screen)
		{
			LF_Debug.Error(LF_Gazette.FEATURE, "gazette sheet failed to open");
			return null;
		}

		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		if (s_PendingStand != EntityID.INVALID)
			s_Screen.CloseBeyond(READ_DISTANCE, s_PendingStand);

		s_PendingStand = EntityID.INVALID;
		LF_Debug.Info(LF_Gazette.FEATURE, string.Format("gazette sheet open: edition %1", number));
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Screen)
			return;

		s_Screen.Close();
		s_Screen = null;
	}
}

//------------------------------------------------------------------------------------------------
//! The newsstand's read action (#792): the reader takes the paper in hand and the server answers
//! with the latest edition. Put it on any prop; the server checks the reader stands at one.
class LF_GazetteReadAction : ScriptedUserAction
{
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Gazette_Action";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Gazette", "LF_GazetteReadAction"))
			return;

		if (!pOwnerEntity || !pUserEntity)
			return;

		// Server-driven use (the agent harness): answer that reader directly.
		if (pUserEntity != LF_Utils.GetLocalCharacter())
		{
			if (!Replication.IsServer())
				return;

			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
			LF_Gazette.ServerRead(SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId)));
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_Gazette.FEATURE, "gazette read not sent: no SCR_PlayerController");
			return;
		}

		LF_GazetteSheet.SetPendingStand(pOwnerEntity.GetID());
		controller.LF_AskGazette();
	}
}

//------------------------------------------------------------------------------------------------
//! Gazette bridge on the player controller (owned by the reader from join).
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for the latest edition.
	void LF_AskGazette()
	{
		if (Replication.IsServer())
			RpcAsk_LF_Gazette();
		else
			Rpc(RpcAsk_LF_Gazette);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: send this player the edition.
	void LF_ShowGazette(string paperKey, int number, array<string> rows)
	{
		if (LF_IsLocalController())
			RpcDo_LF_ShowGazette(paperKey, number, rows);
		else
			Rpc(RpcDo_LF_ShowGazette, paperKey, number, rows);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_Gazette()
	{
		LF_Gazette.ServerRead(this);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowGazette(string paperKey, int number, array<string> rows)
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error(LF_Gazette.FEATURE, "gazette sheet not opened: no workspace on this machine");
			return;
		}

		LF_GazetteSheet.Open(paperKey, number, rows);
	}
}
