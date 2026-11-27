// red-proof: with `RedProof()` on the bit table is not asked (every bit keeps its wrong default, the encode is 0, the decode
// is empty), so the position, round trip, unknown-type and Has checks fail (batched --red run 2026-11-27: RED PROOF OK).

//! license/bit-order: the replicated licence mask (LF_LicenseBits). A licence's bit is its place in ORDER, a client decodes a
//! mask with the same table, so an old position moving would hand a late joiner a different licence. This pins the positions
//! of the first and last rung of each branch, the 31-bit ceiling, the encode and decode round trip in ORDER, a type with no
//! bit (refused, never a shifted bit) and `Has`.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LicenceBitOrder : LF_Test
{
	override string GetName()
	{
		return "license/bit-order";
	}

	override void Run(LF_TestContext ctx)
	{
		int unemployed = -9;
		int farmerField = -9;
		int farmerSpeed = -9;
		int minerCoal = -9;
		int lumberjackBasic = -9;
		int medicAccess = -9;
		int policeAccess = -9;
		int criminalWeed = -9;
		int criminalClothes = -9;
		int firearm = -9;
		int driving = -9;
		int unknownBit = 5;
		int orderCount = -1;
		int bits = -1;
		bool hasField = false;
		bool hasDriving = true;
		bool hasUnknown = true;
		int decodedCount = -1;
		LF_ELicenseType decodedFirst = LF_ELicenseType.DRIVING_LICENCE;
		LF_ELicenseType decodedSecond = LF_ELicenseType.UNEMPLOYED;
		int emptyBits = -1;
		int skippedUnknown = -1;
		array<LF_ELicenseType> decoded = {};
		if (!RedProof())
		{
			unemployed = LF_LicenseBits.BitOf(LF_ELicenseType.UNEMPLOYED);
			farmerField = LF_LicenseBits.BitOf(LF_ELicenseType.FARMER_FIELD);
			farmerSpeed = LF_LicenseBits.BitOf(LF_ELicenseType.FARMER_SPEED);
			minerCoal = LF_LicenseBits.BitOf(LF_ELicenseType.MINER_COAL);
			lumberjackBasic = LF_LicenseBits.BitOf(LF_ELicenseType.LUMBERJACK_BASIC);
			medicAccess = LF_LicenseBits.BitOf(LF_ELicenseType.MEDIC_ACCESS);
			policeAccess = LF_LicenseBits.BitOf(LF_ELicenseType.POLICE_ACCESS);
			criminalWeed = LF_LicenseBits.BitOf(LF_ELicenseType.CRIMINAL_WEED);
			criminalClothes = LF_LicenseBits.BitOf(LF_ELicenseType.CRIMINAL_CLOTHES);
			firearm = LF_LicenseBits.BitOf(LF_ELicenseType.FIREARM_PERMIT);
			driving = LF_LicenseBits.BitOf(LF_ELicenseType.DRIVING_LICENCE);
			LF_ELicenseType nobody = 7777;
			unknownBit = LF_LicenseBits.BitOf(nobody);
			orderCount = LF_LicenseBits.Order().Count();

			array<LF_ELicenseType> held = {LF_ELicenseType.DRIVING_LICENCE, LF_ELicenseType.FARMER_FIELD, nobody};
			bits = LF_LicenseBits.Encode(held);
			hasField = LF_LicenseBits.Has(bits, LF_ELicenseType.FARMER_FIELD);
			hasDriving = LF_LicenseBits.Has(bits, LF_ELicenseType.DRIVING_LICENCE);
			hasUnknown = LF_LicenseBits.Has(bits, nobody);
			LF_LicenseBits.Decode(bits, decoded);
			decodedCount = decoded.Count();
			if (decodedCount > 0)
				decodedFirst = decoded[0];

			if (decodedCount > 1)
				decodedSecond = decoded[1];

			array<LF_ELicenseType> none = {};
			emptyBits = LF_LicenseBits.Encode(none);
			array<LF_ELicenseType> onlyUnknown = {nobody};
			skippedUnknown = LF_LicenseBits.Encode(onlyUnknown);
		}

		ctx.Equal(0, unemployed, "the first rung (unemployed) is bit 0");
		ctx.Equal(1, farmerField, "the free field-hand licence is bit 1");
		ctx.Equal(5, farmerSpeed, "the last farmer licence is bit 5");
		ctx.Equal(6, minerCoal, "the first miner licence follows the farmers");
		ctx.Equal(11, lumberjackBasic, "the first lumberjack licence is bit 11");
		ctx.Equal(14, medicAccess, "the medic badge is bit 14");
		ctx.Equal(18, policeAccess, "the police badge is bit 18");
		ctx.Equal(22, criminalWeed, "the first criminal licence is bit 22");
		ctx.Equal(27, criminalClothes, "the last criminal licence is bit 27");
		ctx.Equal(28, firearm, "the firearm permit is bit 28");
		ctx.Equal(29, driving, "the driving licence, the newest, is the last bit used so far");
		ctx.Equal(-1, unknownBit, "a type outside the table has no bit (refused, never a shifted one)");
		ctx.True(orderCount > 0 && orderCount <= 31, "the table fits the 31 usable bits of an int");
		ctx.Equal(30, orderCount, "the table holds 30 licences today; a new one appends and updates this count");

		ctx.Equal((1 << 1) | (1 << 29), bits, "a mask is the OR of its licences' bits; the unknown type adds nothing");
		ctx.True(hasField, "Has finds a licence in the mask");
		ctx.True(hasDriving, "Has finds the last bit");
		ctx.False(hasUnknown, "Has is false for a type with no bit");
		ctx.Equal(2, decodedCount, "the mask decodes to the two licences it holds");
		ctx.Equal(LF_ELicenseType.FARMER_FIELD, decodedFirst, "decode comes out in table order (the field hand first)");
		ctx.Equal(LF_ELicenseType.DRIVING_LICENCE, decodedSecond, "decode comes out in table order (driving last)");
		ctx.Equal(0, emptyBits, "no licences encode to an empty mask");
		ctx.Equal(0, skippedUnknown, "a list of only unknown types encodes to an empty mask");
	}
}
