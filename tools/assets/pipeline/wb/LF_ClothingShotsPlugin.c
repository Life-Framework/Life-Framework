// LF_ClothingShotsPlugin.c - pictures of a garment worn, posed and on the ground (tools\cli asset clothing shots).
//
// tools/assets/pipeline/clothing-shots.mjs copies this file into the checkout's git-ignored
// Scripts/WorkbenchGame/LFClothing/ and starts a hidden Workbench with
//   -wbModule=WorldEditor -plugin=LF_ClothingShotsPlugin -forceUpdate -profile <dir>
// RunCommandline opens the world, enters play mode with a free camera (the way
// tools/visual/wb/LF_VisualShotsPlugin.c does), then runs $profile:lf-clothing/plan.txt top to
// bottom and writes the viewport with System.MakeScreenshot for every `shot`.
//
// plan.txt, one directive per line:
//   world <resource path>                 opened when no world is loaded
//   hour <0-24>                           local solar time, clear weather
//   park <x> <z>                          where the editor camera waits while play starts
//   first <ms>                            extra wait before the first shot (streaming, shaders)
//   spawn <x> <z> <yawDeg> <prefab>       the mannequin: a character prefab, standing on the ground
//   wear <prefab>                         spawn a garment and put it on the mannequin
//                                         (SCR_InventoryStorageManagerComponent.EquipCloth, the
//                                         same call the Equip action makes)
//   stance erect|crouch|prone             CharacterControllerComponent.SetStanceChange
//   move <speed>                          0 stops; above 0 walks/runs forward (SetMovement, re-sent
//                                         every frame of every wait, because the input context resets it)
//   wait <ms>
//   shot <name> <az> <dist> <up> <look> <fov>
//                                         camera `dist` m from the mannequin on heading az relative to
//                                         its facing (0 = in front, 90 = its left side, 180 = behind),
//                                         `up` m above its feet, aimed at `look` m above its feet
//   despawn                               delete the mannequin and what it wears
//   drop <x> <z> <yawDeg> <prefab>        spawn an item lying on the ground
//   groundshot <name> <az> <dist> <up> <fov>
//                                         camera around the last dropped item, aimed at its bounds centre
//   clear                                 delete dropped items
// status.txt gets one line per step (`<ms> ok shot <name> <file> cam=..`, `<ms> error <reason>`,
// `<ms> ok wear <prefab> slot=<area>`) and a final `done <shots> <failed>`.

[WorkbenchPluginAttribute(name: "LF Clothing Shots", description: "Takes the shots of $profile:lf-clothing/plan.txt (tools\\cli asset clothing)", wbModules: { "WorldEditor" }, category: "Life Framework")]
class LF_ClothingShotsPlugin : WorldEditorPlugin
{
	protected static const string DIR = "$profile:lf-clothing";
	protected static const int SHOT_TIMEOUT_MS = 5000;
	protected static const int PLAY_TIMEOUT_MS = 90000;
	protected static const int TICK_MS = 20;

	protected ref array<string> m_aStatus = {};
	protected ref array<IEntity> m_aDropped = {};
	protected int m_iStart;
	protected WorldEditorAPI m_Api;
	protected BaseWorld m_World;
	protected CameraBase m_Camera;
	protected IEntity m_Mannequin;
	protected IEntity m_LastDrop;
	protected float m_fHour = -1;
	protected float m_fMove;
	protected bool m_bFirst = true;
	protected int m_iFirstMs = 3000;
	protected int m_iShots;
	protected int m_iFailed;

	//------------------------------------------------------------------------------------------------
	override void RunCommandline()
	{
		m_iStart = System.GetTickCount();
		ShootPlan();
		Note(string.Format("done %1 %2", m_iShots, m_iFailed));
		int code = 0;
		if (m_iFailed != 0)
			code = 1;

		Workbench.Exit(code);
	}

