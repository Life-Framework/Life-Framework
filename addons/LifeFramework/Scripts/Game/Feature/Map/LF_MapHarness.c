//------------------------------------------------------------------------------------------------
//! Session harness for the Everon map (#2228, epic #2223, card M0b). FINAL PROOFS ONLY: iteration is
//! the emulator (`tools\cli map render`) and the LOGIC tests; the `map-shots` proof is the one in-game
//! look a visual card ships with.
//!
//! The verb is client side (the harness poll runs only on a rendering client):
//!   map open                  hold the carried paper map (SCR_MapGadgetComponent), which opens the map
//!   map close                 close it
//!   map pan <x> <z> <zoom>    centre on world (x, z) at the emulator's zoom: the view is
//!                             map width / 2^zoom metres wide, so the in-game and the offline picture
//!                             frame the same ground (`tools\cli map render --zoom <zoom>`)
//!   map pan <x> <z> <zoom> <s> the same over <s> seconds, so a shot catches the canvas mid-pan
//!   map probe                 the places probe (-lfMapPlacesProbe: 400 POIs, rebuild and reposition cost)
//!                             and the mean frame ms since the map opened
//!   map status                open flag, pixels per unit, centre and the view width in metres
//!   map waypoint <x> <z>      what a right-click on the map does: LF_MapWaypoint.SetFromMap (card M7)
//!   map mark <x> <z> [n]      n personal marks (default 1) at (x, z), each through the marker manager's
//!                             InsertStaticMarker as the marker dialog does; answers the count before and
//!                             after, so the 21st refusal (card M8) reads back
//!
//! The harness `input` verb refuses the GadgetMap input, so the gadget is held the way `hold` does, and
//! the pan is SCR_MapEntity.ZoomPanSmooth with a near-zero time. The map takes a few frames to start up
//! after it opens: a pan before then is a no-op, so `map status` is the read-back the proof loops on.
//! The map size comes from the map entity, never from a constant here (core holds no world numbers).
//------------------------------------------------------------------------------------------------
class LF_MapHarness
{
	static const string FEATURE = "Map";
	static const string VERB = "map";

	//! A pan or zoom this short is as good as a jump; the entity refuses a zero time.
	static const float PAN_SECONDS = 0.05; // lf-fixed: harness jump, not an owner knob

