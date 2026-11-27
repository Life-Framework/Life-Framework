// LF_WheatFieldReplication.c: a field's cut cells reach every client (#3288, harvester spike section 3). The server
// sends the cuts in batches (at most one reliable RpcDo_ per field per 250 ms, carrying packed cell words) plus one
// regrow RPC, and RplSave/RplLoad carries the whole bitset (9 ints for 260 cells) so a late joiner draws the field as
// it is. A dedicated server keeps the bitset and draws no patch (LF_WheatField.DrawsHere). Nothing is persisted.

//------------------------------------------------------------------------------------------------
//! The wire format of a field's cuts. Pure: no entity, no world, no clock.
//! A batch is a string of 12-character entries, one per bitset word that holds a cut cell: 4 hex digits of word
//! index then 8 hex digits of the 32-cell mask (cell = word * 32 + bit). A string avoids a negative int on the
//! wire and fits one RPC parameter; 260 cells are at most 9 entries, 108 characters.
class LF_WheatFieldWire
{
	static const string FEATURE = "Harvest";
	//! RplSave/RplLoad layout; bump it when the fields change.
	static const int SNAPSHOT_VERSION = 1;
	static const int ENTRY_LENGTH = 12;
	static const int MAX_WORDS = 8192;	// lf-fixed: a sanity cap (262144 cells), far above any field and below the 4-digit word index
	protected static const string HEX = "0123456789abcdef";

