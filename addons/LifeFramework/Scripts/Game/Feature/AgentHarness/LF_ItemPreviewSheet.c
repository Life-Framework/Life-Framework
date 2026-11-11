// LF_ItemPreviewSheet.c - a debug sheet that draws prefabs through the same ItemPreviewWidget
// the inventory slots use, so `tools\cli shots previews` can check every item's inventory
// preview (CameraDistanceToItem, angle, FOV) on one screenshot. Client-only; opened only by the
// agent harness `previews` verb.

class LF_ItemPreviewSheet
{
	protected static const ResourceName SHEET_LAYOUT = "{7A3C1D5E9B2F4A00}UI/Layouts/Debug/LF_ItemPreviewSheet.layout";
	protected static const ResourceName CELL_LAYOUT = "{7A3C1D5E9B2F4B00}UI/Layouts/Debug/LF_ItemPreviewCell.layout";
	protected static const int COLUMNS = 6;
	protected static const int ROWS = 4;
	//! A UI screenshot shows only the top-left 95% of the workspace, so the grid fills that part.
	protected static const float GRID = 0.95;

	protected static Widget s_wRoot;
	protected static bool s_bHidHud;

	//------------------------------------------------------------------------------------------------
	//! Draws up to COLUMNS * ROWS prefabs, one cell each with the prefab's file name under it.
	//! \return the number of cells drawn, or a negative value when the sheet could not open.
	static int Show(array<string> prefabs)
	{
		Hide();
		// The HUD (quest toast, notifications) would cover cells; it comes back on Hide.
		SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
		if (hud && hud.IsVisible())
		{
			hud.SetVisible(false);
			s_bHidHud = true;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!workspace || !world)
			return -1;

		ItemPreviewManagerEntity manager = world.GetItemPreviewManager();
		if (!manager)
		{
			LF_Debug.Error("AgentHarness", "previews: no ItemPreviewManagerEntity");
			return -2;
		}

		// Notification toasts (the quest hint) sit above everything and would cover cells.
		Widget toast = workspace.FindAnyWidget(LF_NotificationToastUI.ROOT_NAME);
		if (toast && toast.GetParent())
		{
			Widget sibling = toast.GetParent().GetChildren();
			while (sibling)
			{
				if (sibling.GetName() == LF_NotificationToastUI.ROOT_NAME)
					sibling.SetVisible(false);
				sibling = sibling.GetSibling();
			}
		}

		s_wRoot = workspace.CreateWidgets(SHEET_LAYOUT);
		if (!s_wRoot)
		{
			LF_Debug.Error("AgentHarness", "previews: failed to load " + SHEET_LAYOUT);
			return -3;
		}

		Widget cells = s_wRoot.FindAnyWidget("Cells");
		int shown = 0;
		foreach (string prefab : prefabs)
		{
			if (shown >= COLUMNS * ROWS)
				break;

			Widget cell = workspace.CreateWidgets(CELL_LAYOUT, cells);
			if (!cell)
				continue;

			int col = shown % COLUMNS;
			int row = shown / COLUMNS;
			FrameSlot.SetAnchorMin(cell, GRID * col / COLUMNS, GRID * row / ROWS);
			FrameSlot.SetAnchorMax(cell, GRID * (col + 1) / COLUMNS, GRID * (row + 1) / ROWS);
			FrameSlot.SetOffsets(cell, 0, 0, 0, 0);

			// The preview renders into a square: size it by the cell's shorter side, centred above the label.
			float side = workspace.DPIUnscale(Math.Min(workspace.GetWidth() * GRID / COLUMNS, workspace.GetHeight() * GRID * 0.86 / ROWS));
			Widget box = cell.FindAnyWidget("Preview");
			if (box)
			{
				FrameSlot.SetAnchorMin(box, 0.5, 0.43);
				FrameSlot.SetAnchorMax(box, 0.5, 0.43);
				FrameSlot.SetAlignment(box, 0.5, 0.5);
				FrameSlot.SetPos(box, 0, 0);
				FrameSlot.SetSize(box, side, side);
			}

			ItemPreviewWidget preview = ItemPreviewWidget.Cast(cell.FindAnyWidget("Preview"));
			TextWidget label = TextWidget.Cast(cell.FindAnyWidget("Label"));
			if (label)
				label.SetText(BaseName(prefab));
			if (preview)
				manager.SetPreviewItemFromPrefab(preview, prefab, null, true);
			shown++;
		}

		LF_Debug.Info("AgentHarness", "previews: " + shown + " cells");
		return shown;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();
		s_wRoot = null;
		if (s_bHidHud)
		{
			SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
			if (hud)
				hud.SetVisible(true);
			s_bHidHud = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static string BaseName(string prefab)
	{
		int slash = prefab.LastIndexOf("/");
		if (slash >= 0)
			prefab = prefab.Substring(slash + 1, prefab.Length() - slash - 1);
		int dot = prefab.LastIndexOf(".");
		if (dot > 0)
			prefab = prefab.Substring(0, dot);
		return prefab;
	}
}
