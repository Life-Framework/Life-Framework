//------------------------------------------------------------------------------------------------
//! The values of one join snapshot, written and read through one small surface so the format code
//! runs the same over a network bit stream (LF_FelledTreesBitWire) and in a test (a token list).
class LF_FelledTreesWire : Managed
{
	void WriteInt(int value)
	{
	}

	//------------------------------------------------------------------------------------------------
	void WriteString(string value)
	{
	}

	//------------------------------------------------------------------------------------------------
	bool ReadInt(out int value)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	bool ReadString(out string value)
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The wire over the game mode's RplSave writer or RplLoad reader.
class LF_FelledTreesBitWire : LF_FelledTreesWire
{
	protected ScriptBitWriter m_Writer;
	protected ScriptBitReader m_Reader;

	//------------------------------------------------------------------------------------------------
	static LF_FelledTreesBitWire ForWriter(ScriptBitWriter writer)
	{
		LF_FelledTreesBitWire wire = new LF_FelledTreesBitWire();
		wire.m_Writer = writer;
		return wire;
	}

	//------------------------------------------------------------------------------------------------
	static LF_FelledTreesBitWire ForReader(ScriptBitReader reader)
	{
		LF_FelledTreesBitWire wire = new LF_FelledTreesBitWire();
		wire.m_Reader = reader;
		return wire;
	}

	//------------------------------------------------------------------------------------------------
	override void WriteInt(int value)
	{
		m_Writer.WriteInt(value);
	}

