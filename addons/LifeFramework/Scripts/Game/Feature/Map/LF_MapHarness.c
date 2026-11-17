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
//!   map status                open flag, pixels per unit, centre and the view width in metres
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
			LF_Debug.Warn(FEATURE, "harness map open: no map gadget carried");
			detail = "not_carried";
			return false;
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
	static bool Pan(float x, float z, float zoom, out string detail)
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
		mapEntity.ZoomPanSmooth(pixelsPerUnit, x, z, PAN_SECONDS);
		LF_Debug.Info(FEATURE, string.Format("harness map pan %1 %2 zoom %3 (%4 px/m)", x, z, zoom, pixelsPerUnit));
		detail = string.Format("panning x=%1 z=%2 zoom=%3 ppu=%4", x, z, zoom, pixelsPerUnit);
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
				ok = LF_MapHarness.Pan(cmd.Arg2.ToFloat(), cmd.Arg3.ToFloat(), cmd.Arg4.ToFloat(), detail);
		}
		else if (cmd.Arg1 == "status")
		{
			ok = true;
			detail = LF_MapHarness.Status();
		}
		else
		{
			detail = "usage: map open|close|pan <x> <z> <zoom>|status";
		}

		if (ok)
			LF_AgentHarness.Answer(cmd.Id, "ok", detail);
		else
			LF_AgentHarness.Answer(cmd.Id, "error", detail);

		return true;
	}
}
