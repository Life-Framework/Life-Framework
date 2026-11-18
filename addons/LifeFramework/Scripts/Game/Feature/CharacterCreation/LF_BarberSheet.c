// LF_BarberSheet.c — the barber's sheet (#3234), on the UI kit. Client-only: "previous" and
// "next" try each look on the seated character (a local preview), "pay" asks the server for the
// haircut, and closing without paying puts the old head back. The server answers with the result
// (RpcDo_LF_BarberResult); a refusal is written on the sheet.

class LF_BarberSheet
{
	static const string PART_LOOK = "BarberLook";
	static const string PART_STATUS = "BarberStatus";
	static const string PART_PREV = "BarberPrev";
	static const string PART_NEXT = "BarberNext";
	static const string PART_PAY = "BarberPay";
	static const string PART_CLOSE = "BarberClose";
	//! The sheet closes when the sitter is this far from the chair.
	static const float CLOSE_DISTANCE = 3.0;	// lf-fixed: a seated body is within a metre of its chair; walking away closes the sheet

	protected static ref LF_BarberSheet s_Sheet;

	protected ref LF_UIScreen m_Screen;
	protected EntityID m_ChairId = EntityID.INVALID;
	protected EntityID m_CharacterId = EntityID.INVALID;
	protected ResourceName m_sOriginalHead;
	protected int m_iLook;
	protected int m_iFee;
	protected bool m_bPaid;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts(int fee)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "BarberTitle", "#LF-Barber_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "BarberIntro", "#LF-Barber_Intro"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_LOOK, ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_STATUS, ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_PREV, "#LF-Barber_Prev"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_NEXT, "#LF-Barber_Next"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_PAY, WidgetManager.Translate("#LF-Barber_Pay", fee)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, PART_CLOSE, "#LF-Barber_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! The look after `step` from `index`, wrapping around `count`.
	static int Step(int index, int step, int count)
	{
		if (count <= 0)
			return 0;

		int next = (index + step) % count;
		if (next < 0)
			next += count;

		return next;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open(IEntity chair, IEntity character, int fee)
	{
		Hide();
		if (!chair || !character)
		{
			LF_Debug.Error(LF_Barber.FEATURE, "barber sheet not opened: no chair or character");
			return null;
		}

		LF_BarberSheet sheet = new LF_BarberSheet();
		sheet.m_ChairId = chair.GetID();
		sheet.m_CharacterId = character.GetID();
		sheet.m_sOriginalHead = LF_Barber.CurrentHead(character);
		sheet.m_iFee = fee;
		sheet.m_iLook = LF_Barber.IndexOf(sheet.m_sOriginalHead);
		if (sheet.m_iLook < 0)
			sheet.m_iLook = 0;

		sheet.m_Screen = LF_UIScreen.Open(BuildParts(fee));
		if (!sheet.m_Screen)
			return null;

		sheet.m_Screen.GetOnButton().Insert(sheet.OnButton);
		sheet.m_Screen.GetOnClosed().Insert(sheet.OnClosed);
		sheet.m_Screen.CloseOnDeath();
		sheet.m_Screen.CloseBeyond(CLOSE_DISTANCE, chair.GetID());
		s_Sheet = sheet;
		sheet.ShowLook();
		LF_Debug.Info(LF_Barber.FEATURE, string.Format("barber sheet open, fee %1, look %2", fee, sheet.m_iLook));
		return sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		if (!s_Sheet)
			return null;

		return s_Sheet.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Sheet)
			return;

		LF_BarberSheet sheet = s_Sheet;
		s_Sheet = null;
		sheet.PutBackPreview();
		if (sheet.m_Screen)
			sheet.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Client, from the server's answer.
	static void OnResult(int refusal)
	{
		if (!s_Sheet)
			return;

		s_Sheet.ShowResult(refusal);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Hide();
		s_Sheet = null;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity Character()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(m_CharacterId);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowLook()
	{
		if (!m_Screen)
			return;

		int count = LF_Barber.Looks().Count();
		m_Screen.SetLine(PART_LOOK, WidgetManager.Translate("#LF-Barber_Look", m_iLook + 1, count));
		LF_Barber.ApplyHead(Character(), LF_Barber.LookAt(m_iLook));
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowResult(int refusal)
	{
		if (refusal == LF_EBarberRefusal.NONE)
		{
			m_bPaid = true;
			m_sOriginalHead = LF_Barber.LookAt(m_iLook);
			if (m_Screen)
				m_Screen.SetLine(PART_STATUS, "#LF-Barber_Done");

			return;
		}

		if (m_Screen)
			m_Screen.SetLine(PART_STATUS, LF_Barber.RefusalKey(refusal));
	}

	//------------------------------------------------------------------------------------------------
	//! Leaving without paying for the shown look: the head goes back to the one the sitter came in with.
	protected void PutBackPreview()
	{
		IEntity character = Character();
		if (!character || m_sOriginalHead.IsEmpty())
			return;

		if (LF_Barber.CurrentHead(character) != m_sOriginalHead)
			LF_Barber.ApplyHead(character, m_sOriginalHead);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (!m_Screen)
			return;

		int count = LF_Barber.Looks().Count();
		if (name == PART_PREV)
		{
			m_iLook = Step(m_iLook, -1, count);
			ShowLook();
			return;
		}

		if (name == PART_NEXT)
		{
			m_iLook = Step(m_iLook, 1, count);
			ShowLook();
			return;
		}

		if (name != PART_PAY)
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		IEntity chair = GetGame().GetWorld().FindEntityByID(m_ChairId);
		RplComponent chairRpl;
		if (chair)
			chairRpl = RplComponent.Cast(chair.FindComponent(RplComponent));

		if (!controller || !chairRpl)
		{
			LF_Debug.Error(LF_Barber.FEATURE, "haircut not asked: no player controller or the chair does not replicate");
			return;
		}

		m_Screen.SetLine(PART_STATUS, "#LF-Barber_Asking");
		controller.LF_AskBarberCut(chairRpl.Id(), m_iLook);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosed()
	{
		if (s_Sheet != this)
			return;

		s_Sheet = null;
		PutBackPreview();
		LF_Debug.Info(LF_Barber.FEATURE, string.Format("barber sheet closed, paid %1", m_bPaid));
	}
}

//------------------------------------------------------------------------------------------------
//! The haircut request and its answer, on the player's own controller.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: ask the server to cut this player's hair to a look on a chair.
	void LF_AskBarberCut(RplId chairId, int lookIndex)
	{
		if (Replication.IsServer())
			RpcAsk_LF_BarberCut(chairId, lookIndex);
		else
			Rpc(RpcAsk_LF_BarberCut, chairId, lookIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player must be seated on a barber's chair; the fee is that chair's.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_BarberCut(RplId chairId, int lookIndex)
	{
		IEntity character = GetControlledEntity();
		RplComponent chairRpl = RplComponent.Cast(Replication.FindItem(chairId));
		IEntity chair;
		if (chairRpl)
			chair = chairRpl.GetEntity();

		LF_BarberAction barber = LF_BarberAction.Of(chair);
		LF_EBarberRefusal refusal;
		if (!barber || !LF_BarberAction.IsSeatedOn(character, chair))
		{
			LF_Debug.Warn(LF_Barber.FEATURE, string.Format("haircut refused for player %1: not seated on a barber's chair", GetPlayerId()));
			refusal = LF_EBarberRefusal.NO_SEAT;
		}
		else
		{
			refusal = LF_Barber.Cut(character, LF_Utils.GetPlayerUID(GetPlayerId()), lookIndex, barber.GetFee());
		}

		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_BarberResult(refusal);
		else
			Rpc(RpcDo_LF_BarberResult, refusal);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_BarberResult(int refusal)
	{
		LF_BarberSheet.OnResult(refusal);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BarberSheet's statics (LF_StaticReset finds it by reflection).
class LF_BarberSheet_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BarberSheet";
	}

	override void Apply()
	{
		LF_BarberSheet.ResetStatics();
	}
}
