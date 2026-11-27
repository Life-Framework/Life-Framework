// LF_MapPrivatePlaces.c: the server decides which private places a player's map may draw (#2492, map review
// 2026-11-17 finding 5; everon-map.md D-M5). A POI kind whose row is not PUBLIC (DISCOVER, ROLE, HIDDEN) is
// server-only: a client's LF_Places never holds it, so a modified client has neither its id nor its position.
// The server sends each player, to that player's own controller only, the private places the visibility rule
// gives them (a DISCOVER place they heard of or visited, a ROLE place while on duty, a HIDDEN place they met),
// and sends the set again when what they know or their duty changes. The join sync carries it, so a late
// joiner gets only their own set. Nothing private is ever broadcast.

//------------------------------------------------------------------------------------------------
class LF_MapPrivatePlaces
{
	static const string FEATURE = "MapPrivate";
	//! Bound on one player's set, so a pack with a huge private table cannot build an unbounded RPC.
	static const int MAX_PLACES = 512; // lf-fixed: payload sanity bound, well above the 400-POI design size
	//! Positions travel as whole decimetres in an int array (an int array is the proven RPC shape).
	static const float POS_SCALE = 10; // lf-fixed: wire encoding, 0.1 m is finer than a map icon

	//! Client: the private places the server says this player's map may draw.
	protected static ref array<ref LF_MapPoiPlace> s_aLocal;
	//! Server: players with a duty-change resend queued.
	protected static ref map<int, bool> s_mQueued;

	//------------------------------------------------------------------------------------------------
	//! A kind stays on the server when the POI table shows it to some players only.
	static bool IsPrivate(LF_MapPoiConfig config, string kind)
	{
		if (!config)
			return false;

		LF_MapPoiKind row = config.Find(kind);
		return row && row.m_eVisibility != LF_EMapPoiVisibility.PUBLIC;
	}

