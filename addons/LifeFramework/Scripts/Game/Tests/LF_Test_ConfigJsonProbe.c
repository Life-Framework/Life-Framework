// red-proof: make LF_ConfigReader.KindOf return OBJECT where StartArray succeeds and
// config/json-probe fails "KindOf: list (expected 4, got 5)" (observed 2026-11-12). The engine
// facts themselves were observed first in a log-only run of this probe.

//------------------------------------------------------------------------------------------------
//! A record shape for the engine class-reader probe.
class LF_Test_ConfigProbeRule
{
	string find;
	int buy = 100;
	int sell = 50;
};

//------------------------------------------------------------------------------------------------
//! Pins how SCR_JsonLoadContext treats the four inputs the config service depends on (#278):
//! a wrong type, an unknown key, a missing key, and a list of keyed records. The answers decide
//! that the engine parses and LF_ConfigReader reads key by key (ReadValue, DoesKeyExist, KindOf)
//! instead of handing a section to the engine's class reader, and that records are an object
//! keyed by their stable key. If an engine update changes any of them, this goes red first.
// tier: LOGIC
class LF_Test_ConfigJsonProbe : LF_Test
{
	override string GetName()
	{
		return "config/json-probe";
	}

	override void Run(LF_TestContext ctx)
	{
		// Wrong type: ReadValue refuses, leaves the target untouched, and the context stays usable.
		SCR_JsonLoadContext wrong = new SCR_JsonLoadContext(false);
		ctx.True(wrong.ImportFromString("{\"price\":\"abc\",\"name\":42,\"flag\":\"yes\",\"after\":9,\"num\":\"12\",\"half\":12.5,\"whole\":12.0}"), "wrong-type document parses");
		int price = 7;
		ctx.False(wrong.ReadValue("price", price), "string into int is refused");
		ctx.Equal(7, price, "a refused int keeps its value");
		string name = "init";
		ctx.False(wrong.ReadValue("name", name), "number into string is refused");
		ctx.EqualStr("init", name, "a refused string keeps its value");
		bool flag = true;
		ctx.False(wrong.ReadValue("flag", flag), "string into bool is refused");
		int num = 7;
		ctx.False(wrong.ReadValue("num", num), "a numeric string is not coerced to int");
		int after = 7;
		ctx.True(wrong.ReadValue("after", after), "a later good key still reads after refusals");
		ctx.Equal(9, after, "the later key has its value");
		int half = 7;
		ctx.False(wrong.ReadValue("half", half), "12.5 into int is refused");
		int whole = 7;
		ctx.False(wrong.ReadValue("whole", whole), "12.0 into int is refused too");

		// Unknown key: ignored by ReadValue, visible to DoesKeyExist, enumerable through StartMap.
		SCR_JsonLoadContext unknown = new SCR_JsonLoadContext(false);
		ctx.True(unknown.ImportFromString("{\"price\":5,\"bogus\":1,\"shops\":{\"a\":1,\"b\":\"x\",\"c\":{\"d\":2}}}"), "unknown-key document parses");
		price = 7;
		ctx.True(unknown.ReadValue("price", price), "a known key reads beside an unknown one");
		ctx.Equal(5, price, "the known key has its value");
		ctx.True(unknown.DoesKeyExist("bogus"), "DoesKeyExist sees the unknown key");
		int mapCount = -1;
		ctx.True(unknown.StartMap("shops", mapCount), "an object opens as a map");
		ctx.Equal(3, mapCount, "the map counts every key");
		string firstKey;
		string lastKey;
		unknown.ReadMapKey(0, firstKey);
		unknown.ReadMapKey(2, lastKey);
		unknown.EndMap();
		ctx.EqualStr("a", firstKey, "ReadMapKey enumerates keys in order");
		ctx.EqualStr("c", lastKey, "ReadMapKey reaches the last key");
		ctx.True(unknown.StartObject("shops"), "the same object opens as an object");
		int a = -1;
		ctx.True(unknown.ReadValue("a", a), "a key inside the section reads");
		ctx.Equal(1, a, "the section key has its value");
		unknown.EndObject();
		price = 7;
		ctx.True(unknown.ReadValue("price", price), "EndObject returns to the root");

		// Missing key: refused, value untouched, DoesKeyExist false; a missing object does not move the cursor.
		SCR_JsonLoadContext missing = new SCR_JsonLoadContext(false);
		ctx.True(missing.ImportFromString("{\"price\":5}"), "missing-key document parses");
		int miss = 7;
		ctx.False(missing.ReadValue("missing", miss), "a missing key is refused");
		ctx.Equal(7, miss, "a missing key keeps the value");
		ctx.False(missing.DoesKeyExist("missing"), "DoesKeyExist is false for a missing key");
		int missDefault = 7;
		missing.ReadValueDefault("missing", missDefault, 11);
		ctx.Equal(11, missDefault, "ReadValueDefault supplies its default");
		ctx.False(missing.StartObject("nothere"), "a missing object does not open");
		price = 7;
		ctx.True(missing.ReadValue("price", price), "the cursor is unchanged after a missing object");

		// A list of keyed records, read by hand: one record per StartObject, each field independent.
		string rulesJson = "{\"rules\":[{\"find\":\"Food\",\"buy\":10},{\"find\":\"Seeds\",\"sell\":3,\"extra\":1},{\"find\":\"Bad\",\"buy\":\"oops\",\"sell\":4}]}";
		SCR_JsonLoadContext manual = new SCR_JsonLoadContext(false);
		ctx.True(manual.ImportFromString(rulesJson), "records document parses");
		int count = -1;
		ctx.True(manual.StartArray("rules", count), "a list opens");
		ctx.Equal(3, count, "the list counts its records");
		array<string> finds = {};
		array<int> buys = {};
		array<int> sells = {};
		for (int r = 0; r < count; r++)
		{
			manual.StartObject();
			string find;
			int buy = -1;
			int sell = -1;
			manual.ReadValue("find", find);
			manual.ReadValue("buy", buy);
			manual.ReadValue("sell", sell);
			manual.EndObject();
			finds.Insert(find);
			buys.Insert(buy);
			sells.Insert(sell);
		}
		ctx.True(manual.EndArray(), "the list closes");
		ctx.EqualStr("Seeds", finds[1], "records are read in order");
		ctx.Equal(10, buys[0], "a present field reads");
		ctx.Equal(-1, sells[0], "a missing field in a record is left alone");
		ctx.Equal(-1, buys[2], "a wrong-type field in a record is refused");
		ctx.Equal(4, sells[2], "the rest of that record still reads");

		// The engine class reader: it cannot tell missing from default, and it silently drops a
		// wrong-type field while reporting success. That is why the service reads by hand.
		SCR_JsonLoadContext engine = new SCR_JsonLoadContext(false);
		engine.ImportFromString(rulesJson);
		array<ref LF_Test_ConfigProbeRule> rules = {};
		ctx.True(engine.ReadValue("rules", rules), "the class reader reports success");
		ctx.Equal(3, rules.Count(), "the class reader builds every record");
		ctx.Equal(50, rules[0].sell, "a missing field takes the class initializer, not a lower layer");
		ctx.Equal(100, rules[2].buy, "a wrong-type field silently takes the initializer");

		// Anonymous list elements cannot be type-checked: a scalar element never reads, a string
		// element opens as an empty object, and a key list inside an anonymous record does not open.
		// Not asserted because it kills the process: StartObject() on a number element, and the
		// class reader on a list holding a string, both end in "ENGINE (F): Crashed". So records
		// are an object keyed by their stable key, never a list.
		SCR_JsonLoadContext anon = new SCR_JsonLoadContext(false);
		anon.ImportFromString("{\"scalars\":[5,7],\"recs\":[\"x\",{\"find\":\"F\",\"z\":1}]}");
		int anonCount;
		anon.StartArray("scalars", anonCount);
		int element = -1;
		ctx.False(anon.ReadValue(string.Empty, element), "an anonymous number element does not read");
		anon.EndArray();
		anon.StartArray("recs", anonCount);
		ctx.True(anon.StartObject(), "a string element opens as an object (so StartObject is no type check)");
		anon.EndObject();
		anon.StartObject();
		int recKeys = -1;
		ctx.False(anon.StartMap(string.Empty, recKeys), "keys inside an anonymous record cannot be listed");
		anon.EndObject();
		anon.EndArray();

		// Named values can be classified with calls that never crash (LF_ConfigReader.KindOf).
		SCR_JsonLoadContext kinds = new SCR_JsonLoadContext(false);
		kinds.ImportFromString("{\"str\":\"s\",\"t\":true,\"nul\":null,\"arr\":[1],\"obj\":{\"a\":1},\"num\":5}");
		ctx.True(kinds.StartObject("str"), "StartObject opens a string");
		kinds.EndObject();
		ctx.True(kinds.StartObject("t"), "StartObject opens a bool");
		kinds.EndObject();
		ctx.True(kinds.StartObject("nul"), "StartObject opens null");
		kinds.EndObject();
		ctx.True(kinds.StartObject("arr"), "StartObject opens a list");
		kinds.EndObject();
		int arrCount;
		ctx.False(kinds.StartArray("num", arrCount), "StartArray refuses a number");
		ctx.False(kinds.StartArray("obj", arrCount), "StartArray refuses an object");
		ctx.False(kinds.StartArray("nul", arrCount), "StartArray refuses null");
		ctx.True(kinds.StartArray("arr", arrCount), "StartArray opens a list");
		kinds.EndArray();
		string nullString = "init";
		ctx.True(kinds.ReadValue("nul", nullString), "null reads as a string");
		ctx.Equal(LF_EConfigJsonKind.NUMBER, LF_ConfigReader.KindOf(kinds, "num"), "KindOf: number");
		ctx.Equal(LF_EConfigJsonKind.BOOL, LF_ConfigReader.KindOf(kinds, "t"), "KindOf: bool");
		ctx.Equal(LF_EConfigJsonKind.STRING, LF_ConfigReader.KindOf(kinds, "str"), "KindOf: string");
		ctx.Equal(LF_EConfigJsonKind.STRING, LF_ConfigReader.KindOf(kinds, "nul"), "KindOf: null counts as a string");
		ctx.Equal(LF_EConfigJsonKind.LIST, LF_ConfigReader.KindOf(kinds, "arr"), "KindOf: list");
		ctx.Equal(LF_EConfigJsonKind.OBJECT, LF_ConfigReader.KindOf(kinds, "obj"), "KindOf: object");
		ctx.Equal(LF_EConfigJsonKind.ABSENT, LF_ConfigReader.KindOf(kinds, "none"), "KindOf: absent");
		int afterKinds = -1;
		ctx.True(kinds.StartObject("obj") && kinds.ReadValue("a", afterKinds), "KindOf leaves the cursor where it was");
		kinds.EndObject();
		ctx.Equal(1, afterKinds, "the object reads after KindOf");

		// Records keyed by their stable key: the keys list in file order and each record opens by name.
		SCR_JsonLoadContext keyed = new SCR_JsonLoadContext(false);
		keyed.ImportFromString("{\"rules\":{\"Prefabs/Items/Food/\":{\"buy\":10},\"type:64,mode:2\":{\"buy\":3}}}");
		int keyCount = -1;
		ctx.True(keyed.StartMap("rules", keyCount), "a records object lists its keys");
		string key0;
		string key1;
		keyed.ReadMapKey(0, key0);
		keyed.ReadMapKey(1, key1);
		keyed.EndMap();
		ctx.EqualStr("Prefabs/Items/Food/", key0, "a key with slashes lists as written");
		ctx.EqualStr("type:64,mode:2", key1, "a key with a colon and comma lists as written");
		keyed.StartObject("rules");
		ctx.Equal(LF_EConfigJsonKind.OBJECT, LF_ConfigReader.KindOf(keyed, key0), "a record is classified by its key");
		keyed.StartObject(key0);
		int keyedBuy = -1;
		keyed.ReadValue("buy", keyedBuy);
		keyed.EndObject();
		keyed.EndObject();
		ctx.Equal(10, keyedBuy, "a record opens and reads by its key");

		// String lists: the class reader is safe on these but partial, and null replaces the target.
		SCR_JsonLoadContext lists = new SCR_JsonLoadContext(false);
		lists.ImportFromString("{\"good\":[\"a\",\"b\"],\"mixed\":[\"a\",5],\"objs\":[{\"a\":1}],\"nul\":null}");
		array<string> good = {};
		ctx.True(lists.ReadValue("good", good), "a string list reads");
		ctx.Equal(2, good.Count(), "a string list has every element");
		array<string> mixed = {};
		ctx.False(lists.ReadValue("mixed", mixed), "a list with a number is refused");
		ctx.Equal(1, mixed.Count(), "a refused string list is filled up to the bad element");
		array<string> objs = {};
		ctx.False(lists.ReadValue("objs", objs), "a list of objects is refused as strings");
		array<string> nulled = {};
		ctx.True(lists.ReadValue("nul", nulled), "null reads as a string list");
		ctx.True(nulled == null, "and replaces the target with null");

		// Malformed document.
		SCR_JsonLoadContext broken = new SCR_JsonLoadContext(false);
		ctx.False(broken.ImportFromString("{\"price\":5,"), "a malformed document is refused at import");
	}
};
