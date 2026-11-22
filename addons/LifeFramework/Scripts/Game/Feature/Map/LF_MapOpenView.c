// LF_MapOpenView.c: how the map opens and what it leaves behind (first-2-hours review 2026-11-22, rows C1, C2, C7).
// - C1: the map opened at whole-island zoom, wherever the player stood. It now opens centred on the player at
//   town zoom (a view OPEN_ZOOM levels in, about 570 m wide), every time.
// - C2: the engine draws the settlement names under every place layer, so a cluster stamp covered "Saint-Philippe"
//   and a badge covered "Meaux". The names are now drawn here, in a layer above the places and lifted clear of
//   the stamps. The vanilla names go off when this layer found names to draw (never both, never neither).
// - C7: the folded paper map stayed in the player's hand after the map screen closed, covering a third of the
//   view. It is put away shortly after the map closes.

//------------------------------------------------------------------------------------------------
//! Puts the paper map back in the pocket. A static method so a delayed call needs no object that the map's
//! close has already freed.
class LF_MapPutAway
{
	static const int DELAY_MS = 350;	// lf-fixed: after the close animation, so the hand-off and the unfocus do not race

	//------------------------------------------------------------------------------------------------
	//! Queues the put-away of the local player's held map.
	static void Schedule()
	{
		GetGame().GetCallqueue().CallLater(Run, DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when a held map was put away.
	static bool Run()
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
			return false;

		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
			return false;

		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(player);
		if (!gadgets)
			return false;

		SCR_GadgetComponent held = gadgets.GetHeldGadgetComponent();
		if (!held || held.GetType() != EGadgetType.MAP)
			return false;

		gadgets.RemoveHeldGadget();
		LF_Debug.Info(LF_MapPlacesUI.FEATURE, "map closed: the paper map is put away");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_MapPlacesUI
{
	//! The zoom the map opens at (LF_MapClusterRules zoom: the view is 12800 m / 2^zoom wide): the whole town.
	protected static const float OPEN_ZOOM = 4.5;	// lf-fixed: past the solo zoom (4), so no stamp stands in the town
	protected static const int OPEN_AFTER_FRAMES = 3;	// lf-fixed: the map refuses a pan until it has run a frame or two
	protected static const float OPEN_PAN_SECONDS = 0.05;	// lf-fixed: as good as a jump; the entity refuses zero
	protected static const float NAME_LIFT = 34;	// lf-fixed: reference units a settlement name sits above its mark's centre
	protected static const float NAME_FONT = 22;	// lf-fixed: a place name reads before a badge does
	protected static const int NAME_MAX = 120;	// lf-fixed: the most settlement names drawn

	protected bool m_bOpenPending;
	protected int m_iOpenFrames;
	protected Widget m_wNameLayer;
	protected ref array<TextWidget> m_aNameWidgets = {};
	protected ref array<vector> m_aNamePositions = {};
	protected ref array<float> m_aNameWidths = {};

	//------------------------------------------------------------------------------------------------
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);
		m_bOpenPending = true;
		m_iOpenFrames = 0;
		BuildNames();
	}

	//------------------------------------------------------------------------------------------------
	//! A harness pan or zoom owns the view: the open view must not undo it.
	void CancelOpenView()
	{
		m_bOpenPending = false;
	}

	//------------------------------------------------------------------------------------------------
	override void Update(float timeSlice)
	{
		super.Update(timeSlice);
		if (!m_bOpenPending)
			return;

		m_iOpenFrames++;
		if (m_iOpenFrames < OPEN_AFTER_FRAMES)
			return;

		m_bOpenPending = false;
		CentreOnPlayer();
	}

	//------------------------------------------------------------------------------------------------
	//! Jumps the view to the player at OPEN_ZOOM. A player with no body leaves the view as the engine set it.
	protected void CentreOnPlayer()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!player || !workspace || !m_MapEntity)
		{
			LF_Debug.Warn(FEATURE, "map opened with no player body: the view stays where it is");
			return;
		}

