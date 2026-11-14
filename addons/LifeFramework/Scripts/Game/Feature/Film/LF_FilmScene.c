//------------------------------------------------------------------------------------------------
//! What a film scene sets in the world and gives back after the take (#394): the local clock hour, the
//! weather state, and whether the day keeps advancing. Dev/harness only, like the rest of Feature/Film.
//! A scene's defaults make every take start from the same light; `scene start <name> [hour|-] [weather]`
//! overrides them for one take. The snapshot taken before a scene is what `LF_FilmEnvironment.Apply`
//! writes back on reset, so repeated takes never drift the clock.
class LF_FilmSceneEnv : Managed
{
	//! The police UAZ that drives up the main street with its lights on (robbery alarms video, #740).
	static const string ROBBERY_POLICE = "robbery-police";

	float Hour = -1;           // local clock hour (sun time); < 0 leaves the clock alone
	string Weather;            // weather state name; empty leaves the weather alone
	int AutoAdvance = -1;      // -1 leave, 0 freeze the day, 1 let it run
	bool Looping;             // weather looping; a snapshot records it so a reset gives it back

	//------------------------------------------------------------------------------------------------
	//! Defaults of a named scene. An unknown name returns an env that changes nothing.
	static LF_FilmSceneEnv ForScene(string scene)
	{
		LF_FilmSceneEnv env = new LF_FilmSceneEnv();
		if (scene == "street" || scene == ROBBERY_POLICE)
		{
			env.Hour = 16;
			env.Weather = "Clear";
			env.AutoAdvance = 0;
			return env;
		}

		float hour = EnvOnlyHour(scene);
		if (hour >= 0)
		{
			env.Hour = hour;
			env.Weather = "Clear";
			env.AutoAdvance = 0;
		}

		return env;
	}

	//------------------------------------------------------------------------------------------------
	//! Per-location presets of the trailer storyboard (video/trailer-571.json): a clock and clear weather, no
	//! actors. The hour is local sun time; 17.9 is the golden hour every gallery shot uses. -1: not a preset.
	static float EnvOnlyHour(string scene)
	{
		if (scene == "trailer-bank" || scene == "trailer-square" || scene == "trailer-fuel" || scene == "trailer-police" || scene == "trailer-store" || scene == "trailer-farm" || scene == "trailer-hospital" || scene == "trailer-convoy")
			return 17.9;
		if (scene == "trailer-square-dusk")
			return 19.2;
		if (scene == "trailer-quarry")
			return 16.5;
		if (scene == "trailer-chop")
			return 18.3;
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! True for a preset that only sets the light and spawns nothing; the director starts no actors for it.
	static bool IsEnvOnly(string scene)
	{
		return EnvOnlyHour(scene) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Apply a per-take override from the harness. hour "" or "-" keeps the scene's hour; weather "" or "-"
	//! keeps its weather. Returns false (and changes nothing) for an hour that is not a number in 0..24.
	bool Override(string hour, string weather)
	{
		if (!hour.IsEmpty() && hour != "-")
		{
			if (!IsNumber(hour))
				return false;
			float h = hour.ToFloat();
			if (h < 0 || h >= 24)
				return false;
			Hour = h;
		}

		if (!weather.IsEmpty() && weather != "-")
			Weather = weather;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsNumber(string text)
	{
		int dots;
		for (int i = 0; i < text.Length(); i++)
		{
			string c = text.Get(i);
			if (c == ".")
			{
				dots++;
				if (dots > 1)
					return false;
			}
			else if (c < "0" || c > "9")
			{
				return false;
			}
		}

		return !text.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Local sun time to the game clock (the clock verb's rule: local = game + offset), wrapped into 0..24.
	static float LocalToGame(float local, float offset)
	{
		return Wrap24(local - offset);
	}

	//------------------------------------------------------------------------------------------------
	static float GameToLocal(float game, float offset)
	{
		return Wrap24(game + offset);
	}

	//------------------------------------------------------------------------------------------------
	static float Wrap24(float hour)
	{
		while (hour < 0)
			hour += 24;
		while (hour >= 24)
			hour -= 24;
		return hour;
	}

	//------------------------------------------------------------------------------------------------
	string Describe()
	{
		return string.Format("hour=%1 weather=%2 advance=%3", Hour, Weather, AutoAdvance);
	}
};

//------------------------------------------------------------------------------------------------
//! Reads and writes the world's clock and weather for a scene. Server only (the manager is replicated from there).
class LF_FilmEnvironment
{
	protected static const string FEATURE = "FilmScene";

	//------------------------------------------------------------------------------------------------
	protected static TimeAndWeatherManagerEntity GetClock()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return null;
		return world.GetTimeAndWeatherManager();
	}

	//------------------------------------------------------------------------------------------------
	protected static float Offset(TimeAndWeatherManagerEntity clock)
	{
		return clock.GetTimeZoneOffset() + clock.GetCurrentLongitude() / 15;
	}

	//------------------------------------------------------------------------------------------------
	//! The world's current environment, or null (logged) when there is no manager.
	static LF_FilmSceneEnv Capture()
	{
		TimeAndWeatherManagerEntity clock = GetClock();
		if (!clock)
		{
			LF_Debug.Error(FEATURE, "no time and weather manager: scene runs without a clock or weather");
			return null;
		}

		LF_FilmSceneEnv env = new LF_FilmSceneEnv();
		env.Hour = LF_FilmSceneEnv.GameToLocal(clock.GetTimeOfTheDay(), Offset(clock));
		WeatherState state = clock.GetCurrentWeatherState();
		if (state)
			env.Weather = state.GetStateName();
		if (clock.GetIsDayAutoAdvanced())
			env.AutoAdvance = 1;
		else
			env.AutoAdvance = 0;
		env.Looping = clock.IsWeatherLooping();
		return env;
	}

	//------------------------------------------------------------------------------------------------
	//! Write env into the world. Fields that say "leave" are skipped. Returns false when there is no manager.
	static bool Apply(LF_FilmSceneEnv env, string why)
	{
		TimeAndWeatherManagerEntity clock = GetClock();
		if (!clock || !env)
			return false;

		if (env.AutoAdvance == 0)
			clock.SetIsDayAutoAdvanced(false);
		if (env.Hour >= 0)
			clock.SetTimeOfTheDay(LF_FilmSceneEnv.LocalToGame(env.Hour, Offset(clock)), true);
		if (!env.Weather.IsEmpty())
			clock.ForceWeatherTo(env.Looping, env.Weather, 0, 3600);
		if (env.AutoAdvance == 1)
			clock.SetIsDayAutoAdvanced(true);

		LF_Debug.Info(FEATURE, string.Format("%1: %2, now hour=%3", why, env.Describe(), LF_FilmSceneEnv.GameToLocal(clock.GetTimeOfTheDay(), Offset(clock))));
		return true;
	}
};