	//------------------------------------------------------------------------------------------------
	protected void Note(string line)
	{
		string row = string.Format("%1 %2", System.GetTickCount() - m_iStart, line);
		m_aStatus.Insert(row);
		Print("[LFClothing] " + row, LogLevel.NORMAL);

		// Rewritten after every step, so a killed run still says how far it got.
		FileHandle f = FileIO.OpenFile(DIR + "/status.txt", FileMode.WRITE);
		if (!f)
			return;

		foreach (string saved : m_aStatus)
		{
			f.WriteLine(saved);
		}

		f.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void Fail(string reason)
	{
		m_iFailed++;
		Note("error " + reason);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShootPlan()
	{
		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		if (!worldEditor)
		{
			Fail("no_world_editor");
			return;
		}

		FileHandle plan = FileIO.OpenFile(DIR + "/plan.txt", FileMode.READ);
		if (!plan)
		{
			Fail("no_plan " + DIR + "/plan.txt");
			return;
		}

		array<string> lines = {};
		string line;
		while (plan.ReadLine(line) >= 0)
		{
			line.TrimInPlace();
			if (!line.IsEmpty() && !line.StartsWith("#"))
				lines.Insert(line);
		}

		plan.Close();

		string world;
		string park;
		foreach (string l : lines)
		{
			array<string> p = {};
			l.Split(" ", p, true);
			if (p[0] == "world" && p.Count() > 1)
				world = p[1];
			else if (p[0] == "park" && p.Count() > 2)
				park = p[1] + " " + p[2];
			else if (p[0] == "hour" && p.Count() > 1)
				m_fHour = p[1].ToFloat();
			else if (p[0] == "first" && p.Count() > 1)
				m_iFirstMs = p[1].ToInt();
		}

		m_Api = worldEditor.GetApi();
		string opened;
		if (m_Api)
			m_Api.GetWorldPath(opened);

		if (opened.IsEmpty())
		{
			if (world.IsEmpty() || !worldEditor.SetOpenedResource(world))
			{
				Fail("world_not_opened " + world);
				return;
			}

			m_Api = worldEditor.GetApi();
		}

		if (!m_Api || !m_Api.GetWorld())
		{
			Fail("no_world_api");
			return;
		}

		m_World = m_Api.GetWorld();
		m_Api.GetWorldPath(opened);
		Note(string.Format("ok world %1 view=%2x%3", opened, m_Api.GetScreenWidth(), m_Api.GetScreenHeight()));
		if (!EnterPlay(worldEditor, park))
			return;

		SetHour();
		foreach (string step : lines)
		{
			Step(step);
		}

		Despawn();
		Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected bool EnterPlay(notnull WorldEditor worldEditor, string park)
	{
		array<string> p = {};
		park.Split(" ", p, true);
		if (p.Count() >= 2)
		{
			float px = p[0].ToFloat();
			float pz = p[1].ToFloat();
			m_Api.SetCamera(Vector(px, m_World.GetSurfaceY(px, pz) + 2, pz), "0 -0.5 0.86");
			Sleep(100);
		}

		worldEditor.SwitchToGameMode();
		int waited;
		while (!m_Api.IsGameMode())
		{
			if (waited > PLAY_TIMEOUT_MS)
			{
				Fail("play_not_started");
				return false;
			}

			Sleep(100);
			waited += 100;
		}

		ArmaReforgerScripted game = GetGame();
		while (!game || !game.GetWorld() || !game.GetCameraManager())
		{
			if (waited > PLAY_TIMEOUT_MS)
			{
				Fail("play_no_world");
				return false;
			}

			Sleep(100);
			waited += 100;
			game = GetGame();
		}

		m_World = game.GetWorld();
		m_Camera = CameraBase.Cast(game.SpawnEntity(CameraBase));
		if (!m_Camera)
		{
			Fail("play_no_camera");
			return false;
		}

		game.GetCameraManager().SetCamera(m_Camera);
		Note("ok play waited=" + waited);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetHour()
	{
		if (m_fHour < 0)
			return;

		ChimeraWorld world = ChimeraWorld.CastFrom(m_World);
		TimeAndWeatherManagerEntity clock;
		if (world)
			clock = world.GetTimeAndWeatherManager();

		if (!clock)
		{
			Note("warn no_clock");
			return;
		}

		float game = m_fHour - clock.GetTimeZoneOffset() - clock.GetCurrentLongitude() / 15;
		while (game < 0)
			game += 24;

		while (game >= 24)
			game -= 24;

		clock.SetTimeOfTheDay(game, true);
		clock.ForceWeatherTo(clock.IsWeatherLooping(), "Clear", 0, 3600);
	}

	//------------------------------------------------------------------------------------------------
	protected void Step(string line)
	{
		array<string> p = {};
		line.Split(" ", p, true);
		string verb = p[0];
		if (verb == "spawn" && p.Count() >= 5)
			Spawn(p[1].ToFloat(), p[2].ToFloat(), p[3].ToFloat(), p[4]);
		else if (verb == "wear" && p.Count() >= 2)
			Wear(p[1]);
		else if (verb == "stance" && p.Count() >= 2)
			Stance(p[1]);
		else if (verb == "move" && p.Count() >= 2)
			m_fMove = p[1].ToFloat();
		else if (verb == "wait" && p.Count() >= 2)
			Hold(p[1].ToInt());
		else if (verb == "shot" && p.Count() >= 7)
			Shot(p[1], p[2].ToFloat(), p[3].ToFloat(), p[4].ToFloat(), p[5].ToFloat(), p[6].ToFloat());
		else if (verb == "despawn")
			Despawn();
		else if (verb == "drop" && p.Count() >= 5)
			Drop(p[1].ToFloat(), p[2].ToFloat(), p[3].ToFloat(), p[4]);
		else if (verb == "groundshot" && p.Count() >= 6)
			GroundShot(p[1], p[2].ToFloat(), p[3].ToFloat(), p[4].ToFloat(), p[5].ToFloat());
		else if (verb == "clear")
			Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Wait `ms` of wall-clock time in frame-sized ticks, re-sending the mannequin's movement each tick.
	//! Counted on the tick clock: a plugin's Sleep(20) returns after one frame, far sooner than 20 ms
	//! (measured 2026-11-14: 2000 ms of Sleep(20) calls took 6 ms), so prone and run were never reached.
	protected void Hold(int ms)
	{
		int start = System.GetTickCount();
		int ticks;
		while (System.GetTickCount() - start < ms)
		{
			Pump();
			Sleep(TICK_MS);
			ticks++;
		}

		if (ms >= 1000)
			Note(string.Format("ok hold %1 took=%2 ticks=%3", ms, System.GetTickCount() - start, ticks));
	}

	//------------------------------------------------------------------------------------------------
	protected void Pump()
	{
		if (!m_Mannequin || m_fMove <= 0)
			return;

		CharacterControllerComponent ctrl = CharacterControllerComponent.Cast(m_Mannequin.FindComponent(CharacterControllerComponent));
		if (ctrl)
			ctrl.SetMovement(m_fMove, "0 0 1");
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAt(string prefab, float x, float z, float yawDeg)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return null;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		vector angles = Vector(yawDeg, 0, 0);
		Math3D.AnglesToMatrix(angles, params.Transform);
		params.Transform[3] = Vector(x, m_World.GetSurfaceY(x, z), z);
		return GetGame().SpawnEntityPrefab(res, m_World, params);
	}

	//------------------------------------------------------------------------------------------------
	protected void Spawn(float x, float z, float yawDeg, string prefab)
	{
		Despawn();
		m_fMove = 0;
		m_Mannequin = SpawnAt(prefab, x, z, yawDeg);
		if (!m_Mannequin)
		{
			Fail("spawn_failed " + prefab);
			return;
		}

		Hold(1500);
		Note("ok spawn " + prefab);
	}

	//------------------------------------------------------------------------------------------------
	protected void Wear(string prefab)
	{
		if (!m_Mannequin)
		{
			Fail("wear_without_mannequin " + prefab);
			return;
		}

		SCR_InventoryStorageManagerComponent inv = SCR_InventoryStorageManagerComponent.Cast(m_Mannequin.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inv)
		{
			Fail("mannequin_has_no_inventory");
			return;
		}

		vector pos = m_Mannequin.GetOrigin();
		IEntity cloth = SpawnAt(prefab, pos[0] + 1, pos[2] + 1, 0);
		if (!cloth)
		{
			Fail("garment_spawn_failed " + prefab);
			return;
		}

		BaseLoadoutClothComponent loadout = BaseLoadoutClothComponent.Cast(cloth.FindComponent(BaseLoadoutClothComponent));
		string area = "none";
		if (loadout && loadout.GetAreaType())
			area = loadout.GetAreaType().Type().ToString();

		// What the slot holds now is deleted first: EquipCloth would drop it at the mannequin's feet, into the picture.
		EquipedLoadoutStorageComponent worn = EquipedLoadoutStorageComponent.Cast(m_Mannequin.FindComponent(EquipedLoadoutStorageComponent));
		if (worn && loadout && loadout.GetAreaType())
		{
			IEntity old = worn.GetClothFromArea(loadout.GetAreaType().Type());
			if (old)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(old);
				Hold(500);
			}
		}

		inv.EquipCloth(cloth);
		Hold(2000);

		bool on = false;
		if (worn && loadout && loadout.GetAreaType())
			on = worn.GetClothFromArea(loadout.GetAreaType().Type()) == cloth;

		if (!on)
		{
			Fail(string.Format("not_worn %1 slot=%2", prefab, area));
			return;
		}

		Note(string.Format("ok wear %1 slot=%2", prefab, area));
	}

	//------------------------------------------------------------------------------------------------
	protected void Stance(string stance)
	{
		if (!m_Mannequin)
			return;

		CharacterControllerComponent ctrl = CharacterControllerComponent.Cast(m_Mannequin.FindComponent(CharacterControllerComponent));
		if (!ctrl)
		{
			Fail("mannequin_has_no_controller");
			return;
		}

		// SetStanceChange asks for the transition the player's key makes (STANCECHANGE_*: 1 erect, 2 crouch, 3 prone).
		int change = 1;
		if (stance == "crouch")
			change = 2;
		else if (stance == "prone")
			change = 3;

		ctrl.SetStanceChange(change);
		Note(string.Format("ok stance %1", stance));
	}

	//------------------------------------------------------------------------------------------------
	protected void Despawn()
	{
		if (!m_Mannequin)
			return;

		SCR_EntityHelper.DeleteEntityAndChildren(m_Mannequin);
		m_Mannequin = null;
		m_fMove = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void Drop(float x, float z, float yawDeg, string prefab)
	{
		IEntity item = SpawnAt(prefab, x, z, yawDeg);
		if (!item)
		{
			Fail("drop_failed " + prefab);
			return;
		}

		m_aDropped.Insert(item);
		m_LastDrop = item;
		Hold(1500);
		Note("ok drop " + prefab);
	}

	//------------------------------------------------------------------------------------------------
	protected void Clear()
	{
		foreach (IEntity e : m_aDropped)
		{
			if (e)
				SCR_EntityHelper.DeleteEntityAndChildren(e);
		}

		m_aDropped.Clear();
		m_LastDrop = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void Shot(string name, float azDeg, float dist, float up, float look, float fov)
	{
		if (!m_Mannequin)
		{
			Fail("shot_without_mannequin " + name);
			return;
		}

		// The first wait also lets the game mode set its own clock; set ours again after it.
		if (m_bFirst)
		{
			Hold(m_iFirstMs);
			SetHour();
			Hold(1500);
			m_bFirst = false;
		}

		// Aimed twice: a running mannequin moves during the settle, so the second aim follows it.
		vector cam = AimAtMannequin(azDeg, dist, up, look, fov);
		Hold(400);
		cam = AimAtMannequin(azDeg, dist, up, look, fov);
		Capture(name, cam);
	}

	//------------------------------------------------------------------------------------------------
	//! Put the camera `dist` m from the mannequin on heading az relative to its facing; returns the camera position.
	protected vector AimAtMannequin(float azDeg, float dist, float up, float look, float fov)
	{
		vector mat[4];
		m_Mannequin.GetWorldTransform(mat);
		vector fwd = mat[2];
		fwd[1] = 0;
		if (fwd.Length() < 0.01)
			fwd = "0 0 1";

		fwd.Normalize();
		float heading = Math.Atan2(fwd[0], fwd[2]) + azDeg * Math.DEG2RAD;
		vector feet = mat[3];
		vector cam = feet + Vector(Math.Sin(heading) * dist, up, Math.Cos(heading) * dist);
		Place(cam, feet + Vector(0, look, 0), fov);
		return cam;
	}

	//------------------------------------------------------------------------------------------------
	protected void GroundShot(string name, float azDeg, float dist, float up, float fov)
	{
		if (!m_LastDrop)
		{
			Fail("groundshot_without_item " + name);
			return;
		}

		if (m_bFirst)
		{
			Hold(m_iFirstMs);
			SetHour();
			Hold(1500);
			m_bFirst = false;
		}

		vector mins, maxs;
		m_LastDrop.GetWorldBounds(mins, maxs);
		vector target = (mins + maxs) * 0.5;
		float az = azDeg * Math.DEG2RAD;
		vector cam = target + Vector(Math.Sin(az) * dist, up, Math.Cos(az) * dist);
		vector size = maxs - mins;
		Place(cam, target, fov);
		Hold(400);
		Place(cam, target, fov);
		Capture(name + string.Format(" size=%1,%2,%3", size[0], size[1], size[2]), cam);
	}

	//------------------------------------------------------------------------------------------------
	protected void Place(vector cam, vector target, float fov)
	{
		vector dir = target - cam;
		dir.Normalize();
		vector right = Vector(dir[2], 0, -dir[0]);
		if (right.Length() < 0.02)
			right = "1 0 0";

		right.Normalize();
		vector up = Vector(dir[1] * right[2] - dir[2] * right[1], dir[2] * right[0] - dir[0] * right[2], dir[0] * right[1] - dir[1] * right[0]);
		vector mat[4];
		mat[0] = right;
		mat[1] = up;
		mat[2] = dir;
		mat[3] = cam;
		m_Camera.SetTransform(mat);
		m_Camera.SetVerticalFOV(fov);
	}

	//------------------------------------------------------------------------------------------------
	//! Write the viewport as it is now. \param label the shot name, optionally followed by " key=value" details for the status line
	protected void Capture(string label, vector cam)
	{
		array<string> bits = {};
		label.Split(" ", bits, true);
		string name = bits[0];
		string detail = label.Substring(name.Length(), label.Length() - name.Length());
		m_iShots++;
		Sleep(60);

		string path = DIR + "/" + name;
		System.MakeScreenshot(path);
		int waited;
		string found;
		while (waited < SHOT_TIMEOUT_MS)
		{
			Pump();
			Sleep(50);
			waited += 50;
			if (FileIO.FileExists(path + ".png"))
				found = path + ".png";
			else if (FileIO.FileExists(path + ".bmp"))
				found = path + ".bmp";

			if (!found.IsEmpty())
				break;
		}

		if (found.IsEmpty())
		{
			Fail("no_file " + name);
			return;
		}

		Note(string.Format("ok shot %1 %2 cam=%3,%4,%5%6", name, FilePath.StripPath(found), cam[0], cam[1], cam[2], detail));
	}
}