	//------------------------------------------------------------------------------------------------
	protected static string HexOf(int value, int digits)
	{
		string text;
		for (int i = digits - 1; i >= 0; i--)
		{
			int nibble = (value >> (4 * i)) & 15;
			text += HEX.Substring(nibble, 1);
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when a character is not a hex digit.
	protected static bool ParseHex(string text, int from, int digits, out int value)
	{
		value = 0;
		for (int i = 0; i < digits; i++)
		{
			int nibble = HEX.IndexOf(text.Substring(from + i, 1));
			if (nibble < 0)
				return false;

			value = (value << 4) | nibble;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Packs a set of cells (any order, repeats and negatives or past MAX_WORDS ignored) into the batch string.
	static string Encode(array<int> cells)
	{
		if (!cells || cells.IsEmpty())
			return "";

		int maxWord = -1;
		foreach (int cell : cells)
		{
			if (cell < 0 || cell / 32 >= MAX_WORDS)
				continue;

			maxWord = LF_Utils.MaxInt(maxWord, cell / 32);
		}

		if (maxWord < 0)
			return "";

		array<int> words = {};
		for (int i = 0; i <= maxWord; i++)
		{
			words.Insert(0);
		}

		foreach (int cell : cells)
		{
			if (cell < 0 || cell / 32 >= MAX_WORDS)
				continue;

			words[cell / 32] = words[cell / 32] | (1 << (cell % 32));
		}

		string packed;
		for (int word = 0; word <= maxWord; word++)
		{
			if (words[word] != 0)
				packed += HexOf(word, 4) + HexOf(words[word], 8);
		}

		return packed;
	}

	//------------------------------------------------------------------------------------------------
	//! Unpacks a batch into its cells, ascending. \return false for a string that is not a whole number of valid
	//! entries (nothing is added then).
	static bool Decode(string packed, out array<int> cells)
	{
		cells = {};
		int length = packed.Length();
		if (length % ENTRY_LENGTH != 0)
			return false;

		array<int> found = {};
		for (int at = 0; at < length; at += ENTRY_LENGTH)
		{
			int word;
			int mask;
			if (!ParseHex(packed, at, 4, word) || !ParseHex(packed, at + 4, 8, mask) || word >= MAX_WORDS)
				return false;

			for (int bit = 0; bit < 32; bit++)
			{
				if ((mask & (1 << bit)) != 0)
					found.Insert(word * 32 + bit);
			}
		}

		cells.Copy(found);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The late-join snapshot of a grid: its bitset words (260 cells are 9 ints).
	static array<int> Snapshot(LF_WheatGrid grid)
	{
		array<int> words = {};
		if (grid)
			grid.ExportWords(words);

		return words;
	}

	//------------------------------------------------------------------------------------------------
	static void WriteSnapshot(notnull ScriptBitWriter writer, array<int> words)
	{
		writer.WriteInt(SNAPSHOT_VERSION);
		int count = 0;
		if (words)
			count = words.Count();

		writer.WriteInt(count);
		for (int i = 0; i < count; i++)
		{
			writer.WriteInt(words[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when the stream is short or corrupt. A format this build does not know is read to keep the
	//! stream aligned and gives no words (the field then draws standing).
	static bool ReadSnapshot(notnull ScriptBitReader reader, out array<int> words)
	{
		words = {};
		int version;
		int count;
		if (!reader.ReadInt(version) || !reader.ReadInt(count))
			return false;

		if (count < 0 || count > MAX_WORDS)
		{
			LF_Debug.Error(FEATURE, string.Format("wheat field replication: the snapshot claims %1 words; refused", count));
			return false;
		}

		for (int i = 0; i < count; i++)
		{
			int word;
			if (!reader.ReadInt(word))
				return false;

			words.Insert(word);
		}

		if (version != SNAPSHOT_VERSION)
		{
			LF_Debug.Error(FEATURE, string.Format("wheat field replication: snapshot dropped: the server sends format %1, this build reads %2", version, SNAPSHOT_VERSION));
			words.Clear();
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The server's outbox for one field: cells cut since the last send, released at most once per INTERVAL_MS. Pure:
//! the caller passes the clock (milliseconds) so a test drives it.
class LF_WheatCutBatch
{
	static const int INTERVAL_MS = 250;	// lf-fixed: the wire-rate contract of the spike (one reliable RPC per field per 250 ms)

	protected ref array<int> m_aWords = {};
	protected int m_iPending;
	protected int m_iLastSendMs;
	protected bool m_bSentOnce;

	//------------------------------------------------------------------------------------------------
	int Pending()
	{
		return m_iPending;
	}

	//------------------------------------------------------------------------------------------------
	//! Queues a cell; a cell already queued or out of range counts once or not at all.
	void Add(int cell)
	{
		if (cell < 0 || cell / 32 >= LF_WheatFieldWire.MAX_WORDS)
			return;

		while (m_aWords.Count() <= cell / 32)
		{
			m_aWords.Insert(0);
		}

		int bit = 1 << (cell % 32);
		if ((m_aWords[cell / 32] & bit) != 0)
			return;

		m_aWords[cell / 32] = m_aWords[cell / 32] | bit;
		m_iPending++;
	}

	//------------------------------------------------------------------------------------------------
	//! Milliseconds until a send is allowed: 0 when one may go now (also when nothing is queued).
	int WaitMs(int nowMs)
	{
		if (!m_bSentOnce)
			return 0;

		int since = nowMs - m_iLastSendMs;
		if (since < 0 || since >= INTERVAL_MS)
			return 0;

		return INTERVAL_MS - since;
	}

	//------------------------------------------------------------------------------------------------
	//! The packed batch if one may go now, else "". Sending empties the queue and starts the interval; an empty
	//! queue sends nothing and does not start it.
	string Take(int nowMs)
	{
		if (m_iPending == 0 || WaitMs(nowMs) > 0)
			return "";

		array<int> cells = {};
		for (int word = 0; word < m_aWords.Count(); word++)
		{
			for (int bit = 0; bit < 32; bit++)
			{
				if ((m_aWords[word] & (1 << bit)) != 0)
					cells.Insert(word * 32 + bit);
			}
		}

		Clear();
		m_bSentOnce = true;
		m_iLastSendMs = nowMs;
		return LF_WheatFieldWire.Encode(cells);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops everything queued (the field regrew, so the cuts mean nothing).
	void Clear()
	{
		m_aWords.Clear();
		m_iPending = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! The replicated side of LF_WheatField (its prefab carries the RplComponent). The server sends cuts through
//! CutCells and the regrow through Regrow; a client applies them to its own grid and local meshes.
//! A broadcast never runs on the server, and a listen host already drew its own cut, so nothing is called twice.
modded class LF_WheatField
{
	protected ref LF_WheatCutBatch m_CutBatch;
	protected static ref array<EntityID> s_aFieldIds;

	//------------------------------------------------------------------------------------------------
	protected LF_WheatCutBatch CutBatch()
	{
		if (!m_CutBatch)
			m_CutBatch = new LF_WheatCutBatch();

		return m_CutBatch;
	}

	//------------------------------------------------------------------------------------------------
	//! The nth field that has initialised on this machine (the harness verbs address fields by index), or null.
	static LF_WheatField FindByIndex(int index)
	{
		if (!s_aFieldIds || index < 0 || index >= s_aFieldIds.Count())
			return null;

		return LF_WheatField.Cast(GetGame().GetWorld().FindEntityByID(s_aFieldIds[index]));
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aFieldIds = null;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		if (!s_aFieldIds)
			s_aFieldIds = {};

		if (s_aFieldIds.Find(GetID()) < 0)
			s_aFieldIds.Insert(GetID());
	}

	//------------------------------------------------------------------------------------------------
	override int CutCells(array<int> cells)
	{
		if (!cells || !Replication.IsServer())
			return super.CutCells(cells);

		EnsureReady();
		array<int> fresh = {};
		foreach (int cell : cells)
		{
			if (cell >= 0 && cell < m_Grid.CellCount() && !m_Grid.IsCut(cell))
				fresh.Insert(cell);
		}

		int newlyCut = super.CutCells(cells);
		foreach (int cell : fresh)
		{
			CutBatch().Add(cell);
		}

		if (newlyCut > 0)
			SendDue();

		return newlyCut;
	}

	//------------------------------------------------------------------------------------------------
	override void Regrow()
	{
		super.Regrow();
		if (!Replication.IsServer())
			return;

		CutBatch().Clear();
		ClearEventMask(EntityEvent.FRAME);
		Rpc(RpcDo_LF_WheatRegrow);
		LF_Debug.Info(FEATURE, string.Format("wheat field %1: regrow sent to clients", GetName()));
	}

	//------------------------------------------------------------------------------------------------
	//! Sends the queued cuts when the 250 ms interval allows; otherwise waits for the next frame that does.
	protected void SendDue()
	{
		int now = System.GetTickCount();
		string packed = CutBatch().Take(now);
		if (packed.IsEmpty())
		{
			if (CutBatch().Pending() > 0)
				SetEventMask(EntityEvent.FRAME);

			return;
		}

		ClearEventMask(EntityEvent.FRAME);
		Rpc(RpcDo_LF_WheatCut, packed);
		LF_Debug.Info(FEATURE, string.Format("wheat field %1: sent a cut batch of %2 chars to clients, %3 of %4 cut", GetName(), packed.Length(), m_Grid.CutCount(), m_Grid.CellCount()));
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		if (!Replication.IsServer())
		{
			ClearEventMask(EntityEvent.FRAME);
			return;
		}

		SendDue();
		if (CutBatch().Pending() == 0)
			ClearEventMask(EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_WheatCut(string packed)
	{
		array<int> cells;
		if (!LF_WheatFieldWire.Decode(packed, cells))
		{
			LF_Debug.Error(FEATURE, string.Format("wheat field %1: a cut batch from the server is not valid, dropped (%2 chars)", GetName(), packed.Length()));
			return;
		}

		int newlyCut = CutCells(cells);
		LF_Debug.Info(FEATURE, string.Format("wheat field %1: client got a cut batch of %2 cells, %3 new; %4 stubbles drawn", GetName(), cells.Count(), newlyCut, StubbleCount()));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_WheatRegrow()
	{
		Regrow();
		LF_Debug.Info(FEATURE, string.Format("wheat field %1: client regrew, %2 patches drawn", GetName(), PatchCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! The late-join snapshot: the bitset, after whatever the chain below writes (RplLoad reads it back in order).
	override bool RplSave(ScriptBitWriter writer)
	{
		if (!super.RplSave(writer))
			return false;

		LF_WheatFieldWire.WriteSnapshot(writer, LF_WheatFieldWire.Snapshot(Grid()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		if (!super.RplLoad(reader))
			return false;

		array<int> words;
		if (!LF_WheatFieldWire.ReadSnapshot(reader, words))
		{
			LF_Debug.Error(FEATURE, string.Format("wheat field %1: the join snapshot carries no cut state; the field draws standing until the next cut", GetName()));
			return false;
		}

		Grid().ImportWords(words);
		// EOnInit may already have drawn the field from an empty bitset; draw it again from the real one.
		ShowLocal();
		LF_Debug.Info(FEATURE, string.Format("wheat field %1: join snapshot has %2 of %3 cells cut, %4 stubbles drawn", GetName(), CutCount(), CellCount(), StubbleCount()));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Harness verbs for the session proof (docs/features/harvest.md):
//!   wheatsync state <fieldIndex>             cut, stubble and patch counts this machine shows
//!   wheatsync cut <fieldIndex> <first> <n>   the server cuts n cells from first
//!   wheatsync regrow <fieldIndex>            the server regrows the field
//! A client asks the server through its character (the harness gate allows it on a dev build only).
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "wheatsync")
			return super.Handle(cmd);

		if (cmd.Arg1 == "state")
		{
			LF_WheatField field = LF_WheatField.FindByIndex(cmd.Arg2.ToInt());
			if (!field)
			{
				LF_AgentHarness.Answer(cmd.Id, "error", "no wheat field with that index; usage: wheatsync state|regrow <fieldIndex> | cut <fieldIndex> <first> <n>");
				return true;
			}

			LF_AgentHarness.Answer(cmd.Id, "ok", string.Format("cut=%1 stubble=%2 patches=%3 cells=%4", field.CutCount(), field.StubbleCount(), field.PatchCount(), field.CellCount()));
			return true;
		}

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if ((cmd.Arg1 != "cut" && cmd.Arg1 != "regrow") || !character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: wheatsync state|regrow <fieldIndex> | cut <fieldIndex> <first> <n> (cut and regrow need a character)");
			return true;
		}

		character.LF_AskHarnessWheatSync(cmd.Id, cmd.Arg1, cmd.Arg2.ToInt(), cmd.Arg3.ToInt(), cmd.Arg4.ToInt());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskHarnessWheatSync(int commandId, string action, int fieldIndex, int first, int count)
	{
		if (Replication.IsServer())
		{
			LF_HarnessWheatSyncDo(commandId, action, fieldIndex, first, count);
			return;
		}

		Rpc(RpcAsk_LF_HarnessWheatSync, commandId, action, fieldIndex, first, count);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessWheatSync(int commandId, string action, int fieldIndex, int first, int count)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "wheatsync"))
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		LF_HarnessWheatSyncDo(commandId, action, fieldIndex, first, count);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_HarnessWheatSyncDo(int commandId, string action, int fieldIndex, int first, int count)
	{
		LF_WheatField field = LF_WheatField.FindByIndex(fieldIndex);
		if (!field)
		{
			LF_HarnessReply(commandId, "error", "no wheat field with index " + fieldIndex);
			return;
		}

		if (action == "regrow")
		{
			field.Regrow();
			LF_HarnessReply(commandId, "ok", "regrown");
			return;
		}

		array<int> cells = {};
		for (int i = 0; i < count && i < 1000; i++)
		{
			cells.Insert(first + i);
		}

		int cut = field.CutCells(cells);
		LF_HarnessReply(commandId, "ok", string.Format("cut=%1 total=%2", cut, field.CutCount()));
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_WheatField's field list (LF_StaticReset finds it by reflection).
class LF_WheatField_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_WheatField";
	}

	override void Apply()
	{
		LF_WheatField.ResetStatics();
	}
}
