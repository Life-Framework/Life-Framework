// LF_BackRoomDoor.c - what the back room's door says to the player (#933).
// Today the room has no gate of its own (no hour, no standing, no key): it is open to anyone with
// the cash, and shut only for a while after a police search (#600). The door says exactly that.
// Pure wording, so a LOGIC test can drive it; LF_ShopAction applies it to the prompt, and
// LF_ShopComponent replicates the shutter so a client sees "shut" before it walks up and knocks.

class LF_BackRoomDoor
{
	//------------------------------------------------------------------------------------------------
	//! Prompt on the back room's counter. Empty for an ordinary shop: its own label stands.
	static string LabelKey(bool isBackRoom, bool shut)
	{
		if (!isBackRoom)
			return string.Empty;

		if (shut)
			return "#LF-BackRoom_DoorShut";

		return "#LF-Shop_Open_BackRoom";
	}

	//------------------------------------------------------------------------------------------------
	//! Why the door will not open, under the prompt. Empty when it will.
	static string ReasonKey(bool isBackRoom, bool shut)
	{
		if (isBackRoom && shut)
			return "#LF-Shop_RefuseClosed";

		return string.Empty;
	}
}
