//------------------------------------------------------------------------
// Game.cpp
//------------------------------------------------------------------------

#include "AudioEngine.h"
#include "Gameplay/Application.h"

float gameTime;

//------------------------------------------------------------------------
// Called before first update. Do any initial setup here.
//------------------------------------------------------------------------

void Init()
{
	AudioEngine& engine = AudioEngine::Instance();
	engine.Init();
	
	engine.LoadBank("sound/WinSoundBank");//("sound/Master")
	engine.LoadBank("sound/ReloadBank");
	engine.LoadBank("sound/OnHitBank");
	engine.LoadBank("sound/BackgroundBank");
	engine.LoadBank("sound/Master");
	engine.LoadBank("sound/Master.strings");
	
	engine.LoadBus("WinBus", "bus:/WinBus");//("MusicBus", "{a5b53ded-d7b3-4e6b-a920-0b241ef6f268}")
	engine.LoadBus("ReloadBus", "bus:/ReloadBus");
	engine.LoadBus("HitBus", "bus:/HitBus");
	engine.LoadBus("BackgroundBus", "bus:/BackgroundBus");
	//play a music event
	AudioEvent& music = engine.CreateEvent("music", "event:/WinSound");//("music", "{b56cb9d2-1d47-4099-b80e-7d257b99a823}")
	AudioEvent& music1 = engine.CreateEvent("music1", "event:/ReloadSound");
	AudioEvent& music2 = engine.CreateEvent("music2", "event:/OnHitSound");
	AudioEvent& music3 = engine.CreateEvent("music3", "event:/BackgroundSound");

	

	//move to update for button press
	//music.Play();
	//music1.Play();
	//music2.Play();
	music3.Play();

}

//------------------------------------------------------------------------
// Update your game here. 
//------------------------------------------------------------------------
void Update(float deltaTime, bool win, bool win2, bool hit, bool hit2, bool bottle1, bool bottle2, bool bottle3, bool bottle4, bool bottle5, bool bottle6, bool bottle7, bool bottle8, bool bottle9, bool bottle10)
{
	GLFWwindow* window = Application::Instance().Window;
	gameTime += deltaTime;
	
	AudioEngine& engine = AudioEngine::Instance();

	//get ref to bus
	AudioBus& musicBus = engine.GetBus("WinBus");
	AudioBus& musicBus1 = engine.GetBus("ReloadBus");
	AudioBus& musicBus2 = engine.GetBus("HitBus");
	AudioBus& musicBus3 = engine.GetBus("BackgroundBus");

	//get ref to music
	AudioEvent& music = engine.GetEvent("music");
	AudioEvent& music1 = engine.GetEvent("music1");
	AudioEvent& music2 = engine.GetEvent("music2");
	AudioEvent& music3 = engine.GetEvent("music3");

	

	//get ref to listener
	AudioListener& listener = engine.GetListener();

	listener.SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
	
	
	// After 5 seconds go underwater
	//if (gameTime > 4.0f)
	//{
	//	music.SetParameter("Underwater", 1.0f);
	//	musicBus.SetPaused(true);
	//}
	//if (gameTime > 6.0f)
	//{
	//	engine.SetGlobalParameter("Timewarp", 0.0f);
	//	musicBus.SetPaused(false);
	//}

	if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS)
	{
		//music3 swap between fast and slow
		music3.SetParameter("GoToLoop", '0');
	}

	if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
	{
		//music3 swap between fast and slow
		music3.SetParameter("GoToLoop", '1');
	}

	if (win || win2)
	//if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
	{
		music.Play();//Win Sound
	}
	
	if (bottle1)
	{
		music1.Play();//Reload Waterbottle
		//listener.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
	}if (bottle2)
	{
		music1.Play();//Reload Waterbottle
		//listener.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
	}if (bottle3)
	{
		music1.Play();//Reload Waterbottle
		//listener.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
	}if (bottle4)
	{
		music1.Play();//Reload Waterbottle
		//listener.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
	}if (bottle5)
	{
		music1.Play();//Reload Waterbottle
		//listener.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
	}if (bottle6)
	{
		music1.Play();//Reload Waterbottle
		//listener.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
	}if (bottle7)
	{
		music1.Play();//Reload Waterbottle
		//listener.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
	}if (bottle8)
	{
		music1.Play();//Reload Waterbottle
		//listener.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
	}if (bottle9)
	{
		music1.Play();//Reload Waterbottle
		//listener.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
	}if (bottle10)
	{
		music1.Play();//Reload Waterbottle
		//listener.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));
	}
	
	if (hit)
	//if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
	{
		music2.Play();//Splash
	}

	if (hit2)
		//if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
	{
		music2.Play();//Splash
	}


	engine.Update();
}


//------------------------------------------------------------------------
// Add your display calls here
//------------------------------------------------------------------------
void Render()
{
	// Who needs graphics when you have audio?
}


//------------------------------------------------------------------------
// Add your shutdown code here.
//------------------------------------------------------------------------
void Shutdown()
{
	AudioEngine::Instance().Shutdown();
}