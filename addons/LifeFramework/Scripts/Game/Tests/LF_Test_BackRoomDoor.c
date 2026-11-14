// red-proof: 2026-11-14 `tools\cli test --no-build --only LF_Test_BackRoomDoor` with the shut label changed to the open one failed `a searched back room reads as shut (expected '#LF-BackRoom_DoorShut', got '#LF-Shop_Open_BackRoom')`. Restored; green.
// tier: LOGIC
//! Back room door (#933): the prompt names the room and, once police have searched it, says it is
//! shut and why; the shut refusal tells the player what to do (come back later).
class LF_Test_BackRoomDoor : LF_Test
{
	override string GetName()
	{
		return "shop/back-room-door";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("#LF-Shop_Open_BackRoom", LF_BackRoomDoor.LabelKey(true, false), "an open back room reads as the knock prompt");
		ctx.EqualStr("#LF-BackRoom_DoorShut", LF_BackRoomDoor.LabelKey(true, true), "a searched back room reads as shut");
		ctx.EqualStr("", LF_BackRoomDoor.LabelKey(false, true), "an ordinary shop keeps its own label");
		ctx.EqualStr("", LF_BackRoomDoor.ReasonKey(true, false), "an open back room has no refusal");
		ctx.EqualStr("#LF-Shop_RefuseClosed", LF_BackRoomDoor.ReasonKey(true, true), "a shut back room says why");
		ctx.EqualStr("", LF_BackRoomDoor.ReasonKey(false, true), "an ordinary shop never reads as raided");

		// The till slip and the prompt give the same reason, and the reason is a key that resolves.
		ctx.EqualStr("#LF-Shop_RefuseClosed", LF_ShopRefusal.Key(LF_EShopRefusal.CLOSED), "the slip refusal for a shut room");
		ctx.EqualStr("#LF-Shop_RefuseNotTrusted", LF_ShopRefusal.Key(LF_EShopRefusal.NOT_TRUSTED), "the fence refusal is still its own line");
		ctx.Equal(LF_EShopRefusal.CLOSED, LF_ShopRefusal.FromWire(LF_EShopRefusal.CLOSED), "a closed reason survives the wire");
	}
}