	//------------------------------------------------------------------------------------------------
	override void WriteString(string value)
	{
		m_Writer.WriteString(value);
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadInt(out int value)
	{
		return m_Reader.ReadInt(value);
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadString(out string value)
	{
		return m_Reader.ReadString(value);
	}
}

//------------------------------------------------------------------------------------------------
//! Map entities found near a point, for matching a felled tree key to the tree it names.
class LF_FelledTreeQuery : Managed
{
	ref array<IEntity> m_aEntities = {};

	//------------------------------------------------------------------------------------------------
	bool Collect(IEntity entity)
	{
		if (entity)
			m_aEntities.Insert(entity);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Felled map trees reach every client (#3261, design docs/roadmap/lumber.md "Felling a real map tree").
//! A map tree is the same entity on every machine, but the server's hide of it is not replicated, so a
//! felled tree kept standing on every other screen. The server keeps a table of felled tree KEYS
//! (LF_TreeFelling.KeyOf: prefab and position to 0.1 m, never an EntityID, which differs per machine):
//!  - Publish and Retract change the table and each send one broadcast RPC on the game mode (a change
//!    only; nothing per frame);
//!  - a late joiner gets the whole table in the game mode's RplSave/RplLoad (a prefab list, then x, z
//!    and prefab index per tree);
//!  - a client keeps a mirror of the keys, finds the map tree near each key by prefab and position, and
//!    hides it (invisible, untraceable, out of the collision world) or shows it again. A key whose tree is not
//!    streamed in yet stays pending and is tried again every few seconds.
//! The server side of a fell (the real hide, the ledger, regrow and save) belongs to the axe action and
//! the regrow tick, which call Publish and Retract. A broadcast never runs on its sender, so a listen host
//! (not a console app) runs the same client handler itself and hides its own copy; RplLoad never runs on the server.
class LF_FelledTreesReplication
{
	static const string FEATURE = "Lumber";
	//! RplSave/RplLoad layout; bump it when the fields change.
	static const int FORMAT_VERSION = 1;
	static const int MAX_KEYS = 2048;	// lf-fixed: a sanity cap on a corrupt count, far above the trees a server has down at once (45 min regrow)
	static const int MAX_PREFABS = 256;	// lf-fixed: a sanity cap on a corrupt count, far above the tree prefabs on the island
	//! A key's position is rounded to 0.1 m on x and z, so the same tree is within 0.071 m of it.
	static const float MATCH_M = 0.08;
	//! Query sphere around a key's ground point; a tree's origin is at its foot, this only covers the terrain height guess.
	static const float QUERY_M = 3;
	static const int RETRY_MS = 4000;	// lf-fixed: a client-side retry cadence for a tree that has not streamed in, not a tunable
	static const int RETRY_PER_PASS = 24;	// lf-fixed: bounds the queries one retry pass makes

	protected static ref map<string, bool> s_mTable;
	protected static ref map<string, bool> s_mMirror;
	protected static ref map<string, EntityID> s_mHidden;
	protected static ref map<string, int> s_mLayers;
	protected static ref array<string> s_aPending;
	protected static bool s_bRetryScheduled;
	protected static bool s_bNoCarrierLogged;

	//------------------------------------------------------------------------------------------------
	protected static map<string, bool> Table()
	{
		if (!s_mTable)
			s_mTable = new map<string, bool>();

		return s_mTable;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, bool> Mirror()
	{
		if (!s_mMirror)
			s_mMirror = new map<string, bool>();

		return s_mMirror;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, EntityID> Hidden()
	{
		if (!s_mHidden)
			s_mHidden = new map<string, EntityID>();

		return s_mHidden;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, int> Layers()
	{
		if (!s_mLayers)
			s_mLayers = new map<string, int>();

		return s_mLayers;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<string> Pending()
	{
		if (!s_aPending)
			s_aPending = new array<string>();

		return s_aPending;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mTable = null;
		s_mMirror = null;
		s_mHidden = null;
		s_mLayers = null;
		s_aPending = null;
		s_bRetryScheduled = false;
		s_bNoCarrierLogged = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The game mode that carries the RPC and the join snapshot; null when the mode is not ours.
	protected static LF_GameModeRoleplay Carrier()
	{
		LF_GameModeRoleplay carrier = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (!carrier && !s_bNoCarrierLogged)
		{
			s_bNoCarrierLogged = true;
			LF_Debug.Warn(FEATURE, "felled trees: the game mode is not LF_GameModeRoleplay, clients are not told about felled trees");
		}

		return carrier;
	}

	//------------------------------------------------------------------------------------------------
	// Keys
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Splits a key made by LF_TreeFelling.KeyOf ("<dx>:<dz>:<prefab>", x and z in decimetres).
	//! \return false when the text is not a tree key.
	static bool ParseKey(string key, out int dx, out int dz, out string prefab)
	{
		int first = key.IndexOf(":");
		if (first <= 0)
			return false;

		string rest = key.Substring(first + 1, key.Length() - first - 1);
		int second = rest.IndexOf(":");
		if (second <= 0)
			return false;

		string xText = key.Substring(0, first);
		string zText = rest.Substring(0, second);
		prefab = rest.Substring(second + 1, rest.Length() - second - 1);
		if (prefab.IsEmpty() || !IsWholeNumber(xText) || !IsWholeNumber(zText))
			return false;

		dx = xText.ToInt();
		dz = zText.ToInt();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsWholeNumber(string text)
	{
		if (text.IsEmpty())
			return false;

		string digits = "0123456789";
		for (int i = 0; i < text.Length(); i++)
		{
			string c = text.Get(i);
			if (i == 0 && c == "-" && text.Length() > 1)
				continue;

			if (digits.IndexOf(c) < 0)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The index of the candidate the key names, or -1. A candidate is a tree's position and prefab
	//! path; it matches when its prefab is the key's and it is the nearest within MATCH_M on x and z, so
	//! two trees 0.5 m apart are never mixed up.
	static int Pick(notnull array<vector> positions, notnull array<string> prefabs, string key)
	{
		int dx;
		int dz;
		string prefab;
		if (!ParseKey(key, dx, dz, prefab))
			return -1;

		prefab.ToLower();
		float keyX = dx / 10.0;
		float keyZ = dz / 10.0;
		int best = -1;
		float bestGap = MATCH_M;
		int count = LF_Utils.MinInt(positions.Count(), prefabs.Count());
		for (int i = 0; i < count; i++)
		{
			string candidate = prefabs[i];
			candidate.ToLower();
			if (candidate != prefab)	// lf-path-ok: both sides are the same lower-cased path from LF_TreeFelling.KeyOf, never a GUID-only reference
				continue;

			float gapX = positions[i][0] - keyX;
			float gapZ = positions[i][2] - keyZ;
			float gap = Math.Sqrt(gapX * gapX + gapZ * gapZ);
			if (gap <= bestGap)
			{
				bestGap = gap;
				best = i;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	// Server
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: a map tree is down; clients hide it. \return false when nothing changed (not a server, not a
	//! tree key, already down, or the table is full).
	static bool Publish(string key)
	{
		if (!Replication.IsServer())
			return false;

		int dx;
		int dz;
		string prefab;
		if (!ParseKey(key, dx, dz, prefab))
		{
			LF_Debug.Warn(FEATURE, "felled trees: not a tree key, not sent: " + key);
			return false;
		}

		if (Table().Contains(key))
			return false;

		if (Table().Count() >= MAX_KEYS)
		{
			LF_Debug.Error(FEATURE, string.Format("felled trees: %1 trees are already down; %2 is not sent to clients", Table().Count(), key));
			return false;
		}

		Table().Set(key, true);
		LF_Debug.Info(FEATURE, string.Format("felled trees: %1 is down, told to clients (%2 down)", key, Table().Count()));
		LF_GameModeRoleplay carrier = Carrier();
		if (carrier)
			carrier.LF_FelledTreeBroadcastDown(key);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a map tree stands again; clients show it. \return false when it was not in the table.
	static bool Retract(string key)
	{
		if (!Replication.IsServer() || !Table().Contains(key))
			return false;

		Table().Remove(key);
		LF_Debug.Info(FEATURE, string.Format("felled trees: %1 stands again, told to clients (%2 down)", key, Table().Count()));
		LF_GameModeRoleplay carrier = Carrier();
		if (carrier)
			carrier.LF_FelledTreeBroadcastUp(key);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! What a late joiner is sent: every key that is down.
	static array<string> Collect()
	{
		array<string> keys = {};
		foreach (string key, bool down : Table())
		{
			keys.Insert(key);
		}

		return keys;
	}

	//------------------------------------------------------------------------------------------------
	// Join snapshot
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Writes the keys: the format, the distinct prefab paths, then x, z and the prefab index of each tree.
	static void Write(notnull LF_FelledTreesWire wire, notnull array<string> keys)
	{
		array<string> prefabs = {};
		array<int> xs = {};
		array<int> zs = {};
		array<int> slots = {};
		foreach (string key : keys)
		{
			int dx;
			int dz;
			string prefab;
			if (!ParseKey(key, dx, dz, prefab))
				continue;

			int slot = prefabs.Find(prefab);
			if (slot < 0)
			{
				slot = prefabs.Count();
				prefabs.Insert(prefab);
			}

			xs.Insert(dx);
			zs.Insert(dz);
			slots.Insert(slot);
		}

		wire.WriteInt(FORMAT_VERSION);
		wire.WriteInt(prefabs.Count());
		foreach (string path : prefabs)
		{
			wire.WriteString(path);
		}

		wire.WriteInt(xs.Count());
		for (int i = 0; i < xs.Count(); i++)
		{
			wire.WriteInt(xs[i]);
			wire.WriteInt(zs[i]);
			wire.WriteInt(slots[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when the stream is short or corrupt (the snapshot is then refused). A format this
	//! build does not know is read to keep the stream aligned and gives no keys.
	static bool Read(notnull LF_FelledTreesWire wire, notnull array<string> keys)
	{
		int version;
		int prefabCount;
		if (!wire.ReadInt(version) || !wire.ReadInt(prefabCount))
			return false;

		if (prefabCount < 0 || prefabCount > MAX_PREFABS)
		{
			LF_Debug.Error(FEATURE, string.Format("felled trees: the snapshot claims %1 tree prefabs; refused", prefabCount));
			return false;
		}

		array<string> prefabs = {};
		for (int i = 0; i < prefabCount; i++)
		{
			string path;
			if (!wire.ReadString(path))
				return false;

			prefabs.Insert(path);
		}

		int count;
		if (!wire.ReadInt(count))
			return false;

		if (count < 0 || count > MAX_KEYS)
		{
			LF_Debug.Error(FEATURE, string.Format("felled trees: the snapshot claims %1 felled trees; refused", count));
			return false;
		}

		array<string> read = {};
		for (int t = 0; t < count; t++)
		{
			int dx;
			int dz;
			int slot;
			if (!wire.ReadInt(dx) || !wire.ReadInt(dz) || !wire.ReadInt(slot))
				return false;

			if (slot < 0 || slot >= prefabs.Count())
			{
				LF_Debug.Error(FEATURE, string.Format("felled trees: a snapshot tree names prefab %1 of %2; refused", slot, prefabs.Count()));
				return false;
			}

			read.Insert(string.Format("%1:%2:%3", dx, dz, prefabs[slot]));
		}

		if (version != FORMAT_VERSION)
		{
			LF_Debug.Error(FEATURE, string.Format("felled trees: trees not hidden: the server sends format %1, this build reads %2", version, FORMAT_VERSION));
			return true;
		}

		keys.InsertAll(read);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Client
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! How many keys this client believes are down.
	static int ClientCount()
	{
		return Mirror().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! How many of them have a tree hidden on this client.
	static int ClientHiddenCount()
	{
		return Hidden().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! How many keys wait for their tree to stream in.
	static int ClientPendingCount()
	{
		return Pending().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a tree is down. The key joins the mirror and its tree is hidden, now or when it streams in.
	static void ClientDown(string key)
	{
		int dx;
		int dz;
		string prefab;
		if (!ParseKey(key, dx, dz, prefab))
			return;

		Mirror().Set(key, true);
		if (Hidden().Contains(key))
			return;

		if (!TryHide(key))
			Defer(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a tree stands again. It is shown and the key leaves the mirror.
	static void ClientUp(string key)
	{
		Mirror().Remove(key);
		RemovePending(key);
		Reveal(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the whole table replaces the mirror (a join snapshot). Trees no longer down are shown.
	//! \return how many keys are down.
	static int ClientApplyAll(array<string> keys)
	{
		map<string, bool> next = new map<string, bool>();
		if (keys)
		{
			foreach (string key : keys)
			{
				next.Set(key, true);
			}
		}

		array<string> gone = {};
		foreach (string key, bool down : Mirror())
		{
			if (!next.Contains(key))
				gone.Insert(key);
		}

		foreach (string key : gone)
		{
			ClientUp(key);
		}

		foreach (string key, bool down : next)
		{
			ClientDown(key);
		}

		return Mirror().Count();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Defer(string key)
	{
		if (Pending().Find(key) < 0)
			Pending().Insert(key);

		ScheduleRetry();
	}

	//------------------------------------------------------------------------------------------------
	protected static void RemovePending(string key)
	{
		if (!s_aPending)
			return;

		int at = s_aPending.Find(key);
		if (at >= 0)
			s_aPending.Remove(at);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ScheduleRetry()
	{
		if (s_bRetryScheduled || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bRetryScheduled = true;
		GetGame().GetCallqueue().CallLater(RetryPending, RETRY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Tries a bounded batch of waiting keys again, oldest first; the rest wait for the next pass.
	protected static void RetryPending()
	{
		s_bRetryScheduled = false;
		if (!s_aPending || s_aPending.IsEmpty())
			return;

		int tries = LF_Utils.MinInt(s_aPending.Count(), RETRY_PER_PASS);
		for (int i = 0; i < tries; i++)
		{
			string key = s_aPending[0];
			s_aPending.Remove(0);
			if (!Mirror().Contains(key))
				continue;

			if (!TryHide(key))
				s_aPending.Insert(key);
		}

		if (!s_aPending.IsEmpty())
			ScheduleRetry();
	}

	//------------------------------------------------------------------------------------------------
	//! The map tree a key names, or null when it is not streamed in here.
	static IEntity FindTree(string key)
	{
		int dx;
		int dz;
		string prefab;
		if (!ParseKey(key, dx, dz, prefab))
			return null;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		float x = dx / 10.0;
		float z = dz / 10.0;
		vector centre = Vector(x, world.GetSurfaceY(x, z), z);
		LF_FelledTreeQuery query = new LF_FelledTreeQuery();
		world.QueryEntitiesBySphere(centre, QUERY_M, query.Collect, null, EQueryEntitiesFlags.ALL);

		array<vector> positions = {};
		array<string> prefabs = {};
		array<IEntity> entities = {};
		foreach (IEntity entity : query.m_aEntities)
		{
			EntityPrefabData data = entity.GetPrefabData();
			if (!data)
				continue;

			positions.Insert(entity.GetOrigin());
			prefabs.Insert(data.GetPrefabName());
			entities.Insert(entity);
		}

		int at = Pick(positions, prefabs, key);
		if (at < 0)
			return null;

		return entities[at];
	}

	//------------------------------------------------------------------------------------------------
	//! Hides the tree a key names on this machine. \return false when it is not there to hide.
	static bool TryHide(string key)
	{
		IEntity tree = FindTree(key);
		if (!tree)
			return false;

		Layers().Set(key, HideTree(tree));
		Hidden().Set(key, tree.GetID());
		LF_Debug.Info(FEATURE, string.Format("felled trees: hid tree %1 (%2 hidden here)", key, Hidden().Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the tree this machine hid for a key.
	protected static void Reveal(string key)
	{
		if (!s_mHidden)
			return;

		EntityID id;
		if (!s_mHidden.Find(key, id))
			return;

		s_mHidden.Remove(key);
		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetWorld())
			return;

		int layer = -1;
		if (s_mLayers && s_mLayers.Find(key, layer))
			s_mLayers.Remove(key);

		IEntity tree = game.GetWorld().FindEntityByID(id);
		if (tree)
			ShowTree(tree, layer);

		LF_Debug.Info(FEATURE, string.Format("felled trees: showed tree %1 again (%2 hidden here)", key, s_mHidden.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a tree out of the world as a player sees and bumps into it: not drawn, not hit by a trace
	//! or a bullet, no collision. Public so the axe action hides the server's own copy the same way.
	//! The body leaves the collision world (SimulationState.NONE, as a spent ore node does): a static
	//! body ignores SetInteractionLayer(0), so a layer change left an invisible tree players walk into.
	//! \return the simulation state it had, to hand back to ShowTree; -1 when it has no physics or was
	//! already hidden. On a listen host the axe action hides the tree, then the local broadcast handler
	//! finds the same entity; whoever hid it first holds the state, so a second show cannot undo it wrong.
	static int HideTree(notnull IEntity tree)
	{
		if (IsHidden(tree))
			return -1;

		tree.ClearFlags(EntityFlags.VISIBLE | EntityFlags.TRACEABLE, false);
		Physics physics = tree.GetPhysics();
		if (!physics)
			return -1;

		int state = physics.GetSimulationState();
		physics.ChangeSimulationState(SimulationState.NONE);
		return state;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a hidden tree back: drawn, traceable, with the simulation state HideTree returned (-1: none to restore).
	static void ShowTree(notnull IEntity tree, int state)
	{
		tree.SetFlags(EntityFlags.VISIBLE | EntityFlags.TRACEABLE, false);
		if (state < 0)
			return;

		Physics physics = tree.GetPhysics();
		if (physics)
			physics.ChangeSimulationState(state);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the tree is hidden as HideTree leaves it (not drawn).
	static bool IsHidden(notnull IEntity tree)
	{
		return (tree.GetFlags() & EntityFlags.VISIBLE) == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Harness: the nearest map tree within `radius` of a point, or null.
	static IEntity NearestTree(vector from, float radius, out int noPrefab)
	{
		noPrefab = 0;
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		LF_FelledTreeQuery query = new LF_FelledTreeQuery();
		world.QueryEntitiesBySphere(from, radius, query.Collect, null, EQueryEntitiesFlags.ALL);
		IEntity best;
		float bestGap = float.MAX;
		foreach (IEntity entity : query.m_aEntities)
		{
			if (!Tree.Cast(entity) || IsHidden(entity))
				continue;

			if (KeyOfTree(entity).IsEmpty())
			{
				noPrefab++;
				continue;
			}

			float gap = vector.Distance(entity.GetOrigin(), from);
			if (gap < bestGap)
			{
				bestGap = gap;
				best = entity;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Harness: what this client shows for a key: `down` (hidden), `standing`, or `missing` (not streamed in).
	static string StateOf(string key)
	{
		IEntity tree = FindTree(key);
		if (!tree)
			return "missing";

		if (IsHidden(tree))
			return "down";

		return "standing";
	}

	//------------------------------------------------------------------------------------------------
	//! Harness: the key of a tree.
	static string KeyOfTree(notnull IEntity tree)
	{
		EntityPrefabData data = tree.GetPrefabData();
		if (!data || data.GetPrefabName().IsEmpty())
			return string.Empty;

		return LF_TreeFelling.KeyOf(tree.GetOrigin(), data.GetPrefabName());
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FelledTreesReplication's statics (LF_StaticReset finds it by reflection).
class LF_FelledTreesReplication_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FelledTreesReplication";
	}

	override void Apply()
	{
		LF_FelledTreesReplication.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The game mode is the replicated owner that carries felled trees: two broadcast RPCs for the
//! changes and, after whatever the chain below writes, the whole table for a late joiner (positional:
//! RplLoad reads exactly what RplSave writes, in this order, after super).
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	//! Server: a map tree is down.
	void LF_FelledTreeBroadcastDown(string key)
	{
		Rpc(RpcDo_LF_FelledTreeDown, key);
		if (!System.IsConsoleApp())
			RpcDo_LF_FelledTreeDown(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a map tree stands again.
	void LF_FelledTreeBroadcastUp(string key)
	{
		Rpc(RpcDo_LF_FelledTreeUp, key);
		if (!System.IsConsoleApp())
			RpcDo_LF_FelledTreeUp(key);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_FelledTreeDown(string key)
	{
		LF_FelledTreesReplication.ClientDown(key);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_FelledTreeUp(string key)
	{
		LF_FelledTreesReplication.ClientUp(key);
	}

	//------------------------------------------------------------------------------------------------
	override bool RplSave(ScriptBitWriter writer)
	{
		if (!super.RplSave(writer))
			return false;

		array<string> keys = LF_FelledTreesReplication.Collect();
		LF_FelledTreesReplication.Write(LF_FelledTreesBitWire.ForWriter(writer), keys);
		if (!keys.IsEmpty())
			LF_Debug.Info(LF_FelledTreesReplication.FEATURE, string.Format("felled trees: join snapshot carries %1 tree(s)", keys.Count()));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		if (!super.RplLoad(reader))
			return false;

		array<string> keys = {};
		if (!LF_FelledTreesReplication.Read(LF_FelledTreesBitWire.ForReader(reader), keys))
		{
			LF_Debug.Error(LF_FelledTreesReplication.FEATURE, "felled trees: the join snapshot carries no tree table; trees hide as they are felled");
			return false;
		}

		int applied = LF_FelledTreesReplication.ClientApplyAll(keys);
		LF_Debug.Info(LF_FelledTreesReplication.FEATURE, string.Format("felled trees: join snapshot has %1 tree(s) down, %2 hidden now, %3 waiting for the map", applied, LF_FelledTreesReplication.ClientHiddenCount(), LF_FelledTreesReplication.ClientPendingCount()));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Harness `fellsync`: the late-join proof for felled trees (dev gate on the server).
//!   fellsync find [radius]   the nearest standing tree near the character: `key=<key> pos=<x> <y> <z>`
//!   fellsync fell [radius|key]  the client asks the server to publish that tree (a key from `find`, or the
//!                            nearest one); the reply is `key=<key> changed=<0|1>`
//!   fellsync state <key>     what this client shows for that tree: down, standing or missing
//!   fellsync up <key>        the server shows the tree again
//! The harness only publishes the key (what the axe action does after its own checks); it does not
//! fell anything for real.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "fellsync")
			return super.Handle(cmd);

		if (cmd.Arg1 == "state")
		{
			string stateKey = cmd.Arg2;
			if (stateKey.IsEmpty())
			{
				LF_AgentHarness.Answer(cmd.Id, "error", "usage: fellsync state <key>");
				return true;
			}

			string state = LF_FelledTreesReplication.StateOf(stateKey);
			LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("state=%1 mirror=%2 hidden=%3 pending=%4", state, LF_FelledTreesReplication.ClientCount(), LF_FelledTreesReplication.ClientHiddenCount(), LF_FelledTreesReplication.ClientPendingCount()));
			return true;
		}

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if ((cmd.Arg1 != "fell" && cmd.Arg1 != "find" && cmd.Arg1 != "up") || !character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: fellsync find|fell [radius] | up <key> | state <key> (find, fell and up need a character)");
			return true;
		}

		string key = cmd.Arg2;
		// `fell <key>` publishes the tree `find` named; `fell [radius]` picks the nearest one itself.
		bool keyGiven = cmd.Arg1 == "fell" && cmd.Arg2.Contains(":");
		if ((cmd.Arg1 == "fell" && !keyGiven) || cmd.Arg1 == "find")
		{
			float radius = 25;
			if (!cmd.Arg2.IsEmpty())
				radius = cmd.Arg2.ToFloat();

			int noPrefab;
			IEntity tree = LF_FelledTreesReplication.NearestTree(character.GetOrigin(), radius, noPrefab);
			if (!tree)
			{
				LF_AgentHarness.Answer(cmd.Id, "error", string.Format("no standing map tree with a prefab within %1 m of the character (%2 trees without a prefab skipped)", radius, noPrefab));
				return true;
			}

			key = LF_FelledTreesReplication.KeyOfTree(tree);
			if (cmd.Arg1 == "find")
			{
				vector at = tree.GetOrigin();
				LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("key=%1 pos=%2 %3 %4", key, at[0], at[1], at[2]));
				return true;
			}
		}

		if (key.IsEmpty())
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: fellsync up <key>");
			return true;
		}

		character.LF_AskHarnessFellSync(cmd.Id, cmd.Arg1 == "fell", key);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskHarnessFellSync(int commandId, bool down, string key)
	{
		if (Replication.IsServer())
		{
			LF_HarnessFellSyncDo(commandId, down, key);
			return;
		}

		Rpc(RpcAsk_LF_HarnessFellSync, commandId, down, key);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessFellSync(int commandId, bool down, string key)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "fellsync"))
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		LF_HarnessFellSyncDo(commandId, down, key);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_HarnessFellSyncDo(int commandId, bool down, string key)
	{
		bool changed;
		if (down)
			changed = LF_FelledTreesReplication.Publish(key);
		else
			changed = LF_FelledTreesReplication.Retract(key);

		LF_HarnessReply(commandId, "ok", string.Format("key=%1 changed=%2", key, changed));
	}
}