		vector at = player.GetOrigin();
		float viewWidth = LF_MapClusterRules.WORLD_M / Math.Pow(2, OPEN_ZOOM);
		m_MapEntity.ZoomPanSmooth(workspace.GetWidth() / viewWidth, at[0], at[2], OPEN_PAN_SECONDS);
		LF_Debug.Info(FEATURE, string.Format("map opens on the player at zoom %1 (%2 m wide)", OPEN_ZOOM, viewWidth));
	}

	//------------------------------------------------------------------------------------------------
	//! One label per settlement the terrain names, in a layer above the places and the card.
	protected void BuildNames()
	{
		ClearNames();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!m_wLayer || !workspace || !m_RootWidget || !m_MapEntity)
			return;

		array<int> types = {
			EMapDescriptorType.MDT_NAME_GENERIC,
			EMapDescriptorType.MDT_NAME_CITY,
			EMapDescriptorType.MDT_NAME_VILLAGE,
			EMapDescriptorType.MDT_NAME_TOWN,
			EMapDescriptorType.MDT_NAME_SETTLEMENT,
			EMapDescriptorType.MDT_NAME_LOCAL
		};
		array<string> seen = {};
		m_wNameLayer = workspace.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.NOFOCUS | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, m_RootWidget);
		if (!m_wNameLayer)
		{
			LF_Debug.Error(FEATURE, "the settlement name layer was not created; the vanilla names stay");
			return;
		}

		FrameSlot.SetAnchorMin(m_wNameLayer, 0, 0);
		FrameSlot.SetAnchorMax(m_wNameLayer, 1, 1);
		FrameSlot.SetOffsets(m_wNameLayer, 0, 0, 0, 0);
		// Above the places layer and under the card.
		m_wNameLayer.SetZOrder(1);
		if (m_wCard)
			m_wCard.SetZOrder(2);

		foreach (int type : types)
		{
			// The terrain's generic names (harbor, factory, farm, landfill) are lower-case labels on every shed: not drawn.
			if (type == EMapDescriptorType.MDT_NAME_GENERIC)
				continue;

			array<MapItem> items = {};
			m_MapEntity.GetByType(items, type);
			foreach (MapItem item : items)
			{
				if (m_aNameWidgets.Count() >= NAME_MAX)
					break;

				IEntity entity = item.Entity();
				if (!entity)
					continue;

				string name = WidgetManager.Translate(item.GetDisplayName());
				if (name.IsEmpty())
					continue;

				vector at = entity.GetOrigin();
				string key = string.Format("%1|%2|%3", name, Math.Round(at[0]), Math.Round(at[2]));
				if (seen.Find(key) >= 0)
					continue;

				seen.Insert(key);
				TextWidget label = TextWidget.Cast(workspace.CreateWidgets(TYPE_LAYOUT, m_wNameLayer));
				if (!label)
					continue;

				label.SetFlags(WidgetFlags.IGNORE_CURSOR);
				label.SetText(name);
				label.SetExactFontSize(NAME_FONT);
				float width = name.Length() * NAME_FONT * LF_MapDeclutter.CHAR_ADVANCE + LF_MapDeclutter.LABEL_PAD * 2;
				FrameSlot.SetAlignment(label, 0.5, 1);
				FrameSlot.SetSize(label, width, NAME_FONT + 10);
				label.SetVisible(false);
				m_aNameWidgets.Insert(label);
				m_aNamePositions.Insert(at);
				m_aNameWidths.Insert(width);
			}
		}

		if (m_aNameWidgets.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "no settlement names found on the terrain; the vanilla names stay");
			return;
		}

		// Ours replace the engine's at every zoom.
		int layers = m_MapEntity.LayerCount();
		foreach (int type : types)
		{
			for (int layerId = 0; layerId < layers; layerId++)
			{
				MapLayer layer = m_MapEntity.GetLayer(layerId);
				if (!layer)
					continue;

				MapDescriptorProps props = layer.GetPropsFor(EFactionMapID.UNKNOWN, type);
				if (props)
					props.SetVisible(false);
			}
		}

		LF_Debug.Info(FEATURE, string.Format("%1 settlement name(s) drawn above the places", m_aNameWidgets.Count()));
		RepositionNames();
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearNames()
	{
		if (m_wNameLayer)
			m_wNameLayer.RemoveFromHierarchy();

		m_wNameLayer = null;
		m_aNameWidgets.Clear();
		m_aNamePositions.Clear();
		m_aNameWidths.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Names show where the badges have folded into stamps (layer 2 and wider, as the vanilla names did): up
	//! close each badge is its own label on hover and a town name would only sit on top of them.
	protected void RepositionNames()
	{
		if (!m_wNameLayer || !m_MapEntity)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		float width = workspace.DPIUnscale(workspace.GetWidth());
		float height = workspace.DPIUnscale(workspace.GetHeight());
		bool wide = m_MapEntity.GetLayerIndex() >= LF_MapDescriptors.NAMES_FROM_LAYER;
		foreach (int i, TextWidget label : m_aNameWidgets)
		{
			if (!wide)
			{
				label.SetVisible(false);
				continue;
			}

			int sx;
			int sy;
			vector at = m_aNamePositions[i];
			m_MapEntity.WorldToScreen(at[0], at[2], sx, sy, true);
			float x = workspace.DPIUnscale(sx);
			float y = workspace.DPIUnscale(sy) - NAME_LIFT;
			bool onScreen = x > -100 && y > -40 && x < width + 100 && y < height + 40;
			label.SetVisible(onScreen);
			if (onScreen)
				FrameSlot.SetPos(label, x, y);
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected void Reposition()
	{
		super.Reposition();
		RepositionNames();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		m_bOpenPending = false;
		ClearNames();
		super.OnMapClose(config);
		LF_MapPutAway.Schedule();
	}
}
