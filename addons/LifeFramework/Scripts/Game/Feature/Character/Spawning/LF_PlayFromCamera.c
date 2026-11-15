//------------------------------------------------------------------------------------------------
//! Workbench "Play from camera": the editor hands the player a body at the camera before the account
//! flow spawns the real character. LF_SpawnLogic replaces that body (wrong prefab, no account) and used
//! to put the new one on a town spawn point, so every play from camera ended at Spawn_Town. Vanilla
//! SCR_SpawnLogic (RequestPlayerData_S / OnPlayerEntityChanged_S, #ifdef WORKBENCH) keeps the pose of
//! the body Workbench handed over and applies it to the first spawn; this does the same.
//! Only a Workbench build compiles WORKBENCH, so a dedicated server or a published build never takes it.
class LF_PlayFromCamera
{
	//------------------------------------------------------------------------------------------------
	//! True only in a Workbench build.
	static bool IsWorkbench()
	{
#ifdef WORKBENCH
		return true;
#else
		return false;
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! The decision. Pure, so it is LOGIC-tier tested (LF_Test_PlayFromCamera).
	//! \param workbench Running inside Workbench (IsWorkbench()).
	//! \param replacedBody The spawn replaced a body Workbench handed the player.
	//! \param bodyPosition Where that body stood; zero when unknown.
	//! \return true when the new character spawns where the replaced body stood.
	static bool UseCameraPose(bool workbench, bool replacedBody, vector bodyPosition)
	{
		return workbench && replacedBody && bodyPosition != vector.Zero;
	}
}