	//------------------------------------------------------------------------------------------------
	//! \return The map gadget component of the carried item that has one, or null.
	protected static SCR_MapGadgetComponent FindCarried(IEntity character, out IEntity item)
	{
		InventoryStorageManagerComponent inventory = LF_Component<InventoryStorageManagerComponent>.Find(character);
		if (!inventory)
			return null;

		array<IEntity> items = {};
		inventory.GetItems(items);
		foreach (IEntity candidate : items)
		{
			SCR_MapGadgetComponent gadget = LF_Component<SCR_MapGadgetComponent>.Find(candidate);
			if (gadget)
			{
				item = candidate;
				return gadget;
			}
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Holds the carried paper map so the map opens. \return true and a detail, or false and the reason.
	static bool Open(out string detail)
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
		{
			detail = "already open";
			return true;
		}

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		SCR_GadgetManagerComponent gadgets;
		if (character)
			gadgets = SCR_GadgetManagerComponent.GetGadgetManager(character);

		if (!character || !gadgets)
		{
			detail = "no_character";
			return false;
		}

		// Already in hand with the map closed (a `close` leaves it there): switch the map mode on again.
		SCR_MapGadgetComponent heldMap = SCR_MapGadgetComponent.Cast(gadgets.GetHeldGadgetComponent());
		if (heldMap)
		{
			heldMap.SetMapMode(true);
			LF_Debug.Info(FEATURE, "harness map open (held map)");
			detail = "opening (held map)";
			return true;
		}

		IEntity item;
		if (!FindCarried(character, item))
		{
			// No paper map on this character (the welcome gift comes after the arrival chain): open the
			// menu the gadget opens, which reads the same game mode map config.
			GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
			LF_Debug.Info(FEATURE, "harness map open: no map gadget carried, opened the map menu");
			detail = "opening (menu, no map carried)";
			return true;
		}

		gadgets.SetGadgetMode(item, EGadgetMode.IN_HAND);
		LF_Debug.Info(FEATURE, "harness map open: held " + LF_Utils.GetPrefabName(item));
		detail = "opening " + FilePath.StripExtension(FilePath.StripPath(LF_Utils.GetPrefabName(item)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the map: the held map gadget's own switch, else the menu the gadget would close.
	static bool Close(out string detail)
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.IsOpen())
		{
			detail = "already closed";
			return true;
		}

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		SCR_MapGadgetComponent heldMap;
		if (character)
		{
			SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(character);
			if (gadgets)
				heldMap = SCR_MapGadgetComponent.Cast(gadgets.GetHeldGadgetComponent());
		}

		if (heldMap)
			heldMap.SetMapMode(false);
		else
			GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);

		LF_Debug.Info(FEATURE, "harness map close");
		detail = "closing";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Centres the open map on world (x, z) at the emulator zoom. \return true when the move was asked
	//! for; the entity animates it for PAN_SECONDS, `Status` reads where it landed.
	static bool Pan(float x, float z, float zoom, out string detail, float seconds = PAN_SECONDS)
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.IsOpen())
		{
			detail = "map_not_open";
			return false;
		}

		CanvasWidget canvas = mapEntity.GetMapWidget();
		float width;
		float height;
		if (canvas)
			canvas.GetScreenSize(width, height);

		float sizeX = mapEntity.GetMapSizeX();
		if (width <= 0 || sizeX <= 0)
		{
			detail = "map_not_ready";
			return false;
		}

		float viewMetres = sizeX / Math.Pow(2, zoom);
		float pixelsPerUnit = width / viewMetres;
		if (seconds < PAN_SECONDS)
			seconds = PAN_SECONDS;

		mapEntity.ZoomPanSmooth(pixelsPerUnit, x, z, seconds);
		LF_Debug.Info(FEATURE, string.Format("harness map pan %1 %2 zoom %3 (%4 px/m)", x, z, zoom, pixelsPerUnit));
		detail = string.Format("panning x=%1 z=%2 zoom=%3 ppu=%4", x, z, zoom, pixelsPerUnit);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the waypoint at world (x, z) the way a right-click on the map does. \return true when set.
	static bool Waypoint(float x, float z, out string detail)
	{
		vector world = Vector(x, 0, z);
		BaseWorld baseWorld = GetGame().GetWorld();
		if (baseWorld)
			world[1] = baseWorld.GetSurfaceY(x, z);

		bool ok = LF_MapWaypoint.SetFromMap(world);
		LF_Debug.Info(FEATURE, string.Format("harness map waypoint %1 %2: %3", x, z, ok));
		detail = string.Format("waypoint x=%1 z=%2 set=%3", x, z, ok);
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! Places `count` personal marks at world (x, z), one metre apart, through InsertStaticMarker.
	//! \return false with no marker manager; the detail is `before=N after=M asked=K`.
	static bool Mark(float x, float z, int count, out string detail)
	{
		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!manager)
		{
			detail = "no_marker_manager";
			return false;
		}

		if (count < 1)
			count = 1;

		int before = LF_MapMarksClient.CountLocal(manager);
		ChimeraWorld world = GetGame().GetWorld();
		for (int i = 0; i < count; i++)
		{
			SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
			marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
			marker.SetIconEntry(0);
			marker.SetColorEntry(0);
			marker.SetWorldPos(x + i, z);
			if (world)
				marker.SetTimestamp(world.GetServerTimestamp());

			manager.InsertStaticMarker(marker, true);
		}

		int after = LF_MapMarksClient.CountLocal(manager);
		LF_Debug.Info(FEATURE, string.Format("harness map mark %1 %2 x%3: %4 -> %5", x, z, count, before, after));
		detail = string.Format("before=%1 after=%2 asked=%3", before, after, count);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the places probe on the open map. \return false when the map or the places layer is missing.
	static bool Probe(out string detail)
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.IsOpen())
		{
			detail = "map_not_open";
			return false;
		}

		LF_MapPlacesUI places = LF_MapPlacesUI.Cast(mapEntity.GetMapUIComponent(LF_MapPlacesUI));
		if (!places)
		{
			detail = "no_places_layer";
			return false;
		}

		int frames;
		float frameMs = places.FrameMs(frames);
		detail = string.Format("frame_ms=%1 frames=%2 | %3", frameMs, frames, places.Probe());
		LF_Debug.Info(FEATURE, "harness map probe: " + detail);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One line the proof parses: `open=1 ppu=.. min=.. max=.. cx=.. cz=.. view_m=.. map_m=.. screen=WxH`.
	static string Status()
	{
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity || !mapEntity.IsOpen())
			return "open=0";

		CanvasWidget canvas = mapEntity.GetMapWidget();
		float width;
		float height;
		if (canvas)
			canvas.GetScreenSize(width, height);

		float ppu = mapEntity.GetCurrentZoom();
		float viewMetres;
		if (ppu > 0)
			viewMetres = width / ppu;

		float cx;
		float cz;
		mapEntity.GetMapCenterWorldPosition(cx, cz);
		return string.Format("open=1 ppu=%1 min=%2 max=%3 cx=%4 cz=%5 view_m=%6 map_m=%7 screen=%8x%9", ppu, mapEntity.GetMinZoom(), mapEntity.GetMaxZoom(), cx, cz, viewMetres, mapEntity.GetMapSizeX(), width, height);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != LF_MapHarness.VERB)
			return super.Handle(cmd);

