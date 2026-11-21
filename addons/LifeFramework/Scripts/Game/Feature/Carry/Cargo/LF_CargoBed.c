// LF_CargoBed.c: the pure rules of a vehicle's cargo bed (#3265). A bed is a stack of units; each kind of
// load (softwood log 1, hardwood log 2, ore sack 1, crate 2) takes its unit size. The component holds the
// stack, this file decides what may go in and how the stack is written down for replication and saves.

enum LF_ECargoRefusal
{
	NONE,
	NO_BED,
	NO_LOAD,
	NOT_ACCEPTED,
	FULL,
	EMPTY,
	TOO_FAR,
	NO_HANDS,
	NO_ITEM_PREFAB,
	LOCKED
}

//------------------------------------------------------------------------------------------------
//! One kind of load a bed accepts. Rows are data in LF_CargoBed.Kinds(); the ore sack and the crate have no
//! prefab yet, so the bed refuses them until the slice that adds them fills the row in.
class LF_CargoKind
{
	string m_sKind;
	int m_iUnits;
	ResourceName m_ItemPrefab;
	ResourceName m_Model;

	//------------------------------------------------------------------------------------------------
	static LF_CargoKind Make(string kind, int units, ResourceName itemPrefab, ResourceName model)
	{
		LF_CargoKind row = new LF_CargoKind();
		row.m_sKind = kind;
		row.m_iUnits = units;
		row.m_ItemPrefab = itemPrefab;
		row.m_Model = model;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
class LF_CargoBed
{
	static const string FEATURE = "Cargo";

	//! How far from the bed a carrier may stand and still load or unload (metres, from the tailgate point).
	static const float REACH = 6.0; // lf-fixed: the tailgate action context is about 2.5 m; this only stops a far-away script call

	protected static ref array<ref LF_CargoKind> s_aKinds;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_CargoKind> Kinds()
	{
		if (!s_aKinds)
		{
			s_aKinds = new array<ref LF_CargoKind>();
			s_aKinds.Insert(LF_CargoKind.Make("log", 1, "{32F5A906D8A573BC}Prefabs/Resources/WoodCutting/WoodLog_01_Small.et", "{8D73FF908E973C20}Assets/Props/Forest/WoodPile_01/Dst/WoodLog_01_C_dst_01.xob"));
			s_aKinds.Insert(LF_CargoKind.Make("hardwood", 2, "{FC7D57CC9C0798D7}Prefabs/Resources/WoodCutting/WoodLog_Hardwood.et", "{4CCEDF1BAD52818C}Assets/Props/Forest/WoodPile_01/WoodLog_01_B.xob"));
			s_aKinds.Insert(LF_CargoKind.Make("ore", 1, "", ""));
			s_aKinds.Insert(LF_CargoKind.Make("crate", 2, "", ""));
		}

		return s_aKinds;
	}

	//------------------------------------------------------------------------------------------------
	//! The index of a kind, or -1.
	static int KindIndex(string kind)
	{
		array<ref LF_CargoKind> kinds = Kinds();
		foreach (int index, LF_CargoKind row : kinds)
		{
			if (row.m_sKind == kind)
				return index;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	static int UnitsOf(int kindIndex)
	{
		array<ref LF_CargoKind> kinds = Kinds();
		if (kindIndex < 0 || kindIndex >= kinds.Count())
			return 0;

		return kinds[kindIndex].m_iUnits;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a load of `units` fits a bed of `capacity` that holds `used`. First refusal wins.
	static LF_ECargoRefusal DecideLoad(int capacity, int used, int units, bool accepted)
	{
		if (capacity <= 0)
			return LF_ECargoRefusal.NO_BED;

		if (units <= 0 || !accepted)
			return LF_ECargoRefusal.NOT_ACCEPTED;

		if (used + units > capacity)
			return LF_ECargoRefusal.FULL;

		return LF_ECargoRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The toast key for a refusal, or "" when the bed says nothing.
	static string RefusalKey(LF_ECargoRefusal refusal)
	{
		switch (refusal)
		{
			case LF_ECargoRefusal.NO_BED: return "#LF-Cargo_NoBed";
			case LF_ECargoRefusal.NO_LOAD: return "#LF-Cargo_NoLoad";
			case LF_ECargoRefusal.NOT_ACCEPTED: return "#LF-Cargo_NotAccepted";
			case LF_ECargoRefusal.FULL: return "#LF-Cargo_Full";
			case LF_ECargoRefusal.EMPTY: return "#LF-Cargo_Empty";
			case LF_ECargoRefusal.NO_HANDS: return "#LF-Cargo_HandsFull";
			case LF_ECargoRefusal.LOCKED: return "#EL-VehicleLock_Reason";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalText(LF_ECargoRefusal refusal)
	{
		switch (refusal)
		{
			case LF_ECargoRefusal.NO_BED: return "the vehicle has no cargo bed";
			case LF_ECargoRefusal.NO_LOAD: return "nothing is carried";
			case LF_ECargoRefusal.NOT_ACCEPTED: return "the bed does not take this load";
			case LF_ECargoRefusal.FULL: return "the bed is full";
			case LF_ECargoRefusal.EMPTY: return "the bed is empty";
			case LF_ECargoRefusal.TOO_FAR: return "too far from the bed";
			case LF_ECargoRefusal.NO_HANDS: return "the unloader's hands are not free";
			case LF_ECargoRefusal.NO_ITEM_PREFAB: return "the kind has no item prefab yet";
			case LF_ECargoRefusal.LOCKED: return "the vehicle is locked";
		}

		return "ok";
	}

	//------------------------------------------------------------------------------------------------
	//! The stack written as "0,0,1,1": one kind index per unit, bottom first. Short, so it replicates and saves
	//! as a string.
	static string Pack(array<int> stack)
	{
		string text;
		foreach (int index, int kind : stack)
		{
			if (index > 0)
				text += ",";

			text += kind.ToString();
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! True for a non-empty run of digits (ToInt turns "x" into 0, which would read as a real kind).
	protected static bool IsNumber(string token)
	{
		if (token.IsEmpty())
			return false;

		for (int i = 0; i < token.Length(); i++)
		{
			string ch = token.Get(i);
			if (ch < "0" || ch > "9")
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The inverse of Pack. A bad token is dropped, so a corrupt save leaves a shorter stack, never a crash.
	static void Unpack(string text, array<int> stack, int capacity)
	{
		stack.Clear();
		if (text.IsEmpty())
			return;

		array<string> tokens = {};
		text.Split(",", tokens, true);
		foreach (string token : tokens)
		{
			if (stack.Count() >= capacity)
				break;

			if (!IsNumber(token))
				continue;

			int kind = token.ToInt();
			if (kind < 0 || kind >= Kinds().Count())
				continue;

			stack.Insert(kind);
		}
	}
}