	//------------------------------------------------------------------------------------------------
	//! The private places one audience may see: the visibility rule run on every place whose row is not
	//! PUBLIC; a place the rule hides is left out. Pure: the logic tests call it with fixtures.
	//! \return How many places were added to into.
	static int Entitled(notnull LF_MapPoiConfig config, notnull LF_MapPoiAudience audience, notnull array<ref LF_MapPoiPlace> places, notnull array<ref LF_MapPoiPlace> into)
	{
		int added = 0;
		foreach (LF_MapPoiPlace place : places)
		{
			if (!place)
				continue;

			LF_MapPoiKind row = config.Find(place.m_sKind);
			if (!row || row.m_eVisibility == LF_EMapPoiVisibility.PUBLIC)
				continue;

			LF_EMapPoiState state = LF_MapPoiRules.Resolve(row.m_eVisibility, row.m_sRole, place.m_bLive, audience.KnownOf(place.m_sId), audience.GetRoles());
			if (state == LF_EMapPoiState.HIDDEN)
				continue;

			into.Insert(place);
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! The role bits of a packed account state that change what a player may see (police, medic on duty).
	static int RoleMask(int state)
	{
		int mask = 0;
		if (LF_AccountStateBits.IsOnDutyPolice(state))
			mask |= 1;

		if (LF_MedicDuty.IsOnDutyState(state))
			mask |= 2;

		return mask;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every live place of a private kind, from the server's own LF_Places.
	static void GatherPrivate(notnull LF_MapPoiConfig config, notnull array<ref LF_MapPoiPlace> into)
	{
		if (!config.m_aKinds)
			return;

		foreach (LF_MapPoiKind row : config.m_aKinds)
		{
			if (row && !LF_MapPoiConfig.IsSlotRow(row) && row.m_eVisibility != LF_EMapPoiVisibility.PUBLIC)
				LF_MapPois.GatherKind(row.m_sKind, into);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: what the server knows of one player: the roles their account is on duty in and what their
	//! character knows of each of the given places.
	static LF_MapPoiAudience ServerAudience(string characterId, IEntity character, notnull array<ref LF_MapPoiPlace> places)
	{
		LF_MapPoiAudience audience = new LF_MapPoiAudience();
		audience.AddRolesFromState(LF_AccountReplication.GetState(character));
		if (characterId.IsEmpty())
			return audience;

		foreach (LF_MapPoiPlace place : places)
		{
			if (place)
				audience.SetKnown(place.m_sId, LF_MapKnown.KnownOf(characterId, place.m_sId));
		}

		return audience;
	}

	//------------------------------------------------------------------------------------------------
	//! Wire form of a set: parallel kinds and ids, three decimetre ints per position. Pure.
	static void Encode(notnull array<ref LF_MapPoiPlace> places, notnull array<string> kinds, notnull array<string> ids, notnull array<int> positions)
	{
		foreach (LF_MapPoiPlace place : places)
		{
			if (!place || kinds.Count() >= MAX_PLACES)
				continue;

			int x = Math.Round(place.m_vPos[0] * POS_SCALE);
			int y = Math.Round(place.m_vPos[1] * POS_SCALE);
			int z = Math.Round(place.m_vPos[2] * POS_SCALE);
			kinds.Insert(place.m_sKind);
			ids.Insert(place.m_sId);
			positions.Insert(x);
			positions.Insert(y);
			positions.Insert(z);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the wire form back; a short or mismatched payload keeps what lines up. Pure.
	static int Decode(array<string> kinds, array<string> ids, array<int> positions, notnull array<ref LF_MapPoiPlace> into)
	{
		if (!kinds || !ids || !positions)
			return 0;

		int count = Math.Min(kinds.Count(), ids.Count());
		count = Math.Min(count, positions.Count() / 3);
		count = Math.Min(count, MAX_PLACES);
		for (int i = 0; i < count; i++)
		{
			vector pos = Vector(positions[i * 3] / POS_SCALE, positions[i * 3 + 1] / POS_SCALE, positions[i * 3 + 2] / POS_SCALE);
			into.Insert(LF_MapPoiPlace.Make(kinds[i], ids[i], pos, true));
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the private places this player may see now, to that player's own controller.
	//! \return How many places were sent; -1 when nothing was sent.
	static int SendTo(SCR_PlayerController controller)
	{
		if (!controller || !Replication.IsServer())
			return -1;

		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		if (!config)
			return -1;

		int playerId = controller.GetPlayerId();
		string characterId = LF_Utils.GetPlayerUID(playerId);
		array<ref LF_MapPoiPlace> places = {};
		GatherPrivate(config, places);
		LF_MapPoiAudience audience = ServerAudience(characterId, controller.GetControlledEntity(), places);
		array<ref LF_MapPoiPlace> entitled = {};
		Entitled(config, audience, places, entitled);
		if (entitled.Count() > MAX_PLACES)
			LF_Debug.Warn(FEATURE, string.Format("player %1 may see %2 private places; only the first %3 are sent", playerId, entitled.Count(), MAX_PLACES));

		array<string> kinds = {};
		array<string> ids = {};
		array<int> positions = {};
		Encode(entitled, kinds, ids, positions);
		controller.LF_SendMapPrivate(kinds, ids, positions);
		LF_Debug.Info(FEATURE, string.Format("player %1 may see %2 of %3 private place(s)", playerId, kinds.Count(), places.Count()));
		return kinds.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a duty change of this character; its player's set is sent once at the end of the frame.
	static void QueueFor(IEntity character)
	{
		if (!character || !Replication.IsServer() || !GetGame() || !GetGame().GetPlayerManager())
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character);
		if (playerId <= 0)
			return;

		if (!s_mQueued)
			s_mQueued = new map<int, bool>();

		if (s_mQueued.Contains(playerId))
			return;

		s_mQueued.Set(playerId, true);
		GetGame().GetCallqueue().CallLater(SendQueued, 0, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SendQueued(int playerId)
	{
		if (s_mQueued)
			s_mQueued.Remove(playerId);

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		SendTo(SCR_PlayerController.Cast(players.GetPlayerController(playerId)));
	}

	//------------------------------------------------------------------------------------------------
	//! Client: replaces the set with what the server sent.
	static void SetLocal(array<string> kinds, array<string> ids, array<int> positions)
	{
		s_aLocal = new array<ref LF_MapPoiPlace>();
		Decode(kinds, ids, positions, s_aLocal);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: appends the server-sent private places to into (LF_MapPois.GatherVisible).
	static int AppendLocal(notnull array<ref LF_MapPoiPlace> into)
	{
		if (!s_aLocal)
			return 0;

		foreach (LF_MapPoiPlace place : s_aLocal)
		{
			into.Insert(place);
		}

		return s_aLocal.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: how many private places the server sent; and whether it sent one with this id.
	static int LocalCount()
	{
		if (!s_aLocal)
			return 0;

		return s_aLocal.Count();
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_mQueued && !s_mQueued.IsEmpty() && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(SendQueued);

		s_aLocal = null;
		s_mQueued = null;
	}
}

//------------------------------------------------------------------------------------------------
//! A private POI kind never registers on a client (LF_Places.RegisterOn).
modded class LF_Places
{
	//------------------------------------------------------------------------------------------------
	override static bool IsServerOnlyKind(string kind)
	{
		return LF_MapPrivatePlaces.IsPrivate(LF_MapPoiConfig.Get(), kind);
	}
}

//------------------------------------------------------------------------------------------------
//! The private-places channel on the player's own controller: one owner RPC, never a broadcast.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Server: this player's private places.
	void LF_SendMapPrivate(array<string> kinds, array<string> ids, array<int> positions)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_MapPrivate(kinds, ids, positions);
		else
			Rpc(RpcDo_LF_MapPrivate, kinds, ids, positions);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_MapPrivate(array<string> kinds, array<string> ids, array<int> positions)
	{
		LF_MapPrivatePlaces.SetLocal(kinds, ids, positions);
		LF_Debug.Info(LF_MapPrivatePlaces.FEATURE, string.Format("this map may draw %1 private place(s)", LF_MapPrivatePlaces.LocalCount()));
	}
}

//------------------------------------------------------------------------------------------------
//! Going on or off duty changes which ROLE places a player may see: the server sends the set again.
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	override void LF_SetAccountState(int state)
	{
		int before = LF_GetAccountState();
		super.LF_SetAccountState(state);
		if (!Replication.IsServer())
			return;

		if (LF_MapPrivatePlaces.RoleMask(before) != LF_MapPrivatePlaces.RoleMask(state))
			LF_MapPrivatePlaces.QueueFor(this);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MapPrivatePlaces's statics (LF_StaticReset finds it by reflection).
class LF_MapPrivatePlaces_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MapPrivatePlaces";
	}

	override void Apply()
	{
		LF_MapPrivatePlaces.ResetStatics();
	}
}
