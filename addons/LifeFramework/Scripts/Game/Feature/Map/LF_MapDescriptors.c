// LF_MapDescriptors.c: which vanilla map symbols the open map keeps (map clean-up, 2026-11-22: "make sure the map
// is super clean and easy to use"). The terrain's own descriptors drew black crosses, fuel pumps, bus stops and a
// giant town name over our badges. Each badge already says what a place is, so the vanilla point symbols that
// repeat it in black are hidden, and the settlement names show only once the view is wide enough that the badges
// fold into stamps (layer 2 and wider: under 0.5 px per metre). Terrain symbols that help a player find their
// way (hills, lighthouses, transmitters, ruins, water names) stay; towers, view towers and bunkers were black drops
// all over the town edges and go too.
//
// The engine builds each layer's descriptor props when the map opens (SCR_MapEntity.InitLayers, before the UI
// components' OnMapOpen), so Apply runs on every open from LF_MapPlacesUI.OnMapOpen.

//------------------------------------------------------------------------------------------------
//! The rules, with no map, so a LOGIC test covers them (LF_Test_MapDescriptors).
class LF_MapDescriptors
{
	static const string FEATURE = "MapDescriptors";
	//! Settlement names show from this layer index up (layer 0 is the closest zoom; 2 is from 0.5 px per metre
	//! out, MapLayersDefault.conf), where the town's badges have folded into stamps.
	static const int NAMES_FROM_LAYER = 2;	// lf-fixed: the vanilla layer table
	//! Churches show from this layer up: in town the Church badge stands for them.
	static const int CHURCH_FROM_LAYER = 2;	// lf-fixed: the vanilla layer table

	//------------------------------------------------------------------------------------------------
	//! Point symbols a badge stands for, or town clutter that reads as stray black marks: hidden at every zoom.
	static bool IsNoise(int type)
	{
		switch (type)
		{
			case EMapDescriptorType.MDT_CALVARY:
			case EMapDescriptorType.MDT_CHAPEL:
			case EMapDescriptorType.MDT_CROSS:
			case EMapDescriptorType.MDT_FOUNTAIN:
			case EMapDescriptorType.MDT_SPRING:
			case EMapDescriptorType.MDT_WATERTOWER:
			case EMapDescriptorType.MDT_FUELSTATION:
			case EMapDescriptorType.MDT_HOSPITAL:
			case EMapDescriptorType.MDT_BUSSTOP:
			case EMapDescriptorType.MDT_BUSSTATION:
			case EMapDescriptorType.MDT_CRANE:
			case EMapDescriptorType.MDT_TRANSFORMER:
			case EMapDescriptorType.MDT_STACK:
			case EMapDescriptorType.MDT_TOURISM:
			case EMapDescriptorType.MDT_TOURISTSHELTER:
			case EMapDescriptorType.MDT_TOURISTSIGN:
			case EMapDescriptorType.MDT_MONUMENT:
			case EMapDescriptorType.MDT_WATERPUMP:
			case EMapDescriptorType.MDT_POLICE:
			case EMapDescriptorType.MDT_STORE:
			case EMapDescriptorType.MDT_HOTEL:
			case EMapDescriptorType.MDT_PUB:
			case EMapDescriptorType.MDT_FIREDEP:
			case EMapDescriptorType.MDT_PARKING:
			case EMapDescriptorType.MDT_CONSTRUCTION_SITE:
			case EMapDescriptorType.MDT_BUOY:
			case EMapDescriptorType.MDT_LIGHT:
			case EMapDescriptorType.MDT_TOWER:
			case EMapDescriptorType.MDT_VIEWTOWER:
			case EMapDescriptorType.MDT_BUNKER:
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A settlement or local place name: the big type that sat over the town's badges.
	static bool IsSettlementName(int type)
	{
		switch (type)
		{
			case EMapDescriptorType.MDT_NAME_GENERIC:
			case EMapDescriptorType.MDT_NAME_CITY:
			case EMapDescriptorType.MDT_NAME_VILLAGE:
			case EMapDescriptorType.MDT_NAME_TOWN:
			case EMapDescriptorType.MDT_NAME_SETTLEMENT:
			case EMapDescriptorType.MDT_NAME_LOCAL:
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the open map hides descriptors of `type` on layer `layerId` (0 = the closest zoom).
	static bool HiddenAt(int type, int layerId)
	{
		if (IsNoise(type))
			return true;

		if (IsSettlementName(type))
			return layerId < NAMES_FROM_LAYER;

		if (type == EMapDescriptorType.MDT_CHURCH)
			return layerId < CHURCH_FROM_LAYER;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Hides what HiddenAt says on every layer of the open map. Leaves the rest as vanilla set it.
	//! \return the number of props switched off.
	static int Apply(SCR_MapEntity mapEntity)
	{
		if (!mapEntity)
		{
			LF_Debug.Error(FEATURE, "no map entity; the vanilla symbols stay as they are");
			return 0;
		}

		int hidden;
		int layers = mapEntity.LayerCount();
		for (int layerId = 0; layerId < layers; layerId++)
		{
			MapLayer layer = mapEntity.GetLayer(layerId);
			if (!layer)
				continue;

			for (int type = 0; type < EMapDescriptorType.MDT_COUNT; type++)
			{
				if (!HiddenAt(type, layerId))
					continue;

				MapDescriptorProps props = layer.GetPropsFor(EFactionMapID.UNKNOWN, type);
				if (!props)
					continue;

				props.SetVisible(false);
				hidden++;
			}
		}

		LF_Debug.Info(FEATURE, string.Format("hid %1 vanilla symbol style(s) over %2 layer(s)", hidden, layers));
		return hidden;
	}

	//------------------------------------------------------------------------------------------------
	//! For the harness: every descriptor type with items on the map, its item count and whether its props are
	//! visible on the current layer, as "type:count:shown" words. Finds which vanilla symbol is which.
	static string Census(SCR_MapEntity mapEntity)
	{
		if (!mapEntity || !mapEntity.IsOpen())
			return "map_not_open";

		int layerId = mapEntity.GetLayerIndex();
		MapLayer layer = mapEntity.GetLayer(layerId);
		string line = string.Format("layer=%1", layerId);
		array<MapItem> items = {};
		for (int type = 0; type < EMapDescriptorType.MDT_COUNT; type++)
		{
			items.Clear();
			int count = mapEntity.GetByType(items, type);
			if (count <= 0)
				continue;

			string shown = "?";
			if (layer)
			{
				MapDescriptorProps props = layer.GetPropsFor(EFactionMapID.UNKNOWN, type);
				if (props && props.IsVisible())
					shown = "1";
				else if (props)
					shown = "0";
			}

			line += string.Format(" %1:%2:%3", typename.EnumToString(EMapDescriptorType, type), count, shown);
		}

		return line;
	}
}
