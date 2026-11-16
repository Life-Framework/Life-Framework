// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_OutletSerial`: RedProof() mints with no outlet and `a serial minted for gun_counter parses back to gun_counter` fails (expected gun_counter, got empty); hook off: green.
// tier: LOGIC
//! A sold weapon's serial names the outlet that sold it (#1765, Refs #1067): a serial minted for
//! outlet `gun_counter` parses back to `gun_counter`, an old serial with no prefix parses to no
//! outlet and still resolves in the book, and the booked serial from LF_FenceSerials.Issue carries it.
class LF_Test_OutletSerial : LF_Test
{
	protected static const string BUYER = "outlet-serial-buyer";
	protected static const string OUTLET = "gun_counter";
	protected static const string RIFLE = "{1E06C1BB38AFB556}Prefabs/Items/Police/LF_ArmoryM16.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "catalogue/outlet-serial";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string outlet = OUTLET;
		if (RedProof())
			outlet = "";

		string minted = LF_OutletSerial.Mint(outlet, 7);
		ctx.EqualStr(OUTLET, LF_OutletSerial.Outlet(minted), "a serial minted for gun_counter parses back to gun_counter");

		// An old serial has no prefix: no outlet, and it is still a serial the book resolves.
		ctx.EqualStr("", LF_OutletSerial.Outlet("LF-0001"), "an old serial parses to no outlet");
		ctx.EqualStr("", LF_OutletSerial.Outlet(""), "an empty serial parses to no outlet");
		ctx.EqualStr("", LF_OutletSerial.Outlet("garbage"), "an unknown shape parses to no outlet");
		ctx.EqualStr("LF-0007", LF_OutletSerial.Mint("", 7), "no outlet mints the old shape");
		ctx.EqualStr("LF-0007", LF_OutletSerial.Mint("bad-id", 7), "an outlet id that breaks the shape is left out");

		array<ref LF_FenceSerialRow> before = LF_FenceSerials.ExportAll();
		LF_FenceSerialRow oldRow = new LF_FenceSerialRow();
		oldRow.m_iNumber = 1;
		oldRow.m_sSerial = "LF-0001";
		oldRow.m_sBuyerUid = BUYER;
		oldRow.m_sPrefab = RIFLE;
		array<ref LF_FenceSerialRow> loaded = {};
		loaded.Insert(oldRow);
		LF_FenceSerials.ApplyAll(loaded);
		ctx.NotNull(LF_FenceSerials.Find("LF-0001"), "an old serial with no prefix still loads");

		string booked = LF_FenceSerials.Issue(BUYER, RIFLE, OUTLET);
		ctx.EqualStr(OUTLET, LF_OutletSerial.Outlet(booked), "the booked serial names the outlet");
		LF_FenceSerialRow row = LF_FenceSerials.Find(booked);
		ctx.NotNull(row, "the prefixed serial is in the book");
		if (row)
			ctx.EqualStr(BUYER, row.m_sBuyerUid, "the prefixed serial still names its buyer");
		ctx.True(booked != "LF-0001", "the new serial does not reuse the old number");

		LF_FenceSerials.ApplyAll(before);
	}
}