		string detail;
		bool ok;
		if (cmd.Arg1 == "open")
		{
			ok = LF_MapHarness.Open(detail);
		}
		else if (cmd.Arg1 == "close")
		{
			ok = LF_MapHarness.Close(detail);
		}
		else if (cmd.Arg1 == "pan")
		{
			if (cmd.Arg2.IsEmpty() || cmd.Arg3.IsEmpty() || cmd.Arg4.IsEmpty())
				detail = "usage: map pan <x> <z> <zoom>";
			else
				ok = LF_MapHarness.Pan(cmd.Arg2.ToFloat(), cmd.Arg3.ToFloat(), cmd.Arg4.ToFloat(), detail, cmd.Arg5.ToFloat());
		}
		else if (cmd.Arg1 == "probe")
		{
			ok = LF_MapHarness.Probe(detail);
		}
		else if (cmd.Arg1 == "waypoint")
		{
			if (cmd.Arg2.IsEmpty() || cmd.Arg3.IsEmpty())
				detail = "usage: map waypoint <x> <z>";
			else
				ok = LF_MapHarness.Waypoint(cmd.Arg2.ToFloat(), cmd.Arg3.ToFloat(), detail);
		}
		else if (cmd.Arg1 == "mark")
		{
			if (cmd.Arg2.IsEmpty() || cmd.Arg3.IsEmpty())
				detail = "usage: map mark <x> <z> [n]";
			else
				ok = LF_MapHarness.Mark(cmd.Arg2.ToFloat(), cmd.Arg3.ToFloat(), cmd.Arg4.ToInt(), detail);
		}
		else if (cmd.Arg1 == "status")
		{
			ok = true;
			detail = LF_MapHarness.Status();
		}
		else
		{
			detail = "usage: map open|close|pan <x> <z> <zoom> [s]|waypoint <x> <z>|mark <x> <z> [n]|probe|status";
		}

		if (ok)
			LF_AgentHarness.Answer(cmd.Id, "ok", detail);
		else
			LF_AgentHarness.Answer(cmd.Id, "error", detail);

		return true;
	}
}
