[ComponentEditorProps(category: "LifeFramework/Map", description: "Owner channel for shared map marks (LF_MapShares)")]
class LF_MapShareComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The owner RPC channel of shared map marks, on the player controller (card M9, everon-map.md
//! section 3). The server calls ServerAdd/ServerRemove/ServerClear for this one player; each is an
//! RplRcver.Owner RPC, so only the owning client receives it. Nothing is broadcast and no entity id
//! goes on the wire: a row is its key, a position in metres, icon and colour indexes, a short text and an
//! optional area (a radius in metres and an LF_MapAreaStyle id, radius 0 none).
//! On the client each row becomes a local vanilla static marker the player cannot delete (D-M7: the
//! vanilla networked marker path is not used), and its area, when it has one, a local LF_MapAreas
//! circle (LF_MapShareAreas) that lives and goes with the mark.
class LF_MapShareComponent : ScriptComponent
{
	//! Client: the marker of each shared key, so a remove or a replace finds it.
	protected ref map<string, ref SCR_MapMarkerBase> m_mMarkers = new map<string, ref SCR_MapMarkerBase>();
	//! Client: the keys held, in arrival order (harness and logs read it).
	protected ref array<string> m_aKeys = {};

	//------------------------------------------------------------------------------------------------
	//! Server: the channel on this player's controller; null (logged) when the controller lacks it.
	static LF_MapShareComponent OfPlayer(int playerId)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return null;

		PlayerController controller = players.GetPlayerController(playerId);
		if (!controller)
			return null;

		LF_MapShareComponent channel = LF_MapShareComponent.Cast(controller.FindComponent(LF_MapShareComponent));
		if (!channel)
			LF_Debug.Error(LF_MapShares.FEATURE, string.Format("player %1 controller has no LF_MapShareComponent: shares not sent", playerId));

		return channel;
	}

	//------------------------------------------------------------------------------------------------
	//! The controller this component sits on is the local machine's own (a listen-server host).
	protected bool IsLocalOwner()
	{
		PlayerController controller = PlayerController.Cast(GetOwner());
		return controller && controller == GetGame().GetPlayerController();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: send one row to this player only.
	void ServerAdd(LF_MapShareRow row)
	{
		if (!row || !Replication.IsServer())
			return;

		if (IsLocalOwner())
			RpcDo_MapShareAdd(row.m_sKey, row.m_iX, row.m_iZ, row.m_iIcon, row.m_iColour, row.m_sText, row.m_iAreaRadius, row.m_iAreaStyle);
		else
			Rpc(RpcDo_MapShareAdd, row.m_sKey, row.m_iX, row.m_iZ, row.m_iIcon, row.m_iColour, row.m_sText, row.m_iAreaRadius, row.m_iAreaStyle);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take one row off this player's map.
	void ServerRemove(string key)
	{
		if (!Replication.IsServer())
			return;

		if (IsLocalOwner())
			RpcDo_MapShareRemove(key);
		else
			Rpc(RpcDo_MapShareRemove, key);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: drop every shared mark this player holds (before a full resend on spawn).
	void ServerClear()
	{
		if (!Replication.IsServer())
			return;

		if (IsLocalOwner())
			RpcDo_MapShareClear();
		else
			Rpc(RpcDo_MapShareClear);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the shared keys this player holds.
	int ClientCount()
	{
		return m_aKeys.Count();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_MapShareAdd(string key, int x, int z, int icon, int colour, string text, int areaRadius, int areaStyle)
	{
		DropMarker(key);
		m_aKeys.Insert(key);
		LF_MapShareAreas.Show(key, x, z, areaRadius, areaStyle, text);

		SCR_MapMarkerManagerComponent markers = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markers)
		{
			LF_Debug.Warn(LF_MapShares.FEATURE, "share " + key + " held without a marker: no map marker manager");
			return;
		}

		SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
		marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		marker.SetWorldPos(x, z);
		marker.SetIconEntry(icon);
		marker.SetColorEntry(colour);
		if (text.StartsWith("#"))
			marker.SetCustomText(WidgetManager.Translate(text));
		else
			marker.SetCustomText(text);

		marker.SetCanBeRemovedByOwner(false);
		markers.InsertStaticMarker(marker, true);
		// A local insert makes the local player the owner, and the vanilla edit and drag paths
		// (SCR_MapMarkersUI) only check ownership: they would replace it with a deletable copy this
		// channel no longer tracks. No owner keeps it read-only; a local marker (id -1) still removes.
		marker.SetMarkerOwnerID(-1);
		m_mMarkers.Set(key, marker);
		LF_Debug.Info(LF_MapShares.FEATURE, string.Format("share %1 on the map at %2 %3", key, x, z));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_MapShareRemove(string key)
	{
		DropMarker(key);
		LF_Debug.Info(LF_MapShares.FEATURE, "share " + key + " off the map");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_MapShareClear()
	{
		array<string> keys = {};
		keys.Copy(m_aKeys);
		foreach (string key : keys)
		{
			DropMarker(key);
		}

		LF_Debug.Info(LF_MapShares.FEATURE, string.Format("cleared %1 share(s) for a resend", keys.Count()));
	}

	//------------------------------------------------------------------------------------------------
	protected void DropMarker(string key)
	{
		m_aKeys.RemoveItemOrdered(key);
		LF_MapShareAreas.Hide(key);
		SCR_MapMarkerBase marker = m_mMarkers.Get(key);
		if (!marker)
			return;

		m_mMarkers.Remove(key);
		SCR_MapMarkerManagerComponent markers = SCR_MapMarkerManagerComponent.GetInstance();
		if (markers)
			markers.RemoveStaticMarker(marker);
	}
}
