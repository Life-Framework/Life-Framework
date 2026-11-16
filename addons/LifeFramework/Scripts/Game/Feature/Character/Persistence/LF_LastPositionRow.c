//------------------------------------------------------------------------------------------------
//! Where one character last stood (#1777, first half of #640). The key is the character's
//! persistence id (LF_PlayerCharacter.GetId), stable for the life of the character. Persisted as
//! the "lastpos" payload; the shape is frozen once it ships (validate-record-shapes).
class LF_LastPositionRow
{
	//! The character's persistence id.
	string m_sCharacterId;
	//! World position of the body when it was taken.
	vector m_vPosition;
	//! Heading (yaw, degrees) of the body when it was taken.
	float m_fYaw;
	//! System.GetUnixTime() when it was taken: on disconnect, or at the save that wrote it.
	int m_iTakenUnix;

	//------------------------------------------------------------------------------------------------
	static LF_LastPositionRow Create(string characterId, vector position, float yaw, int takenUnix)
	{
		LF_LastPositionRow row = new LF_LastPositionRow();
		row.m_sCharacterId = characterId;
		row.m_vPosition = position;
		row.m_fYaw = yaw;
		row.m_iTakenUnix = takenUnix;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	LF_LastPositionRow Copy()
	{
		return Create(m_sCharacterId, m_vPosition, m_fYaw, m_iTakenUnix);
	}
}
