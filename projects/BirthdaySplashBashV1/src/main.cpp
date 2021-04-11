/*
Alexander Chow - 100749034
James Pham - 100741773
Trey Cowell - 100745472
Frederic Lai - 100748388
Anita Lim - 100754729

Birthday Splash Bash (DEMO) is a 1v1 duel between 2 players.
First player to hit their opponent 3 times is the winner!
After you fire you water gun, you have to search and walk over a water bottle to reload.

Player 1 Yellow Left: W (Forward), A (Left), S (Back), D (Right), E (Shoot).
Player 2 Pink Right: I (Forward), J (Left), K (Back) L (Right), O (Shoot).

We have been using Parsec, a screen sharing program, to play online "locally" with each other.
*/
#include <Logging.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <json.hpp>
#include <fstream>

#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>

#include "Graphics/IndexBuffer.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/Shader.h"
#include "Graphics/DirectionalLight.h"
#include "Graphics/PointLight.h"
#include "Graphics/UniformBuffer.h"

#include "Gameplay/Camera.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Behaviours/CameraControlBehaviour.h"
#include "Behaviours/FollowPathBehaviour.h"
#include "Behaviours/SimpleMoveBehaviour.h"
#include "Behaviours/PlayerMovement.h"
#include "Gameplay/Application.h"
#include "Gameplay/GameObjectTag.h"
#include "Gameplay/IBehaviour.h"
#include "Gameplay/Transform.h"
#include "Gameplay/UI.h"
#include "Graphics/Texture2D.h"
#include "Graphics/Texture2DData.h"
#include "Utilities/InputHelpers.h"
#include "Utilities/MeshBuilder.h"
#include "Utilities/MeshFactory.h"
#include "Utilities/NotObjLoader.h"
#include "Utilities/ObjLoader.h"
#include "Utilities/VertexTypes.h"
#include "Utilities/BackendHandler.h"
#include "Gameplay/Scene.h"
#include "Gameplay/ShaderMaterial.h"
#include "Gameplay/RendererComponent.h"
#include "Gameplay/Timing.h"
#include "Graphics/TextureCubeMap.h"
#include "Graphics/TextureCubeMapData.h"
#include "Utilities/Util.h"

#define LOG_GL_NOTIFICATIONS
#define NUM_HITBOXES_TEST 2
#define NUM_HITBOXES 21
#define NUM_BOTTLES_ARENA 7

// Borrowed collision from https://learnopengl.com/In-Practice/2D-Game/Collisions/Collision-detection AABB collision
bool Collision(Transform& hitbox1, Transform& hitbox2)
{
	bool colX = hitbox1.GetLocalPosition().x + hitbox1.GetLocalScale().x >= hitbox2.GetLocalPosition().x
		&& hitbox2.GetLocalPosition().x + hitbox2.GetLocalScale().x >= hitbox1.GetLocalPosition().x;

	bool colY = hitbox1.GetLocalPosition().y + hitbox1.GetLocalScale().y >= hitbox2.GetLocalPosition().y
		&& hitbox2.GetLocalPosition().y + hitbox2.GetLocalScale().y >= hitbox1.GetLocalPosition().y;
	return colX && colY;
}

//doesnt work
glm::vec3 projection1(Transform& hitbox1, Transform& hitbox2)
{
	glm::vec3 dotproduct = abs(hitbox1.GetLocalPosition()) * abs(hitbox2.GetLocalPosition()) * hitbox1.GetLocalRotation();
	float magnitude = sqrt(hitbox2.GetLocalPosition().x * hitbox2.GetLocalPosition().x + hitbox2.GetLocalPosition().y * hitbox2.GetLocalPosition().y);
	glm::vec3 projection = (dotproduct / magnitude * magnitude) * hitbox2.GetLocalPosition();

	return projection;
}

//Insertion sort algorithm
void insertionSort(std::vector<float>& highscores, int arraysize)
{
	// Start a timer
	auto start = std::chrono::high_resolution_clock::now();
	for (int i = 1, j; i < arraysize; i++)
	{
		float nextnumber = highscores[i];
		for(j = i - 1; nextnumber > highscores[j] && j >= 0; j--)
		{
			highscores[j + 1] = highscores[j];
		}
		highscores[j + 1] = nextnumber;
	}
	// Stop timer
	auto elapsed = std::chrono::high_resolution_clock::now() - start;

	// Print results
	long long microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
	std::cout << microseconds << std::endl;
}

//FMOD

extern void Init();
extern void Update(float deltaTime, bool win, bool win2, bool hit, bool hit2, bool bottle1, bool bottle2, bool bottle3, bool bottle4, bool bottle5, bool bottle6, bool bottle7, bool bottle8, bool bottle9, bool bottle10);
extern void Render();
extern void Shutdown();

int main() {

	int frameIx = 0;
	float fpsBuffer[128];
	float minFps, maxFps, avgFps;
	int selectedVao = 0; // select cube by default
	std::vector<GameObject> controllables;

	bool drawGBuffer = false;
	bool drawIllumBuffer = false;
	bool PositionBuffer = false;
	bool NormalBuffer = false;
	bool MaterialBuffer = false;
	bool lightaccumbuffer = false;

	BackendHandler::InitAll();

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(BackendHandler::GlDebugMessage, nullptr);

	// Enable texturing
	glEnable(GL_TEXTURE_2D);

	Init();

	// Push another scope so most memory should be freed *before* we exit the app
	{
		#pragma region Shader and ImGui

		// Load our shaders
		Shader::sptr shader = Shader::Create();
		shader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		shader->LoadShaderPartFromFile("shaders/frag_blinn_phong_textured.glsl", GL_FRAGMENT_SHADER);
		shader->Link();
		
		// Load a second material for our reflective material!
		Shader::sptr reflectiveShader = Shader::Create();
		reflectiveShader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		reflectiveShader->LoadShaderPartFromFile("shaders/frag_reflection.frag.glsl", GL_FRAGMENT_SHADER);
		reflectiveShader->Link();

		Shader::sptr reflective = Shader::Create();
		reflective->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		reflective->LoadShaderPartFromFile("shaders/frag_blinn_phong_reflection.glsl", GL_FRAGMENT_SHADER);
		reflective->Link();

		Shader::sptr simpleDepthShader = Shader::Create();
		simpleDepthShader->LoadShaderPartFromFile("shaders/simple_depth_vert.glsl", GL_VERTEX_SHADER);
		simpleDepthShader->LoadShaderPartFromFile("shaders/simple_depth_frag.glsl", GL_FRAGMENT_SHADER);
		simpleDepthShader->Link();

		//Init gBuffer shader
		Shader::sptr gBufferShader = Shader::Create();
		gBufferShader->LoadShaderPartFromFile("shaders/vertex_shader.glsl", GL_VERTEX_SHADER);
		gBufferShader->LoadShaderPartFromFile("shaders/gBuffer_pass_frag.glsl", GL_FRAGMENT_SHADER);
		gBufferShader->Link();

		//Creates our directional Light
		DirectionalLight theSun;
		UniformBuffer directionalLightBuffer;

		//Allocates enough memory for one directional light (we can change this easily, but we only need 1 directional light)
		directionalLightBuffer.AllocateMemory(sizeof(DirectionalLight));
		//Casts our sun as "data" and sends it to the shader
		directionalLightBuffer.SendData(reinterpret_cast<void*>(&theSun), sizeof(DirectionalLight));

		directionalLightBuffer.Bind(0);
		
		glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 17.0f);
		glm::vec3 lightCol = glm::vec3(1.0f, 1.0f, 1.0f);
		float     lightAmbientPow = 4.1f;
		float     lightSpecularPow = 1.0f;
		glm::vec3 ambientCol = glm::vec3(1.0f);
		float     ambientPow = 0.1f;
		float     lightLinearFalloff = 0.009;
		float     lightQuadraticFalloff = 0.032f;
		//variables for turning lighting off and on
		int		  lightoff = 0;
		int		  ambientonly = 0;
		int		  specularonly = 0;
		int		  ambientandspecular = 0;
		int		  ambientspeculartoon = 0;
		int		  Textures = 1;
		int		  shadowonoff = 0;
		bool	  OnOff = true;
		bool	  Shadowsonoff = false;
		std::ofstream highscores;
		std::ifstream highscoresread("highscores.txt");
		int counter = 0;
		std::vector<float> arrayscore;
		std::string line;

		while (std::getline(highscoresread, line)) {
			float value = std::stof(line);
			counter += 1;
			arrayscore.push_back(value);
		}
		highscoresread.close();

		insertionSort(arrayscore, counter);
		
		// These are our application / scene level uniforms that don't necessarily update
		// every frame
		shader->SetUniform("u_LightPos", lightPos);
		shader->SetUniform("u_LightCol", lightCol);
		shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
		shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
		shader->SetUniform("u_AmbientCol", ambientCol);
		shader->SetUniform("u_AmbientStrength", ambientPow);
		shader->SetUniform("u_LightAttenuationConstant", 1.0f);
		shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
		shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
		shader->SetUniform("u_lightoff", lightoff);
		shader->SetUniform("u_ambient", ambientonly);
		shader->SetUniform("u_specular", specularonly);
		shader->SetUniform("u_ambientspecular", ambientandspecular);
		shader->SetUniform("u_ambientspeculartoon", ambientspeculartoon);
		shader->SetUniform("u_Textures", Textures);

		PostEffect* basicEffect;
		Framebuffer* shadowBuffer;
		GBuffer* gBuffer;
		IlluminationBuffer* illuminationBuffer;

		int activeEffect = 0;
		std::vector<PostEffect*> effects;

		BlurEffect* blureffect;
		ColorCorrectEffect* colorCorrectioneffect;
		GrainEffect* grainEffect;
		PixelateEffect* pixelateeffect;

		// We'll add some ImGui controls to control our shader
		BackendHandler::imGuiCallbacks.push_back([&]() {
			if (ImGui::CollapsingHeader("Scene Level Lighting Settings"))
			{
				if (ImGui::ColorPicker3("Ambient Color", glm::value_ptr(ambientCol))) {
					shader->SetUniform("u_AmbientCol", ambientCol);
				}
				if (ImGui::SliderFloat("Fixed Ambient Power", &ambientPow, 0.01f, 1.0f)) {
					shader->SetUniform("u_AmbientStrength", ambientPow);
				}
			}
			if (ImGui::CollapsingHeader("Light Level Lighting Settings"))
			{
				if (ImGui::DragFloat3("Light Pos", glm::value_ptr(lightPos), 0.01f, -10.0f, 10.0f)) {
					shader->SetUniform("u_LightPos", lightPos);
				}
				if (ImGui::ColorPicker3("Light Col", glm::value_ptr(lightCol))) {
					shader->SetUniform("u_LightCol", lightCol);
				}
				if (ImGui::SliderFloat("Light Ambient Power", &lightAmbientPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_AmbientLightStrength", lightAmbientPow);
				}
				if (ImGui::SliderFloat("Light Specular Power", &lightSpecularPow, 0.0f, 1.0f)) {
					shader->SetUniform("u_SpecularLightStrength", lightSpecularPow);
				}
				if (ImGui::DragFloat("Light Linear Falloff", &lightLinearFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationLinear", lightLinearFalloff);
				}
				if (ImGui::DragFloat("Light Quadratic Falloff", &lightQuadraticFalloff, 0.01f, 0.0f, 1.0f)) {
					shader->SetUniform("u_LightAttenuationQuadratic", lightQuadraticFalloff);
				}
			}

			if (ImGui::CollapsingHeader("Effect controls"))
			{
				ImGui::SliderInt("Chosen Effect", &activeEffect, 0, effects.size() - 1);

				if (activeEffect == 0)
				{
					ImGui::Text("Active Effect: Bloom Effect");

					BlurEffect* temp = (BlurEffect*)effects[activeEffect];
					float threshold = temp->GetThreshold();

					if (ImGui::SliderFloat("Threshold", &threshold, 0.0f, 1.0f))
					{
						temp->SetThreshold(threshold);
					}

					BlurEffect* tempa = (BlurEffect*)effects[activeEffect];
					float Passes = tempa->GetPasses();

					if (ImGui::SliderFloat("Blur", &Passes, 0.0f, 10.0f))
					{
						tempa->SetPasses(Passes);
					}
				}

				if (activeEffect == 1)
				{
					ImGui::Text("Active Effect: Color Correct Effect");

					ColorCorrectEffect* temp = (ColorCorrectEffect*)effects[activeEffect];
					static char input[BUFSIZ];
					ImGui::InputText("Lut File to Use", input, BUFSIZ);

					if (ImGui::Button("SetLUT", ImVec2(200.0f, 40.0f)))
					{
						temp->SetLUT(LUT3D(std::string(input)));
					}
				}

				if (activeEffect == 2)
				{
					ImGui::Text("Active Effect: Grain Effect");

					GrainEffect* temp = (GrainEffect*)effects[activeEffect];
				}
				
				if (activeEffect == 3)
				{
					ImGui::Text("Active Effect: Pixelate Effect");

					PixelateEffect* temp = (PixelateEffect*)effects[activeEffect];
				}
			}

			//Toggle buttons
	
			if (ImGui::CollapsingHeader("Toggle buttons")) {
				if (ImGui::Button("No Lighting")) {
					shader->SetUniform("u_lightoff", lightoff = 1);
					shader->SetUniform("u_ambient", ambientonly = 0);
					shader->SetUniform("u_specular", specularonly = 0);
					shader->SetUniform("u_ambientspecular", ambientandspecular = 0);
					shader->SetUniform("u_ambientspeculartoon", ambientspeculartoon = 0);
					shader->SetUniform("u_Textures", Textures = 2);
				}

				if (ImGui::Button("Ambient only")) {
					shader->SetUniform("u_lightoff", lightoff = 0);
					shader->SetUniform("u_ambient", ambientonly = 1);
					shader->SetUniform("u_specular", specularonly = 0);
					shader->SetUniform("u_ambientspecular", ambientandspecular = 0);
					shader->SetUniform("u_ambientspeculartoon", ambientspeculartoon = 0);
					shader->SetUniform("u_Textures", Textures = 2);
				}

				if (ImGui::Button("specular only")) {
					shader->SetUniform("u_lightoff", lightoff = 0);
					shader->SetUniform("u_ambient", ambientonly = 0);
					shader->SetUniform("u_specular", specularonly = 1);
					shader->SetUniform("u_ambientspecular", ambientandspecular = 0);
					shader->SetUniform("u_ambientspeculartoon", ambientspeculartoon = 0);
					shader->SetUniform("u_Textures", Textures = 2);
				}

				if (ImGui::Button("Ambient and Specular")) {
					shader->SetUniform("u_lightoff", lightoff = 0);
					shader->SetUniform("u_ambient", ambientonly = 0);
					shader->SetUniform("u_specular", specularonly = 0);
					shader->SetUniform("u_ambientspecular", ambientandspecular = 1);
					shader->SetUniform("u_ambientspeculartoon", ambientspeculartoon = 0);
					shader->SetUniform("u_Textures", Textures = 2);
				}

				if (ImGui::Button("Ambient, Specular, and Toon Shading")) {
					shader->SetUniform("u_lightoff", lightoff = 0);
					shader->SetUniform("u_ambient", ambientonly = 0);
					shader->SetUniform("u_specular", specularonly = 0);
					shader->SetUniform("u_ambientspecular", ambientandspecular = 0);
					shader->SetUniform("u_ambientspeculartoon", ambientspeculartoon = 1);
					shader->SetUniform("u_Textures", Textures = 2);
				}

				if (OnOff) {
					if (ImGui::Button("Textures Off"))
					{
						shader->SetUniform("u_Textures", Textures = 0);
						OnOff = false;
					}
				}
				else {
					if (ImGui::Button("Textures On"))
					{
						shader->SetUniform("u_Textures", Textures = 1);
						OnOff = true;
					}
				}

				if (Shadowsonoff) {
					if (ImGui::Button("Shadows Off"))
					{
						Shadowsonoff = false;
						shader->SetUniform("u_Shadowsonoff", shadowonoff = 0);
					}
				}
				else {
					if (ImGui::Button("Shadows On"))
					{
						shader->SetUniform("u_Shadowsonoff", shadowonoff = 1);
						Shadowsonoff = true;
					}
				}
			}

			auto name = controllables[selectedVao].get<GameObjectTag>().Name;
			ImGui::Text(name.c_str());
			auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
			ImGui::Checkbox("Relative Rotation", &behaviour->Relative);

			ImGui::Text("Q/E -> Yaw\nLeft/Right -> Roll\nUp/Down -> Pitch\nY -> Toggle Mode\nF1 All 4 -> buffers shown\n1 -> deferred light source\n2 -> posstion buffer\n3 -> Normal buffer\n4 -> material buffer\n5 -> light accum buffer");

			minFps = FLT_MAX;
			maxFps = 0;
			avgFps = 0;
			for (int ix = 0; ix < 128; ix++) {
				if (fpsBuffer[ix] < minFps) { minFps = fpsBuffer[ix]; }
				if (fpsBuffer[ix] > maxFps) { maxFps = fpsBuffer[ix]; }
				avgFps += fpsBuffer[ix];
			}
			ImGui::PlotLines("FPS", fpsBuffer, 128);
			ImGui::Text("MIN: %f MAX: %f AVG: %f", minFps, maxFps, avgFps / 128.0f);
			});

		#pragma endregion Shader and ImGui

		// GL states
		glEnable(GL_DEPTH_TEST);
		//glEnable(GL_CULL_FACE);
		glDepthFunc(GL_LEQUAL); // New 

		#pragma region TEXTURE LOADING

		#pragma region Menu diffuse

		Texture2D::sptr diffuseMenu = Texture2D::LoadFromFile("images/Menu/TitleText.PNG");
		Texture2D::sptr diffuseInstructions = Texture2D::LoadFromFile("images/Menu/IntroText.PNG");
		Texture2D::sptr diffuseControls = Texture2D::LoadFromFile("images/Menu/ControlsController.png");
		Texture2D::sptr diffuseinfo = Texture2D::LoadFromFile("images/Menu/Gameinfo.png");

		#pragma endregion Menu diffuse
		
		#pragma region Pause diffuse

		Texture2D::sptr diffusePause = Texture2D::LoadFromFile("images/Menu/IntroScene.PNG");

		#pragma endregion Pause diffuse

		#pragma region testing scene difuses
		// Load some textures from files
		Texture2D::sptr diffuse = Texture2D::LoadFromFile("images/TestScene/Stone_001_Diffuse.png");
		Texture2D::sptr diffuseGround = Texture2D::LoadFromFile("images/TestScene/grass.jpg");
		Texture2D::sptr diffuseDunce = Texture2D::LoadFromFile("images/TestScene/SkinPNG.png");
		Texture2D::sptr diffuseDuncet = Texture2D::LoadFromFile("images/TestScene/Duncet.png");
		Texture2D::sptr diffuseSlide = Texture2D::LoadFromFile("images/TestScene/Slide.png");
		//Texture2D::sptr diffuseSwing = Texture2D::LoadFromFile("images/TestScene/Swing.png");
		Texture2D::sptr diffuseTable = Texture2D::LoadFromFile("images//TestScene/Table.png");
		Texture2D::sptr diffuseTreeBig = Texture2D::LoadFromFile("images/TestScene/TreeBig.png");
		Texture2D::sptr diffuseRedBalloon = Texture2D::LoadFromFile("images/TestScene/BalloonRed.png");
		Texture2D::sptr diffuseYellowBalloon = Texture2D::LoadFromFile("images/TestScene/BalloonYellow.png");
		Texture2D::sptr diffuse2 = Texture2D::LoadFromFile("images/TestScene/box.bmp");
		Texture2D::sptr specular = Texture2D::LoadFromFile("images/TestScene/Stone_001_Specular.png");
		Texture2D::sptr reflectivity = Texture2D::LoadFromFile("images/TestScene/box-reflections.bmp");
		#pragma endregion testing scene difuses

		#pragma region Arena1 diffuses
		Texture2D::sptr diffuseTrees = Texture2D::LoadFromFile("images/Arena1/Trees.png");
		Texture2D::sptr diffuseFlowers = Texture2D::LoadFromFile("images/Arena1/Flower.png");
		Texture2D::sptr diffuseGroundArena = Texture2D::LoadFromFile("images/Arena1/Ground.png");
		Texture2D::sptr diffuseHedge = Texture2D::LoadFromFile("images/Arena1/Hedge.png");
		Texture2D::sptr diffuseBalloons = Texture2D::LoadFromFile("images/Arena1/Ballons.png");
		Texture2D::sptr diffuseDunceArena = Texture2D::LoadFromFile("images/Arena1/SkinPNG.png");
		Texture2D::sptr diffuseDuncetArena = Texture2D::LoadFromFile("images/Arena1/Duncet.png");
		Texture2D::sptr diffusered = Texture2D::LoadFromFile("images/Arena1/red.png");
		Texture2D::sptr diffuseyellow = Texture2D::LoadFromFile("images/Arena1/yellow.png");
		Texture2D::sptr diffusepink = Texture2D::LoadFromFile("images/Arena1/pink.png");
		Texture2D::sptr diffusemonkeybar = Texture2D::LoadFromFile("images/Arena1/MonkeyBar.png");
		Texture2D::sptr diffusecake = Texture2D::LoadFromFile("images/Arena1/SliceOfCake.png");
		Texture2D::sptr diffusesandbox = Texture2D::LoadFromFile("images/Arena1/SandBox.png");
		Texture2D::sptr diffuseroundabout = Texture2D::LoadFromFile("images/Arena1/RoundAbout.png");
		Texture2D::sptr diffusepinwheel = Texture2D::LoadFromFile("images/Arena1/Pinwheel.png");
		Texture2D::sptr diffuseBench = Texture2D::LoadFromFile("images/Arena1/Bench.png");
		Texture2D::sptr diffuseBottle = Texture2D::LoadFromFile("images/Arena1/Bottle.png");
		Texture2D::sptr diffuseBottleEmpty = Texture2D::LoadFromFile("images/Arena1/Blue.png");
		Texture2D::sptr diffuseWaterBeam = Texture2D::LoadFromFile("images/Arena1/waterBeamTex.png");
		#pragma endregion Arena1 diffuses

		#pragma region Arena2 diffuses
		Texture2D::sptr diffuseGroundArena2 = Texture2D::LoadFromFile("images/Arena2/A2GroundDark.png");
		Texture2D::sptr diffuseGate = Texture2D::LoadFromFile("images/Arena2/A2Gate.png");
		Texture2D::sptr diffuseA2Bush = Texture2D::LoadFromFile("images/Arena2/Bush.png");
		Texture2D::sptr diffuseA2Swing = Texture2D::LoadFromFile("images/Arena2/A2Swing.png");
		Texture2D::sptr diffuseA2Pathway = Texture2D::LoadFromFile("images/Arena2/A2Pathway.png");
		#pragma endregion Arena2 diffuses
		
		// Load the cube map
		TextureCubeMap::sptr environmentMap = TextureCubeMap::LoadFromImages("images/cubemaps/skybox/ocean.jpg"); 
		LUT3D test("cubes/BrightenedCorrection.cube");

		// Creating an empty texture
		Texture2DDescription desc = Texture2DDescription();  
		desc.Width = 1;
		desc.Height = 1;
		desc.Format = InternalFormat::RGB8;
		Texture2D::sptr texture2 = Texture2D::Create(desc);
		// Clear it with a white colour
		texture2->Clear();

		#pragma endregion TEXTURE LOADING

		#pragma region Scene Generation
		
		// We need to tell our scene system what extra component types we want to support
		GameScene::RegisterComponentType<RendererComponent>();
		GameScene::RegisterComponentType<BehaviourBinding>();
		GameScene::RegisterComponentType<Camera>();

		// Create scenes, and set menu to be the active scene in the application
		GameScene::sptr scene = GameScene::Create("test");
		GameScene::sptr Arena1 = GameScene::Create("Arena1");
		GameScene::sptr Menu = GameScene::Create("Menu");
		GameScene::sptr Pause = GameScene::Create("Pause");

		Application::Instance().scenes.push_back(scene);
		Application::Instance().scenes.push_back(Arena1);
		Application::Instance().scenes.push_back(Menu);
		Application::Instance().scenes.push_back(Pause);

		Application::Instance().ActiveScene = scene;

		// We can create a group ahead of time to make iterating on the group faster
		entt::basic_group<entt::entity, entt::exclude_t<>, entt::get_t<Transform>, RendererComponent> renderGroup =
			scene->Registry().group<RendererComponent>(entt::get_t<Transform>());
		
		entt::basic_group<entt::entity, entt::exclude_t<UI>, entt::get_t<Transform>, RendererComponent> renderGroupArena =
			Arena1->Registry().group<RendererComponent>(entt::get_t<Transform>(), entt::exclude_t<UI>());
		
		entt::basic_group<entt::entity, entt::exclude_t<>, entt::get_t<Transform>, RendererComponent> renderGroupPause =
			Pause->Registry().group<RendererComponent>(entt::get_t<Transform>());
		
		entt::basic_group<entt::entity, entt::exclude_t<>, entt::get_t<Transform>, RendererComponent> renderGroupMenu =
			Menu->Registry().group<RendererComponent>(entt::get_t<Transform>());
		#pragma endregion Scene Generation

		#pragma region Materials
		// Create materials and set some properties for them
		ShaderMaterial::sptr materialGround = ShaderMaterial::Create();
		materialGround->Shader = shader;
		materialGround->Set("s_Diffuse", diffuseGround);
		materialGround->Set("s_Diffuse2", diffuse2);
		materialGround->Set("s_Specular", specular);
		materialGround->Set("u_Shininess", 8.0f);
		materialGround->Set("u_TextureMix", 0.0f); 

		ShaderMaterial::sptr materialGround2 = ShaderMaterial::Create();
		materialGround2->Shader = shader;
		materialGround2->Set("s_Diffuse", diffuseGroundArena2);
		materialGround2->Set("s_Diffuse2", diffuse2);
		materialGround2->Set("s_Specular", specular);
		materialGround2->Set("u_Shininess", 8.0f);
		materialGround2->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialDunce = ShaderMaterial::Create(); 
		materialDunce->Shader = shader;
		materialDunce->Set("s_Diffuse", diffuseDunce);
		materialDunce->Set("s_Diffuse2", diffuse2);
		materialDunce->Set("s_Specular", specular);
		materialDunce->Set("u_Shininess", 8.0f);
		materialDunce->Set("u_TextureMix", 0.0f); 
		
		ShaderMaterial::sptr materialDuncet = ShaderMaterial::Create();  
		materialDuncet->Shader = shader;
		materialDuncet->Set("s_Diffuse", diffuseDuncet);
		materialDuncet->Set("s_Diffuse2", diffuse2);
		materialDuncet->Set("s_Specular", specular);
		materialDuncet->Set("u_Shininess", 8.0f);
		materialDuncet->Set("u_TextureMix", 0.0f); 

		ShaderMaterial::sptr materialSlide = ShaderMaterial::Create();  
		materialSlide->Shader = shader;
		materialSlide->Set("s_Diffuse", diffuseSlide);
		materialSlide->Set("s_Diffuse2", diffuse2);
		materialSlide->Set("s_Specular", specular);
		materialSlide->Set("u_Shininess", 8.0f);
		materialSlide->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialSwing = ShaderMaterial::Create();  
		materialSwing->Shader = shader;
		materialSwing->Set("s_Diffuse", diffuseA2Swing);
		materialSwing->Set("s_Diffuse2", diffuse2);
		materialSwing->Set("s_Specular", specular);
		materialSwing->Set("u_Shininess", 8.0f);
		materialSwing->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialMonkeyBar = ShaderMaterial::Create();  
		materialMonkeyBar->Shader = shader;
		materialMonkeyBar->Set("s_Diffuse", diffusemonkeybar);
		materialMonkeyBar->Set("s_Diffuse2", diffuse2);
		materialMonkeyBar->Set("s_Specular", specular);
		materialMonkeyBar->Set("u_Shininess", 8.0f);
		materialMonkeyBar->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr materialSliceOfCake = ShaderMaterial::Create();
		materialSliceOfCake->Shader = shader;
		materialSliceOfCake->Set("s_Diffuse", diffusecake);
		materialSliceOfCake->Set("s_Diffuse2", diffuse2);
		materialSliceOfCake->Set("s_Specular", specular);
		materialSliceOfCake->Set("u_Shininess", 8.0f);
		materialSliceOfCake->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr materialSandBox = ShaderMaterial::Create();
		materialSandBox->Shader = shader;
		materialSandBox->Set("s_Diffuse", diffusesandbox);
		materialSandBox->Set("s_Diffuse2", diffuse2);
		materialSandBox->Set("s_Specular", specular);
		materialSandBox->Set("u_Shininess", 8.0f);
		materialSandBox->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr materialRA = ShaderMaterial::Create();
		materialRA->Shader = shader;
		materialRA->Set("s_Diffuse", diffuseroundabout);
		materialRA->Set("s_Diffuse2", diffuse2);
		materialRA->Set("s_Specular", specular);
		materialRA->Set("u_Shininess", 8.0f);
		materialRA->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialTable = ShaderMaterial::Create();  
		materialTable->Shader = shader;
		materialTable->Set("s_Diffuse", diffuseTable);
		materialTable->Set("s_Diffuse2", diffuse2);
		materialTable->Set("s_Specular", specular);
		materialTable->Set("u_Shininess", 8.0f);
		materialTable->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialTreeBig = ShaderMaterial::Create();  
		materialTreeBig->Shader = shader;
		materialTreeBig->Set("s_Diffuse", diffuseTreeBig);
		materialTreeBig->Set("s_Diffuse2", diffuse2);
		materialTreeBig->Set("s_Specular", specular);
		materialTreeBig->Set("u_Shininess", 8.0f);
		materialTreeBig->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialredballoon = ShaderMaterial::Create();  
		materialredballoon->Shader = shader;
		materialredballoon->Set("s_Diffuse", diffuseRedBalloon);
		materialredballoon->Set("s_Diffuse2", diffuse2);
		materialredballoon->Set("s_Specular", specular);
		materialredballoon->Set("u_Shininess", 8.0f);
		materialredballoon->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialyellowballoon = ShaderMaterial::Create();  
		materialyellowballoon->Shader = shader;
		materialyellowballoon->Set("s_Diffuse", diffuseYellowBalloon);
		materialyellowballoon->Set("s_Diffuse2", diffuse2);
		materialyellowballoon->Set("s_Specular", specular);
		materialyellowballoon->Set("u_Shininess", 8.0f);
		materialyellowballoon->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialtrees = ShaderMaterial::Create();  
		materialtrees->Shader = shader;
		materialtrees->Set("s_Diffuse", diffuseTrees);
		materialtrees->Set("s_Diffuse2", diffuse2);
		materialtrees->Set("s_Specular", specular);
		materialtrees->Set("u_Shininess", 8.0f);
		materialtrees->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialflowers = ShaderMaterial::Create();  
		materialflowers->Shader = shader;
		materialflowers->Set("s_Diffuse", diffuseFlowers);
		materialflowers->Set("s_Diffuse2", diffuse2);
		materialflowers->Set("s_Specular", specular);
		materialflowers->Set("u_Shininess", 8.0f);
		materialflowers->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialGroundArena = ShaderMaterial::Create();  
		materialGroundArena->Shader = shader;
		materialGroundArena->Set("s_Diffuse", diffuseGroundArena);
		materialGroundArena->Set("s_Diffuse2", diffuse2);
		materialGroundArena->Set("s_Specular", specular);
		materialGroundArena->Set("u_Shininess", 8.0f);
		materialGroundArena->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialHedge = ShaderMaterial::Create();  
		materialHedge->Shader = shader;
		materialHedge->Set("s_Diffuse", diffuseHedge);
		materialHedge->Set("s_Diffuse2", diffuse2);
		materialHedge->Set("s_Specular", specular);
		materialHedge->Set("u_Shininess", 8.0f);
		materialHedge->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialBalloons = ShaderMaterial::Create();  
		materialBalloons->Shader = shader;
		materialBalloons->Set("s_Diffuse", diffuseBalloons);
		materialBalloons->Set("s_Diffuse2", diffuse2);
		materialBalloons->Set("s_Specular", specular);
		materialBalloons->Set("u_Shininess", 8.0f);
		materialBalloons->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialDunceArena = ShaderMaterial::Create();  
		materialDunceArena->Shader = shader;
		materialDunceArena->Set("s_Diffuse", diffuseDunceArena);
		materialDunceArena->Set("s_Diffuse2", diffuse2);
		materialDunceArena->Set("s_Specular", specular);
		materialDunceArena->Set("u_Shininess", 8.0f);
		materialDunceArena->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialDuncetArena = ShaderMaterial::Create();  
		materialDuncetArena->Shader = shader;
		materialDuncetArena->Set("s_Diffuse", diffuseDuncetArena);
		materialDuncetArena->Set("s_Diffuse2", diffuse2);
		materialDuncetArena->Set("s_Specular", specular);
		materialDuncetArena->Set("u_Shininess", 8.0f);
		materialDuncetArena->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr materialBottleyellow = ShaderMaterial::Create();  
		materialBottleyellow->Shader = shader;
		materialBottleyellow->Set("s_Diffuse", diffuseyellow);
		materialBottleyellow->Set("s_Diffuse2", diffuse2);
		materialBottleyellow->Set("s_Specular", specular);
		materialBottleyellow->Set("u_Shininess", 8.0f);
		materialBottleyellow->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr materialBottlepink = ShaderMaterial::Create();  
		materialBottlepink->Shader = shader;
		materialBottlepink->Set("s_Diffuse", diffusepink);
		materialBottlepink->Set("s_Diffuse2", diffuse2);
		materialBottlepink->Set("s_Specular", specular);
		materialBottlepink->Set("u_Shininess", 8.0f);
		materialBottlepink->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr materialBench = ShaderMaterial::Create();
		materialBench->Shader = shader;
		materialBench->Set("s_Diffuse", diffuseBench);
		materialBench->Set("s_Diffuse2", diffuse2);
		materialBench->Set("s_Specular", specular);
		materialBench->Set("u_Shininess", 8.0f);
		materialBench->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialMenu = ShaderMaterial::Create();
		materialMenu->Shader = shader;
		materialMenu->Set("s_Diffuse", diffuseMenu);
		materialMenu->Set("s_Diffuse2", diffuseInstructions);
		materialMenu->Set("s_Specular", specular);
		materialMenu->Set("u_Shininess", 8.0f);
		materialMenu->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr ControlsMenu = ShaderMaterial::Create();
		ControlsMenu->Shader = shader;
		ControlsMenu->Set("s_Diffuse", diffuseControls);
		ControlsMenu->Set("s_Diffuse2", diffuseInstructions);
		ControlsMenu->Set("s_Specular", specular);
		ControlsMenu->Set("u_Shininess", 8.0f);
		ControlsMenu->Set("u_TextureMix", 1.0f);

		ShaderMaterial::sptr materialPause = ShaderMaterial::Create();
		materialPause->Shader = shader;
		materialPause->Set("s_Diffuse", diffuseControls);
		materialPause->Set("s_Diffuse2", diffuseInstructions);
		materialPause->Set("s_Specular", specular);
		materialPause->Set("u_Shininess", 8.0f);
		materialPause->Set("u_TextureMix", 1.0f);

		ShaderMaterial::sptr materialwaterbottle = ShaderMaterial::Create();
		materialwaterbottle->Shader = shader;
		materialwaterbottle->Set("s_Diffuse", diffuseBottle);
		materialwaterbottle->Set("s_Diffuse2", diffuseBottleEmpty);
		materialwaterbottle->Set("s_Specular", specular);
		materialwaterbottle->Set("u_Shininess", 8.0f);
		materialwaterbottle->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialred = ShaderMaterial::Create();
		materialred->Shader = shader;
		materialred->Set("s_Diffuse", diffusered);
		materialred->Set("s_Diffuse2", diffuseBottleEmpty);
		materialred->Set("s_Specular", specular);
		materialred->Set("u_Shininess", 8.0f);
		materialred->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialyellow = ShaderMaterial::Create();
		materialyellow->Shader = shader;
		materialyellow->Set("s_Diffuse", diffuseyellow);
		materialyellow->Set("s_Diffuse2", diffuseBottleEmpty);
		materialyellow->Set("s_Specular", specular);
		materialyellow->Set("u_Shininess", 8.0f);
		materialyellow->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialpink = ShaderMaterial::Create();
		materialpink->Shader = shader;
		materialpink->Set("s_Diffuse", diffusepink);
		materialpink->Set("s_Diffuse2", diffuseBottleEmpty);
		materialpink->Set("s_Specular", specular);
		materialpink->Set("u_Shininess", 8.0f);
		materialpink->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialdropwater = ShaderMaterial::Create();
		materialdropwater->Shader = shader;
		materialdropwater->Set("s_Diffuse", diffuseWaterBeam);
		materialdropwater->Set("s_Diffuse2", diffuseBottleEmpty);
		materialdropwater->Set("s_Specular", specular);
		materialdropwater->Set("u_Shininess", 8.0f);
		materialdropwater->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr materialGate = ShaderMaterial::Create();
		materialGate->Shader = shader;
		materialGate->Set("s_Diffuse", diffuseGate);
		materialGate->Set("s_Diffuse2", diffuse);
		materialGate->Set("s_Specular", specular);
		materialGate->Set("u_Shininess", 8.0f);
		materialGate->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr materialA2Bush = ShaderMaterial::Create();
		materialA2Bush->Shader = shader;
		materialA2Bush->Set("s_Diffuse", diffuseA2Bush);
		materialA2Bush->Set("s_Diffuse2", diffuse);
		materialA2Bush->Set("s_Specular", specular);
		materialA2Bush->Set("u_Shininess", 8.0f);
		materialA2Bush->Set("u_TextureMix", 0.0f);

		ShaderMaterial::sptr materialA2Pathway = ShaderMaterial::Create();
		materialA2Pathway->Shader = shader;
		materialA2Pathway->Set("s_Diffuse", diffuseA2Pathway);
		materialA2Pathway->Set("s_Diffuse2", diffuse);
		materialA2Pathway->Set("s_Specular", specular);
		materialA2Pathway->Set("u_Shininess", 8.0f);
		materialA2Pathway->Set("u_TextureMix", 0.0f);
		
		ShaderMaterial::sptr materialinfo = ShaderMaterial::Create();
		materialinfo->Shader = shader;
		materialinfo->Set("s_Diffuse", diffuseinfo);
		materialinfo->Set("s_Diffuse2", diffuse);
		materialinfo->Set("s_Specular", specular);
		materialinfo->Set("u_Shininess", 8.0f);
		materialinfo->Set("u_TextureMix", 0.0f);

		#pragma endregion Materials

		#pragma region Reflect Mat
		// 
		ShaderMaterial::sptr material1 = ShaderMaterial::Create();
		material1->Shader = reflective;
		material1->Set("s_Diffuse", diffuse);
		material1->Set("s_Diffuse2", diffuse2);
		material1->Set("s_Specular", specular);
		material1->Set("s_Reflectivity", reflectivity);
		material1->Set("s_Environment", environmentMap);
		material1->Set("u_LightPos", lightPos);
		material1->Set("u_LightCol", lightCol);
		material1->Set("u_AmbientLightStrength", lightAmbientPow);
		material1->Set("u_SpecularLightStrength", lightSpecularPow);
		material1->Set("u_AmbientCol", ambientCol);
		material1->Set("u_AmbientStrength", ambientPow);
		material1->Set("u_LightAttenuationConstant", 1.0f);
		material1->Set("u_LightAttenuationLinear", lightLinearFalloff);
		material1->Set("u_LightAttenuationQuadratic", lightQuadraticFalloff);
		material1->Set("u_Shininess", 8.0f);
		material1->Set("u_TextureMix", 0.5f);
		material1->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));

		
		ShaderMaterial::sptr reflectiveMat = ShaderMaterial::Create();
		reflectiveMat->Shader = reflectiveShader;
		reflectiveMat->Set("s_Environment", environmentMap);
		reflectiveMat->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));
		#pragma endregion Reflect Mat

		#pragma region Menu Objects

		GameObject objMenu = Menu->CreateEntity("Main Menu");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Menu/Plane.obj");
			objMenu.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialMenu);
			objMenu.get<Transform>().SetLocalPosition(0.0f, 0.0f, 2.0f);
			objMenu.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			objMenu.get<Transform>().SetLocalScale(1.25f, 1.25f, 1.25f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(objMenu);
		}
		
		GameObject Controls = Menu->CreateEntity("Controls Menu");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Menu/Plane.obj");
			Controls.emplace<RendererComponent>().SetMesh(vao).SetMaterial(ControlsMenu);
			Controls.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
			Controls.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			Controls.get<Transform>().SetLocalScale(1.2f, 1.2f, 1.2f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(Controls);
		}
		
		GameObject info = Menu->CreateEntity("info Menu");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Menu/Plane.obj");
			info.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialinfo);
			info.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
			info.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			info.get<Transform>().SetLocalScale(1.2f, 1.2f, 1.2f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(info);
		}
		
		GameObject Gun = Menu->CreateEntity("Menu cursor");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Menu/gunwithtex.obj");
			Gun.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialDunceArena);
			Gun.get<Transform>().SetLocalPosition(-10.0f, 0.0f, 2.0f);
			Gun.get<Transform>().SetLocalRotation(0.0f, -90.0f, 180.0f);
			Gun.get<Transform>().SetLocalScale(3.0f, 3.0f, 3.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(Gun);
		}

		#pragma endregion Menu Objects

		#pragma region Pause Objects

		GameObject objPause = Pause->CreateEntity("Pause Menu");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Menu/Plane.obj");
			objPause.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialPause);
			objPause.get<Transform>().SetLocalPosition(0.0f, 0.0f, 2.0f);
			objPause.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			objPause.get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(objPause);
		}

		#pragma endregion Pause Objects

		#pragma region Test(scene) Objects

		GameObject objGround = scene->CreateEntity("Ground"); 
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestScene/Ground.obj");
			objGround.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialGround);
			objGround.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			objGround.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			objGround.get<Transform>().SetLocalScale(0.5f, 0.25f, 0.5f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(objGround);
		}

		GameObject objDunce = scene->CreateEntity("Dunce");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestScene/Dunce.obj");
			objDunce.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialDunce);
			objDunce.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.9f);
			objDunce.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(objDunce);
		}

		GameObject objDuncet = scene->CreateEntity("Duncet");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestScene/Duncet.obj");
			objDuncet.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialDuncet);
			objDuncet.get<Transform>().SetLocalPosition(2.0f, 0.0f, 0.8f);
			objDuncet.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(objDuncet);
		}
		

		GameObject objSlide = scene->CreateEntity("Slide");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestScene/Slide.obj");
			objSlide.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialSlide);
			objSlide.get<Transform>().SetLocalPosition(0.0f, 5.0f, 3.0f);
			objSlide.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			objSlide.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(objSlide);
		}
		
		GameObject objRedBalloon = scene->CreateEntity("Redballoon");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestScene/Balloon.obj");
			objRedBalloon.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialredballoon);
			objRedBalloon.get<Transform>().SetLocalPosition(2.5f, -10.0f, 3.0f);
			objRedBalloon.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			objRedBalloon.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(objRedBalloon);

			// Bind returns a smart pointer to the behaviour that was added
			auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(objRedBalloon);
			// Set up a path for the object to follow
			pathing->Points.push_back({ -2.5f, -10.0f, 3.0f });
			pathing->Points.push_back({ 2.5f, -10.0f, 3.0f });
			pathing->Points.push_back({ 2.5f, -5.0f, 3.0f });
			pathing->Points.push_back({ -2.5f, -5.0f, 3.0f });
			pathing->Speed = 2.0f;
		}
		
		GameObject objYellowBalloon = scene->CreateEntity("Yellowballoon");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestScene/Balloon.obj");
			objYellowBalloon.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialyellowballoon);
			objYellowBalloon.get<Transform>().SetLocalPosition(-2.5f, -10.0f, 3.0f);
			objYellowBalloon.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			objYellowBalloon.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(objYellowBalloon);

			// Bind returns a smart pointer to the behaviour that was added
			auto pathing = BehaviourBinding::Bind<FollowPathBehaviour>(objYellowBalloon);
			// Set up a path for the object to follow
			pathing->Points.push_back({ 2.5f, -10.0f, 3.0f });
			pathing->Points.push_back({ -2.5f, -10.0f, 3.0f });
			pathing->Points.push_back({ -2.5f,  -5.0f, 3.0f });
			pathing->Points.push_back({ 2.5f,  -5.0f, 3.0f });
			pathing->Speed = 2.0f;
		}
		

		GameObject objSwing = scene->CreateEntity("Swing");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestScene/Swing.obj");
			objSwing.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialSwing);
			objSwing.get<Transform>().SetLocalPosition(-5.0f, 0.0f, 3.5f);
			objSwing.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			objSwing.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(objSwing);
		}
		
		GameObject objRound = scene->CreateEntity("RA");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena1/RoundAbout.obj");
			objRound.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialRA);
			objRound.get<Transform>().SetLocalPosition(8.0f, 5.0f, 3.5f);
			objRound.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			objRound.get<Transform>().SetLocalScale(0.5f, 0.5f, 0.5f);
			BehaviourBinding::BindDisabled<SimpleMoveBehaviour>(objRound);
		}

		GameObject objTable = scene->CreateEntity("table");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestScene/deer.obj");
			objTable.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialTable);
			objTable.get<Transform>().SetLocalPosition(5.0f, 0.0f, 1.25f);
			objTable.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			objTable.get<Transform>().SetLocalScale(0.35f, 0.35f, 0.35f);
		}

		#pragma endregion Test(scene) Objects
		
		#pragma region Arena1 Objects New
		GameObject objDunceArena = Arena1->CreateEntity("Dunce");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestScene/Dunce.obj");
			objDunceArena.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialDunceArena);
			objDunceArena.get<Transform>().SetLocalPosition(7.0f, -2.0f, 5.0f);
			objDunceArena.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			objDunceArena.get<Transform>().SetLocalScale(0.8f, 0.8f, 0.8f);
		}

		GameObject objDuncetArena = Arena1->CreateEntity("Duncet");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestScene/Duncet.obj");
			objDuncetArena.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialDuncetArena);
			objDuncetArena.get<Transform>().SetLocalPosition(-6.0f, 3.0f, 5.0f);
			objDuncetArena.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
			objDuncetArena.get<Transform>().SetLocalScale(0.8f, 0.8f, 0.8f);
		}
		
		GameObject objGroundArena = Arena1->CreateEntity("Ground");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/A2Ground.obj");
			objGroundArena.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialGround2);
			objGroundArena.get<Transform>().SetLocalPosition(1.2f, 0.0f, -4.0f);
			objGroundArena.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objGroundArena.get<Transform>().SetLocalScale(0.435f, 1.4f, 0.50f);
		}

		GameObject objHedge = Arena1->CreateEntity("Hedge");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/A2Hedge.obj");
			objHedge.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialHedge);
			objHedge.get<Transform>().SetLocalPosition(0.0f, 0.0f, 4.0f);
			objHedge.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objHedge.get<Transform>().SetLocalScale(0.25f, 0.25f, 0.25f);
		}

		GameObject objGateA1 = Arena1->CreateEntity("Gate");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/A2Gate.obj");
			objGateA1.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialGate);
			objGateA1.get<Transform>().SetLocalPosition(0.0f, 0.0f, 4.0f);
			objGateA1.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objGateA1.get<Transform>().SetLocalScale(0.25f, 0.25f, 0.25f);
		}

		GameObject objFlower = Arena1->CreateEntity("Flowers");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/A2Flower.obj");
			objFlower.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialflowers);
			objFlower.get<Transform>().SetLocalPosition(0.0f, 0.0f, 4.0f);
			objFlower.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objFlower.get<Transform>().SetLocalScale(0.25f, 0.25f, 0.25f);
		}

		GameObject objBalloon = Arena1->CreateEntity("Balloon");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/A2Balloon.obj");
			objBalloon.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialBalloons);
			objBalloon.get<Transform>().SetLocalPosition(0.0f, 0.0f, 4.0f);
			objBalloon.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objBalloon.get<Transform>().SetLocalScale(0.25f, 0.25f, 0.25f);
		}
		
		GameObject objSlideA1 = Arena1->CreateEntity("Slide");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/A2SlideNew.obj");
			objSlideA1.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialSlide);
			objSlideA1.get<Transform>().SetLocalPosition(2.0f, 0.0f, 4.0f);
			objSlideA1.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objSlideA1.get<Transform>().SetLocalScale(0.21f, 0.21f, 0.21f);
		}

		GameObject objBushA1 = Arena1->CreateEntity("Bush");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/Bush.obj");
			objBushA1.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialA2Bush);
			objBushA1.get<Transform>().SetLocalPosition(0.0f, 0.0f, 4.0f);
			objBushA1.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objBushA1.get<Transform>().SetLocalScale(0.23f, 0.23f, 0.23f);
		}
		
		GameObject objSandBox = Arena1->CreateEntity("SandBox");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/A2SandBox.obj");
			objSandBox.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialSandBox);
			objSandBox.get<Transform>().SetLocalPosition(0.0f, 0.0f, 2.8f);
			objSandBox.get<Transform>().SetLocalRotation(90.0f, 0.0f, -270.0f);
			objSandBox.get<Transform>().SetLocalScale(0.23f, 0.23f, 0.23f);
		}

		GameObject objBench = Arena1->CreateEntity("Bench");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/A2Bench.obj");
			objBench.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialBench);
			objBench.get<Transform>().SetLocalPosition(0.0f, 0.0f, 5.0f);
			objBench.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objBench.get<Transform>().SetLocalScale(0.23f, 0.23f, 0.23f);
		}

		GameObject objTableA1 = Arena1->CreateEntity("Table");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/A2Table.obj");
			objTableA1.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialTable);
			objTableA1.get<Transform>().SetLocalPosition(0.0f, 0.0f, 4.0f);
			objTableA1.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objTableA1.get<Transform>().SetLocalScale(0.23f, 0.23f, 0.23f);
		}

		GameObject objRoundAboutA1 = Arena1->CreateEntity("RoundAbout");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/RoundAbout.obj");
			objRoundAboutA1.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialRA);
			objRoundAboutA1.get<Transform>().SetLocalPosition(-10.5f, 4.0f, 4.0f);
			objRoundAboutA1.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objRoundAboutA1.get<Transform>().SetLocalScale(0.46f, 0.46f, 0.46f);
		}

		GameObject objCake = Arena1->CreateEntity("Cake");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/A2Cake.obj");
			objCake.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialSliceOfCake);
			objCake.get<Transform>().SetLocalPosition(0.0f, 0.0f, 4.0f);
			objCake.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objCake.get<Transform>().SetLocalScale(0.23f, 0.23f, 0.23f);
		}

		GameObject objMonkeyBar = Arena1->CreateEntity("MonkeyBar");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/A2MonkeyBar.obj");
			objMonkeyBar.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialMonkeyBar);
			objMonkeyBar.get<Transform>().SetLocalPosition(0.0f, 0.0f, 4.0f);
			objMonkeyBar.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objMonkeyBar.get<Transform>().SetLocalScale(0.23f, 0.23f, 0.23f);
		}

		GameObject objSwingA1 = Arena1->CreateEntity("Swing");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/A2Swing.obj");
			objSwingA1.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialSwing);
			objSwingA1.get<Transform>().SetLocalPosition(0.0f, 0.0f, 4.0f);
			objSwingA1.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objSwingA1.get<Transform>().SetLocalScale(0.23f, 0.23f, 0.23f);
		}

		GameObject objPathwayA1 = Arena1->CreateEntity("Pathway");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena2/A2Pathway.obj");
			objPathwayA1.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialA2Pathway);
			objPathwayA1.get<Transform>().SetLocalPosition(0.0f, 0.0f, 3.0f);
			objPathwayA1.get<Transform>().SetLocalRotation(90.0f, 0.0f, -90.0f);
			objPathwayA1.get<Transform>().SetLocalScale(0.23f, 0.23f, 0.23f);
		}

		GameObject objA1BottleText1 = Arena1->CreateEntity("A2BottleUItext");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena1/BottleText.obj");
			objA1BottleText1.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialBottleyellow);
			objA1BottleText1.get<Transform>().SetLocalPosition(10.2f, 8.6f, 8.0f);
			objA1BottleText1.get<Transform>().SetLocalRotation(0.0f, 180.0f, 180.0f);
			objA1BottleText1.get<Transform>().SetLocalScale(2.0f, 2.0f, 2.0f);
		}

		GameObject objA1BottleText2 = Arena1->CreateEntity("A2BottleUItext");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena1/BottleText.obj");
			objA1BottleText2.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialBottlepink);
			objA1BottleText2.get<Transform>().SetLocalPosition(-6.0f, 8.6f, 8.0f);
			objA1BottleText2.get<Transform>().SetLocalRotation(0.0f, 180.0f, 180.0f);
			objA1BottleText2.get<Transform>().SetLocalScale(2.0f, 2.0f, 2.0f);
		}

		GameObject objA1ScoreText = Arena1->CreateEntity("A2Scoretext");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena1/Score.obj");
			objA1ScoreText.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialred);
			objA1ScoreText.get<Transform>().SetLocalPosition(2.0f, -6.7f, 8.0f);
			objA1ScoreText.get<Transform>().SetLocalRotation(0.0f, 180.0f, 180.0f);
			objA1ScoreText.get<Transform>().SetLocalScale(2.0f, 2.0f, 2.0f);
		}
		
		GameObject player1w = Arena1->CreateEntity("player1 win");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena1/p1wins.obj");
			player1w.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialyellow);
			player1w.get<Transform>().SetLocalPosition(0.0f, 0.0f, -2.0f);
			player1w.get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);
			player1w.get<Transform>().SetLocalScale(6.0f, 6.0f, 6.0f);
		}
		
		GameObject player2w = Arena1->CreateEntity("player2 win");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena1/p2wins.obj");
			player2w.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialpink);
			player2w.get<Transform>().SetLocalPosition(0.0f, 0.0f, -2.0f);
			player2w.get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);
			player2w.get<Transform>().SetLocalScale(6.0f, 6.0f, 6.0f);
		}
		
		/*GameObject objDunceAim = Arena1->CreateEntity("DunceAim");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena1/aimAssist.obj");
			objDunceAim.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialred);
			objDunceAim.get<Transform>().SetLocalPosition(8.0f, 6.0f, 1.0f);
			objDunceAim.get<Transform>().SetLocalRotation(90.0f, 0.0f, 180.0f);
			objDunceAim.get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
		}

		GameObject objDuncetAim = Arena1->CreateEntity("DuncetAim");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena1/aimAssist.obj");
			objDuncetAim.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialred);
			objDuncetAim.get<Transform>().SetLocalPosition(-8.0f, 6.0f, 1.0f);
			objDuncetAim.get<Transform>().SetLocalRotation(90.0f, 0.0f, 180.0f);
			objDuncetAim.get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
		}*/
		
		VertexArrayObject::sptr Fullscore = ObjLoader::LoadFromFile("models/Arena1/BalloonIcon.obj");
		VertexArrayObject::sptr Emptyscore = ObjLoader::LoadFromFile("models/Arena1/ScoreOutline.obj");

		std::vector<GameObject> scorecounter;
		{
			for (int i = 0; i < NUM_BOTTLES_ARENA; i++)//NUM_HITBOXES_TEST is located at the top of the code
			{
				scorecounter.push_back(Arena1->CreateEntity("scorecounter" + (std::to_string(i + 1))));
				if (i < 3)
					scorecounter[i].emplace<RendererComponent>().SetMesh(Emptyscore).SetMaterial(materialyellow);
				else
					scorecounter[i].emplace<RendererComponent>().SetMesh(Emptyscore).SetMaterial(materialpink);
			}

			scorecounter[0].get<Transform>().SetLocalPosition(4.5f, -7.5f, 8.0f);//Score1
			scorecounter[0].get<Transform>().SetLocalScale(2.5f, 2.5f, 2.5f);
			scorecounter[0].get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);

			scorecounter[1].get<Transform>().SetLocalPosition(6.5f, -7.5f, 8.0f);//Score2
			scorecounter[1].get<Transform>().SetLocalScale(2.5f, 2.5f, 2.5f);
			scorecounter[1].get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);

			scorecounter[2].get<Transform>().SetLocalPosition(8.5f, -7.5f, 8.0f);//Score3
			scorecounter[2].get<Transform>().SetLocalScale(2.5f, 2.5f, 2.5f);
			scorecounter[2].get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);

			scorecounter[3].get<Transform>().SetLocalPosition(-4.5f, -7.5f, 8.0f);//Score4
			scorecounter[3].get<Transform>().SetLocalScale(2.5f, 2.5f, 2.5f);
			scorecounter[3].get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);

			scorecounter[4].get<Transform>().SetLocalPosition(-6.5f, -7.5f, 8.0f);//Score5
			scorecounter[4].get<Transform>().SetLocalScale(2.5f, 2.5f, 2.5f);
			scorecounter[4].get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);

			scorecounter[5].get<Transform>().SetLocalPosition(-8.5f, -7.5f, 8.0f);//Score6
			scorecounter[5].get<Transform>().SetLocalScale(2.5f, 2.5f, 2.5f);
			scorecounter[5].get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);
		}

		VertexArrayObject::sptr FullBottle = ObjLoader::LoadFromFile("models/Arena1/waterBottle.obj");
		VertexArrayObject::sptr EmptyBottle = ObjLoader::LoadFromFile("models/Arena1/BottleOutline.obj");
		std::vector<GameObject> Bottles;
		{
			for (int i = 0; i < NUM_BOTTLES_ARENA; i++)//NUM_HITBOXES_TEST is located at the top of the code
			{
				Bottles.push_back(Arena1->CreateEntity("Bottle" + (std::to_string(i + 1))));
				Bottles[i].emplace<RendererComponent>().SetMesh(FullBottle).SetMaterial(materialwaterbottle);
			}

			Bottles[0].get<Transform>().SetLocalPosition(0.0f, 0.0f, 6.0f);//Middle
			Bottles[0].get<Transform>().SetLocalRotation(0.0f, 0.0f, 170.0f);
			Bottles[0].get<Transform>().SetLocalScale(0.75f, 0.75f, 0.75f);

			Bottles[1].get<Transform>().SetLocalPosition(-8.0f, -2.5f, 6.0f);//Top Right
			Bottles[1].get<Transform>().SetLocalRotation(0.0f, 0.0f, 110.0f);
			Bottles[1].get<Transform>().SetLocalScale(0.75f, 0.75f, 0.75f);

			Bottles[2].get<Transform>().SetLocalPosition(18.0f, -5.0f, 6.0f);//Top Left
			Bottles[2].get<Transform>().SetLocalRotation(0.0f, 0.0f, -75.0f);
			Bottles[2].get<Transform>().SetLocalScale(0.75f, 0.75f, 0.75f);

			Bottles[3].get<Transform>().SetLocalPosition(11.5f, 3.6f, 6.0f);//Bottom Left
			Bottles[3].get<Transform>().SetLocalRotation(0.0f, 0.0f, 35.0f);
			Bottles[3].get<Transform>().SetLocalScale(0.75f, 0.75f, 0.75f);

			Bottles[4].get<Transform>().SetLocalPosition(-13.6f, 5.0f, 6.0f);//Bottom Right
			Bottles[4].get<Transform>().SetLocalRotation(0.0f, 0.0f, -45.0f);
			Bottles[4].get<Transform>().SetLocalScale(0.75f, 0.75f, 0.75f);

			Bottles[5].get<Transform>().SetLocalPosition(4.0f, 4.5f, 9.0f);//Player 1 ammo
			Bottles[5].get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);
			Bottles[5].get<Transform>().SetLocalScale(1.5f, 1.5f, 1.5f);

			Bottles[6].get<Transform>().SetLocalPosition(-7.0f, 4.5f, 9.0f);//Player 2 ammo
			Bottles[6].get<Transform>().SetLocalRotation(0.0f, 0.0f, 180.0f);
			Bottles[6].get<Transform>().SetLocalScale(1.5f, 1.5f, 1.5f);
			
		}
		
		GameObject objBullet = Arena1->CreateEntity("Bullet1");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena1/waterBeam.obj");
			objBullet.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialdropwater);
			objBullet.get<Transform>().SetLocalPosition(8.0f, 6.0f, 0.0f);
			objBullet.get<Transform>().SetLocalScale(0.25f, 0.25f, 0.25f);
		}
		
		GameObject objBullet2 = Arena1->CreateEntity("Bullet2");
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/Arena1/waterBeam.obj");
			objBullet2.emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialdropwater);
			objBullet2.get<Transform>().SetLocalPosition(-8.0f, 6.0f, 0.0f);
			objBullet2.get<Transform>().SetLocalScale(0.25f, 0.25f, 0.25f);
		}

		//HitBoxes generated using a for loop then each one is given a position
		std::vector<GameObject> HitboxesArena;
		{
			VertexArrayObject::sptr vao = ObjLoader::LoadFromFile("models/TestScene/HitBox.obj");
			for (int i = 0; i < NUM_HITBOXES; i++)//NUM_HITBOXES_TEST is located at the top of the code
			{
				HitboxesArena.push_back(Arena1->CreateEntity("Hitbox" + (std::to_string(i + 1))));
				//HitboxesArena[i].emplace<RendererComponent>().SetMesh(vao).SetMaterial(materialTreeBig);//Material does not matter just invisable hitboxes
			}

			HitboxesArena[0].get<Transform>().SetLocalPosition(-10.75f, 2.5f, 6.0f);//Roundabout
			HitboxesArena[0].get<Transform>().SetLocalScale(3.0f, 3.0f, 1.0f);//Roundabout
			HitboxesArena[1].get<Transform>().SetLocalPosition(10.0f, -3.0f, 6.0f);//Slide
			HitboxesArena[1].get<Transform>().SetLocalScale(4.0f, 1.25f, 1.0f);//Slide scale
			HitboxesArena[2].get<Transform>().SetLocalPosition(2.0f, -2.0f, 6.0f);//leftmonkeybar
			HitboxesArena[2].get<Transform>().SetLocalScale(0.5f, 0.75f, 1.0f);//leftmonkeybar scale
			HitboxesArena[3].get<Transform>().SetLocalPosition(-2.0f, -2.0f, 6.0f);//rightmonkebar
			HitboxesArena[3].get<Transform>().SetLocalScale(0.1f, 0.75f, 1.0f);//rightmonkebar scale
			HitboxesArena[4].get<Transform>().SetLocalPosition(-1.5f, 1.0f, 6.0f);//swing
			HitboxesArena[4].get<Transform>().SetLocalScale(3.25f, 1.75f, 1.0f);//swing scale
			HitboxesArena[5].get<Transform>().SetLocalPosition(-12.0f, -5.0f, 6.0f);//table top
			HitboxesArena[5].get<Transform>().SetLocalScale(2.0f, 2.0f, 1.5f);//table top
			HitboxesArena[6].get<Transform>().SetLocalPosition(-14.0f, -1.0f, 6.0f);//table right down
			HitboxesArena[6].get<Transform>().SetLocalScale(2.0f, 1.75f, 1.5f);//table right down scale
			HitboxesArena[7].get<Transform>().SetLocalPosition(-9.0f, -1.0f, 6.0f);//table left
			HitboxesArena[7].get<Transform>().SetLocalScale(2.0f, 2.0f, 1.0f);//table right scale
			HitboxesArena[8].get<Transform>().SetLocalPosition(4.25f, 7.25f, 6.0f);//Bench left down
			HitboxesArena[8].get<Transform>().SetLocalScale(2.5f, 1.0f, 1.0f);//Bench left down scale
			HitboxesArena[9].get<Transform>().SetLocalPosition(4.25f, -6.25f, 6.0f);//bench left up
			HitboxesArena[9].get<Transform>().SetLocalScale(2.5f, 1.0f, 1.0f);//bench left up scale
			HitboxesArena[10].get<Transform>().SetLocalPosition(-6.0f, 7.0f, 6.0f);//bench right down
			HitboxesArena[10].get<Transform>().SetLocalScale(2.5f, 1.0f, 1.0f);//bench right down scale
			HitboxesArena[11].get<Transform>().SetLocalPosition(-6.0f, -6.25f, 6.0f);//bench right up
			HitboxesArena[11].get<Transform>().SetLocalScale(2.5f, 1.0f, 1.0f);//bench right up scale
			HitboxesArena[12].get<Transform>().SetLocalPosition(0.0f, 0.0f, 6.0f);//bottle middle
			HitboxesArena[13].get<Transform>().SetLocalPosition(-8.0f, -2.0f, 6.0f);//bottle bot Right
			HitboxesArena[14].get<Transform>().SetLocalPosition(10.0f, -5.0f, 6.0f);//bottle top Left
			HitboxesArena[15].get<Transform>().SetLocalPosition(12.0f, 3.5f, 6.0f);//bottle bot Left
			HitboxesArena[16].get<Transform>().SetLocalPosition(-15.0f, -8.5f, 6.0f);//top wall
			HitboxesArena[16].get<Transform>().SetLocalScale(35.0f, 1.0f, 1.0f);//top wall scale
			HitboxesArena[17].get<Transform>().SetLocalPosition(-15.0f, 9.0f, 6.0f);//bot wall
			HitboxesArena[17].get<Transform>().SetLocalScale(35.0f, 1.0f, 1.0f);//bot wall scale
			HitboxesArena[18].get<Transform>().SetLocalPosition(-18.0f, -10.0f, 6.0f);//left wall
			HitboxesArena[18].get<Transform>().SetLocalScale(1.0f, 20.0f, 1.0f);//left wall scale
			HitboxesArena[19].get<Transform>().SetLocalPosition(18.0f, -10.0f, 6.0f);//right wall
			HitboxesArena[19].get<Transform>().SetLocalScale(1.0f, 20.0f, 1.0f);//right wall
			HitboxesArena[20].get<Transform>().SetLocalPosition(-13.0f, 5.5f, 6.0f);//bottle Bot Right
		}

		#pragma region Animation Vaos

		//Dunce
		VertexArrayObject::sptr Dunce1 = ObjLoader::LoadFromFile("models/Animations/Dunce_1.obj");
		VertexArrayObject::sptr Dunce2 = ObjLoader::LoadFromFile("models/Animations/Dunce_2.obj");
		VertexArrayObject::sptr Dunce3 = ObjLoader::LoadFromFile("models/Animations/Dunce_3.obj");
		VertexArrayObject::sptr Dunce4 = ObjLoader::LoadFromFile("models/Animations/Dunce_4.obj");
		VertexArrayObject::sptr Dunce5 = ObjLoader::LoadFromFile("models/Animations/Dunce_5.obj");
		
		//Duncet
		VertexArrayObject::sptr Duncet1 = ObjLoader::LoadFromFile("models/Animations/Duncet_1.obj");
		VertexArrayObject::sptr Duncet2 = ObjLoader::LoadFromFile("models/Animations/Duncet_2.obj");
		VertexArrayObject::sptr Duncet3 = ObjLoader::LoadFromFile("models/Animations/Duncet_3.obj");
		VertexArrayObject::sptr Duncet4 = ObjLoader::LoadFromFile("models/Animations/Duncet_4.obj");
		VertexArrayObject::sptr Duncet5 = ObjLoader::LoadFromFile("models/Animations/Duncet_5.obj");

		//Bottle
		VertexArrayObject::sptr Bottle1 = ObjLoader::LoadFromFile("models/Animations/WaterBottle_frame_1.obj");
		VertexArrayObject::sptr Bottle2 = ObjLoader::LoadFromFile("models/Animations/WaterBottle_frame_2.obj");
		VertexArrayObject::sptr Bottle3 = ObjLoader::LoadFromFile("models/Animations/WaterBottle_frame_3.obj");
		VertexArrayObject::sptr Bottle4 = ObjLoader::LoadFromFile("models/Animations/WaterBottle_frame_4.obj");
		VertexArrayObject::sptr Bottle5 = ObjLoader::LoadFromFile("models/Animations/WaterBottle_frame_5.obj");
		VertexArrayObject::sptr Bottle6 = ObjLoader::LoadFromFile("models/Animations/WaterBottle_frame_6.obj");
		VertexArrayObject::sptr Bottle7 = ObjLoader::LoadFromFile("models/Animations/WaterBottle_frame_7.obj");
		VertexArrayObject::sptr Bottle8 = ObjLoader::LoadFromFile("models/Animations/WaterBottle_frame_8.obj");
		VertexArrayObject::sptr Bottle9 = ObjLoader::LoadFromFile("models/Animations/WaterBottle_frame_9.obj");
		VertexArrayObject::sptr Bottle10 = ObjLoader::LoadFromFile("models/Animations/WaterBottle_frame_10.obj");
		VertexArrayObject::sptr Bottle11 = ObjLoader::LoadFromFile("models/Animations/WaterBottle_frame_11.obj");

		//Swing
		VertexArrayObject::sptr Swing1 = ObjLoader::LoadFromFile("models/Animations/Swing1_21.obj");
		VertexArrayObject::sptr Swing2 = ObjLoader::LoadFromFile("models/Animations/Swing2.obj");
		VertexArrayObject::sptr Swing3 = ObjLoader::LoadFromFile("models/Animations/Swing3.obj");
		VertexArrayObject::sptr Swing4 = ObjLoader::LoadFromFile("models/Animations/Swing4.obj");
		VertexArrayObject::sptr Swing5 = ObjLoader::LoadFromFile("models/Animations/Swing5.obj");
		VertexArrayObject::sptr Swing6 = ObjLoader::LoadFromFile("models/Animations/Swing6.obj");
		VertexArrayObject::sptr Swing7 = ObjLoader::LoadFromFile("models/Animations/Swing7.obj");
		VertexArrayObject::sptr Swing8 = ObjLoader::LoadFromFile("models/Animations/Swing8.obj");
		VertexArrayObject::sptr Swing9 = ObjLoader::LoadFromFile("models/Animations/Swing9.obj");
		VertexArrayObject::sptr Swing10 = ObjLoader::LoadFromFile("models/Animations/Swing10.obj");
		VertexArrayObject::sptr Swing11 = ObjLoader::LoadFromFile("models/Animations/Swing11_12.obj");
		VertexArrayObject::sptr Swing12 = ObjLoader::LoadFromFile("models/Animations/Swing11_12.obj");
		VertexArrayObject::sptr Swing13 = ObjLoader::LoadFromFile("models/Animations/Swing13.obj");
		VertexArrayObject::sptr Swing14 = ObjLoader::LoadFromFile("models/Animations/Swing14.obj");
		VertexArrayObject::sptr Swing15 = ObjLoader::LoadFromFile("models/Animations/Swing15.obj");
		VertexArrayObject::sptr Swing16 = ObjLoader::LoadFromFile("models/Animations/Swing16.obj");
		VertexArrayObject::sptr Swing17 = ObjLoader::LoadFromFile("models/Animations/Swing17.obj");
		VertexArrayObject::sptr Swing18 = ObjLoader::LoadFromFile("models/Animations/Swing18.obj");
		VertexArrayObject::sptr Swing19 = ObjLoader::LoadFromFile("models/Animations/Swing19.obj");
		VertexArrayObject::sptr Swing20 = ObjLoader::LoadFromFile("models/Animations/Swing20.obj");
		VertexArrayObject::sptr Swing21 = ObjLoader::LoadFromFile("models/Animations/Swing1_21.obj");

		#pragma endregion Animation Vaos

		#pragma endregion Arena1 Objects New

		#pragma region PostEffects
		//Post Effects
		int width, height;
		glfwGetWindowSize(BackendHandler::window, &width, &height);

		int shadowWidth = 4096;
		int shadowHeight = 4096;

		GameObject gBufferObject = scene->CreateEntity("g Buffer");
		{
			gBuffer = &gBufferObject.emplace<GBuffer>();
			gBuffer->Init(width, height);
		}

		GameObject illuminationbufferObject = scene->CreateEntity("Illumination buffer");
		{
			illuminationBuffer = &illuminationbufferObject.emplace<IlluminationBuffer>();
			illuminationBuffer->Init(width, height);
		}

		GameObject shadowBufferObject = scene->CreateEntity("Shadow Buffer");
		{
			shadowBuffer = &shadowBufferObject.emplace<Framebuffer>();
			shadowBuffer->AddDepthTarget();
			shadowBuffer->Init(shadowWidth, shadowHeight);
		}

		GameObject framebufferObject = scene->CreateEntity("Basic Effect");
		{
			basicEffect = &framebufferObject.emplace<PostEffect>();
			basicEffect->Init(width, height);
		}

		GameObject blurEffectObject = scene->CreateEntity("Blur Effect");
		{
			blureffect = &blurEffectObject.emplace<BlurEffect>();
			blureffect->Init(width, height);
		}
		effects.push_back(blureffect);
		
		GameObject colorcorrectioneffectObject = scene->CreateEntity("color correction Effect");
		{
			colorCorrectioneffect = &colorcorrectioneffectObject.emplace<ColorCorrectEffect>();
			colorCorrectioneffect->Init(width, height);
		}
		effects.push_back(colorCorrectioneffect);
		
		GameObject GrainEffectObject = scene->CreateEntity("Grain Effect");
		{
			grainEffect = &GrainEffectObject.emplace<GrainEffect>();
			grainEffect->Init(width, height);
		}
		effects.push_back(grainEffect);
		
		GameObject PixelateEffectObject = scene->CreateEntity("Pixelate Effect");
		{
			pixelateeffect = &PixelateEffectObject.emplace<PixelateEffect>();
			pixelateeffect->Init(width, height);
		}
		effects.push_back(pixelateeffect);

		#pragma endregion PostEffects

		// Create an object to be our camera
		GameObject cameraObject = scene->CreateEntity("Camera");
		{
			cameraObject.get<Transform>().SetLocalPosition(0, 0.1, 23.5).LookAt(glm::vec3(0, 0, 0));

			// We'll make our camera a component of the camera object
			Camera& camera = cameraObject.emplace<Camera>();// Camera::Create();
			camera.SetPosition(glm::vec3(0, 3, 3));
			camera.SetUp(glm::vec3(0, 0, 1));
			camera.LookAt(glm::vec3(0));
			camera.SetFovDegrees(60.0f); // Set an initial FOV
			camera.SetOrthoHeight(3.0f);
			BehaviourBinding::Bind<CameraControlBehaviour>(cameraObject);
		}

		#pragma region Skybox
		/////////////////////////////////// SKYBOX ///////////////////////////////////////////////
			// Load our shaders
			Shader::sptr skybox = std::make_shared<Shader>();
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.vert.glsl", GL_VERTEX_SHADER);
			skybox->LoadShaderPartFromFile("shaders/skybox-shader.frag.glsl", GL_FRAGMENT_SHADER);
			skybox->Link();

			ShaderMaterial::sptr skyboxMat = ShaderMaterial::Create();
			skyboxMat->Shader = skybox;  
			skyboxMat->Set("s_Environment", environmentMap);
			skyboxMat->Set("u_EnvironmentRotation", glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0))));
			skyboxMat->RenderLayer = 100;

			MeshBuilder<VertexPosNormTexCol> mesh;
			MeshFactory::AddIcoSphere(mesh, glm::vec3(0.0f), 1.0f);
			MeshFactory::InvertFaces(mesh);
			VertexArrayObject::sptr meshVao = mesh.Bake();
			
			GameObject skyboxObj = scene->CreateEntity("skybox");  
			skyboxObj.get<Transform>().SetLocalPosition(0.0f, 0.0f, 0.0f);
			skyboxObj.get_or_emplace<RendererComponent>().SetMesh(meshVao).SetMaterial(skyboxMat);
		////////////////////////////////////////////////////////////////////////////////////////
		#pragma endregion Skybox

		// We'll use a vector to store all our key press events for now (this should probably be a behaviour eventually)
		std::vector<KeyPressWatcher> keyToggles;
		{
			// This is an example of a key press handling helper. Look at InputHelpers.h an .cpp to see
			// how this is implemented. Note that the ampersand here is capturing the variables within
			// the scope. If you wanted to do some method on the class, your best bet would be to give it a method and
			// use std::bind
			keyToggles.emplace_back(GLFW_KEY_T, [&]() { cameraObject.get<Camera>().ToggleOrtho(); });
			keyToggles.emplace_back(GLFW_KEY_F1, [&]() { drawGBuffer = !drawGBuffer; PositionBuffer = false; NormalBuffer = false; MaterialBuffer = false; lightaccumbuffer = false; });
			keyToggles.emplace_back(GLFW_KEY_F2, [&]() { drawIllumBuffer = !drawIllumBuffer; });
			keyToggles.emplace_back(GLFW_KEY_1, [&]() { PositionBuffer = false; NormalBuffer = false; MaterialBuffer = false; lightaccumbuffer = false; });
			keyToggles.emplace_back(GLFW_KEY_2, [&]() { PositionBuffer = !PositionBuffer; });
			keyToggles.emplace_back(GLFW_KEY_3, [&]() { NormalBuffer = !NormalBuffer; });
			keyToggles.emplace_back(GLFW_KEY_4, [&]() { MaterialBuffer = !MaterialBuffer; });
			keyToggles.emplace_back(GLFW_KEY_5, [&]() { lightaccumbuffer = !lightaccumbuffer; });

			controllables.push_back(objDunce);
			controllables.push_back(objDuncet);

			keyToggles.emplace_back(GLFW_KEY_KP_ADD, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao++;
				if (selectedVao >= controllables.size())
					selectedVao = 0;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});
			keyToggles.emplace_back(GLFW_KEY_KP_SUBTRACT, [&]() {
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = false;
				selectedVao--;
				if (selectedVao < 0)
					selectedVao = controllables.size() - 1;
				BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao])->Enabled = true;
				});

			keyToggles.emplace_back(GLFW_KEY_Y, [&]() {
				auto behaviour = BehaviourBinding::Get<SimpleMoveBehaviour>(controllables[selectedVao]);
				behaviour->Relative = !behaviour->Relative;
				});
		}
		
		// Initialize our timing instance and grab a reference for our use
		Timing& time = Timing::Instance();
		time.LastFrame = glfwGetTime();

		//Variables for gameplay and animation
		bool shoot = false, shoot2 = false, instructions = false, instructionspause = false, p1win = false, p2win = false;
		bool renderammoground1 = true, renderammoground2 = true, renderammoground3 = true, renderammoground4 = true, renderammoground5 = true, renderammo = true, renderammo2 = true, ammo = true, ammo2 = true;
		float bottletime1 = 0.0f, bottletime2 = 0.0f, bottletime3 = 0.0f, bottletime4 = 0.0f, bottletime5 = 0.0f, DunceTime = 0.0f, DuncetTime = 0.0f, SwingTime = 0.0f,AnimBottle = 0.0f, AnimBottle2 = 0.0f, infotime = 0.0f;
		bool animatebottle = false, animatebottle2 = false, infocheck = false;
		bool soundbottle1 = false, soundbottle2 = false, soundbottle3 = false, soundbottle4 = false, soundbottle5 = false, soundbottle6 = false, soundbottle7 = false, soundbottle8 = false, soundbottle9 = false, soundbottle10 = false;
		bool StartorInstructions = false;
		int score1 = 0, score2 = 0;
		float highscore1 = 0.0f, highscore2 = 0.0f, timesub1 = 0.0f, timesub2 = 0.0f;
		bool shaderbool = true;
		bool gbufferboolonoff = true;
		int screenstuff = 0;
		
		std::cout << "Longest shot distance highscores:\n";
		for (int i = 0; i < counter; i++) {
			std::cout << arrayscore[i] << "\n";
		}

		///// Game loop /////
		while (!glfwWindowShouldClose(BackendHandler::window)) {
			glfwPollEvents();

			// Update the timing
			time.CurrentFrame = glfwGetTime();
			time.DeltaTime = static_cast<float>(time.CurrentFrame - time.LastFrame);

			time.DeltaTime = time.DeltaTime > 1.0f ? 1.0f : time.DeltaTime;

			// Update our FPS tracker data
			fpsBuffer[frameIx] = 1.0f / time.DeltaTime;
			frameIx++;
			if (frameIx >= 128)
				frameIx = 0;

			// We'll make sure our UI isn't focused before we start handling input for our game
			if (!ImGui::IsAnyWindowFocused()) {
				// We need to poll our key watchers so they can do their logic with the GLFW state
				// Note that since we want to make sure we don't copy our key handlers, we need a const
				// reference!
				for (const KeyPressWatcher& watcher : keyToggles) {
					watcher.Poll(BackendHandler::window);
				}
			}

			Update(time.DeltaTime, p1win, p2win,
				Collision(objBullet.get<Transform>(), objDuncetArena.get<Transform>()), Collision(objBullet2.get<Transform>(), objDunceArena.get<Transform>()),
				soundbottle1, soundbottle2, soundbottle3, soundbottle4, soundbottle5,
				soundbottle6, soundbottle7, soundbottle8, soundbottle9, soundbottle10);

			// Clear the screen
			basicEffect->Clear();
			for (int i = 0; i < effects.size(); i++)
			{
				effects[i]->Clear();
			}
			shadowBuffer->Clear();
			gBuffer->Clear();
			illuminationBuffer->Clear();

			glClearColor(0.08f, 0.17f, 0.31f, 0.3f);
			glEnable(GL_DEPTH_TEST);
			glClearDepth(1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			#pragma region Rendering seperate scenes

			// Start by assuming no shader or material is applied
			Shader::sptr current = nullptr;
			ShaderMaterial::sptr currentMat = nullptr;

			// Grab out camera info from the camera object
			Transform& camTransform = cameraObject.get<Transform>();
			glm::mat4 view = glm::inverse(camTransform.LocalTransform());
			glm::mat4 projection = cameraObject.get<Camera>().GetProjection();
			glm::mat4 viewProjection = projection * view;

			//Set up light space matrix
			glm::mat4 lightProjectionMatrix = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, -100.0f, 100.0f);
			glm::mat4 lightViewMatrix = glm::lookAt(glm::vec3(-illuminationBuffer->GetSunRef()._lightDirection), glm::vec3(), glm::vec3(0.0f, 0.0f, 1.0f));
			glm::mat4 lightSpaceViewProj = lightProjectionMatrix * lightViewMatrix;

			//Set shadow stuff
			illuminationBuffer->SetLightSpaceViewProj(lightSpaceViewProj);
			glm::vec3 camPos = glm::inverse(view) * glm::vec4(0, 0, 0, 1);
			illuminationBuffer->SetCamPos(camPos);

			#pragma region Menu
			if (Application::Instance().ActiveScene == Menu) {
				p1win = false;
				p2win = false;
				score1 = 0;
				score2 = 0;
				player1w.get<Transform>().SetLocalPosition(0.0f, 0.0f, -2.0f);
				player2w.get<Transform>().SetLocalPosition(0.0f, 0.0f, -2.0f);
				objDunceArena.get<Transform>().SetLocalPosition(7.0f, -2.0f, 5.0f);
				objDuncetArena.get<Transform>().SetLocalPosition(-6.0f, 3.0f, 5.0f);
				objDunceArena.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
				objDuncetArena.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
				renderammoground1 = true;
				renderammoground2 = true;
				renderammoground3 = true;
				renderammoground4 = true;
				renderammoground5 = true;
				ammo = true;
				ammo2 = true;
				int controller1 = glfwJoystickPresent(GLFW_JOYSTICK_1);

				//player 1 controller
				if (controller1 == 1)
				{
					int axesCount;
					int buttonCount;
					const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axesCount);
					const unsigned char* button = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttonCount);

					if (axes[1] >= -1 && axes[1] <= -0.2 && !instructions) {
						StartorInstructions = false;
						Gun.get<Transform>().SetLocalPosition(-9.0f, 0.0f, 2.0f);
					}
					if (axes[1] <= 1 && axes[1] >= 0.2 && !instructions) {
						StartorInstructions = true;
						Gun.get<Transform>().SetLocalPosition(-12.0f, 4.5f, 2.0f);
					}
					if (button[0] == GLFW_PRESS && !StartorInstructions)
					{
						Application::Instance().ActiveScene = Arena1;
						p1win = false;
						p2win = false;
						score1 = 0;
						score2 = 0;
						player1w.get<Transform>().SetLocalPosition(0.0f, 0.0f, -2.0f);
						player2w.get<Transform>().SetLocalPosition(0.0f, 0.0f, -2.0f);
						objDunceArena.get<Transform>().SetLocalPosition(7.0f, -2.0f, 5.0f);
						objDuncetArena.get<Transform>().SetLocalPosition(-6.0f, 3.0f, 5.0f);
						objDunceArena.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
						objDuncetArena.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
						renderammoground1 = true;
						renderammoground2 = true;
						renderammoground3 = true;
						renderammoground4 = true;
						renderammoground5 = true;
						ammo = true;
						ammo2 = true;
					}
					if (button[0] == GLFW_PRESS && StartorInstructions)
					{
						instructions = true;
						Controls.get<Transform>().SetLocalPosition(0.0f, 0.0f, 2.0f);
						objMenu.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
						Gun.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
					}
					if (button[1] == GLFW_PRESS)
					{
						instructions = false;
						Controls.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
						objMenu.get<Transform>().SetLocalPosition(0.0f, 0.0f, 2.0f);
						info.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
						Gun.get<Transform>().SetLocalPosition(-12.0f, 4.5f, 2.0f);
					}
					if (axes[0] >= -1 && axes[0] <= -0.2 && instructions) {
						Controls.get<Transform>().SetLocalPosition(0.0f, 0.0f, 2.0f);
						info.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
						ControlsMenu->Set("u_TextureMix", 1.0f);
					}
					else if (axes[0] <= 1 && axes[0] >= 0.2 && instructions) {
						Controls.get<Transform>().SetLocalPosition(0.0f, 0.0f, 2.0f);
						info.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
						ControlsMenu->Set("u_TextureMix", 0.0f);
					}
					else if (instructions)
					{
						Controls.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
						info.get<Transform>().SetLocalPosition(0.0f, 0.0f, 2.0f);
					}
				}
				else {

					if (glfwGetKey(BackendHandler::window, GLFW_KEY_W) == GLFW_PRESS && !instructions) {
						StartorInstructions = false;
						Gun.get<Transform>().SetLocalPosition(-9.0f, 0.0f, 2.0f);
					}
					if (glfwGetKey(BackendHandler::window, GLFW_KEY_S) == GLFW_PRESS && !instructions) {
						StartorInstructions = true;
						Gun.get<Transform>().SetLocalPosition(-12.0f, 4.5f, 2.0f);
					}

					if (glfwGetKey(BackendHandler::window, GLFW_KEY_ENTER) == GLFW_PRESS && !StartorInstructions)
					{
						Application::Instance().ActiveScene = Arena1;
						p1win = false;
						p2win = false;
						score1 = 0;
						score2 = 0;
						player1w.get<Transform>().SetLocalPosition(0.0f, 0.0f, -2.0f);
						player2w.get<Transform>().SetLocalPosition(0.0f, 0.0f, -2.0f);
						objDunceArena.get<Transform>().SetLocalPosition(7.0f, -2.0f, 5.0f);
						objDuncetArena.get<Transform>().SetLocalPosition(-6.0f, 3.0f, 5.0f);
						objDunceArena.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
						objDuncetArena.get<Transform>().SetLocalRotation(90.0f, 0.0f, 0.0f);
						renderammoground1 = true;
						renderammoground2 = true;
						renderammoground3 = true;
						renderammoground4 = true;
						renderammoground5 = true;
						ammo = true;
						ammo2 = true;
					}

					if (glfwGetKey(BackendHandler::window, GLFW_KEY_ENTER) == GLFW_PRESS && StartorInstructions)
					{
						instructions = true;
						Controls.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
						objMenu.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
						info.get<Transform>().SetLocalPosition(0.0f, 0.0f, 2.0f);
						Gun.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
					}
					if (glfwGetKey(BackendHandler::window, GLFW_KEY_BACKSPACE) == GLFW_PRESS)
					{
						instructions = false;
						Controls.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
						info.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
						objMenu.get<Transform>().SetLocalPosition(0.0f, 0.0f, 2.0f);
						Gun.get<Transform>().SetLocalPosition(-12.0f, 4.5f, 2.0f);
					}

					if (glfwGetKey(BackendHandler::window, GLFW_KEY_A) == GLFW_PRESS && instructions)
					{
						Controls.get<Transform>().SetLocalPosition(0.0f, 0.0f, 2.0f);
						info.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
						ControlsMenu->Set("u_TextureMix", 1.0f);
					}
					else if (glfwGetKey(BackendHandler::window, GLFW_KEY_D) == GLFW_PRESS && instructions)
					{
						Controls.get<Transform>().SetLocalPosition(0.0f, 0.0f, 2.0f);
						info.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
						ControlsMenu->Set("u_TextureMix", 0.0f);
					}
					else if(instructions)
					{
						Controls.get<Transform>().SetLocalPosition(0.0f, 0.0f, 1.0f);
						info.get<Transform>().SetLocalPosition(0.0f, 0.0f, 2.0f);
					}
				}

				if (glfwGetKey(BackendHandler::window, GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS)
				{
					Application::Instance().ActiveScene = scene;//just to test change to arena1 later
				}

				shader->SetUniform("u_lightoff", lightoff = 1);
				shader->SetUniform("u_ambient", ambientonly = 0);
				shader->SetUniform("u_specular", specularonly = 0);
				shader->SetUniform("u_ambientspecular", ambientandspecular = 0);
				shader->SetUniform("u_ambientspeculartoon", ambientspeculartoon = 0);
				shader->SetUniform("u_Textures", Textures = 2);

				// Iterate over all the behaviour binding components
				Menu->Registry().view<BehaviourBinding>().each([&](entt::entity entity, BehaviourBinding& binding) {
					// Iterate over all the behaviour scripts attached to the entity, and update them in sequence (if enabled)
					for (const auto& behaviour : binding.Behaviours) {
						if (behaviour->Enabled) {
							behaviour->Update(entt::handle(Menu->Registry(), entity));
						}
					}
					});

				// Update all world matrices for this frame
				Menu->Registry().view<Transform>().each([](entt::entity entity, Transform& t) {
					t.UpdateWorldMatrix();
					});

				// Sort the renderers by shader and material, we will go for a minimizing context switches approach here,
			// but you could for instance sort front to back to optimize for fill rate if you have intensive fragment shaders
				renderGroupMenu.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
					// Sort by render layer first, higher numbers get drawn last
					if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
					if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

					// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
					if (l.Material->Shader < r.Material->Shader) return true;
					if (l.Material->Shader > r.Material->Shader) return false;

					// Sort by material pointer last (so we can minimize switching between materials)
					if (l.Material < r.Material) return true;
					if (l.Material > r.Material) return false;

					return false;
					});

				// Iterate over the render group components and draw them
				renderGroupMenu.each([&](entt::entity e, RendererComponent& renderer, Transform& transform) {
					// If the shader has changed, set up it's uniforms
					if (current != renderer.Material->Shader) {
						current = renderer.Material->Shader;
						current->Bind();
						BackendHandler::SetupShaderForFrame(current, view, projection);
					}
					// If the material has changed, apply it
					if (currentMat != renderer.Material) {
						currentMat = renderer.Material;
						currentMat->Apply();
					}
					// Render the mesh
					BackendHandler::RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					});
			}
			#pragma endregion Menu

			#pragma region scene(testing)
			if (Application::Instance().ActiveScene == scene) {

				if (glfwGetKey(BackendHandler::window, GLFW_KEY_M) == GLFW_PRESS)
				{
					shaderbool = true;
				}
				if (glfwGetKey(BackendHandler::window, GLFW_KEY_N) == GLFW_PRESS)
				{
					shaderbool = false;
				}

				if (shaderbool) {
					materialTable->Shader = shader;
					materialGround->Shader = shader;
					materialDunce->Shader = shader;
					materialDuncet->Shader = shader;
					materialredballoon->Shader = shader;
					materialyellowballoon->Shader = shader;
					materialSlide->Shader = shader;
					materialSwing->Shader = shader;
				}
				else
				{
					materialTable->Shader = gBufferShader;
					materialGround->Shader = gBufferShader;
					materialDunce->Shader = gBufferShader;
					materialDuncet->Shader = gBufferShader;
					materialredballoon->Shader = gBufferShader;
					materialyellowballoon->Shader = gBufferShader;
					materialSlide->Shader = gBufferShader;
					materialSwing->Shader = gBufferShader;
				}

				if (glfwGetKey(BackendHandler::window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
				{
					Application::Instance().ActiveScene = Pause;
					materialTable->Shader = shader;
					materialGround->Shader = shader;
					materialDunce->Shader = shader;
					materialDuncet->Shader = shader;
					materialredballoon->Shader = shader;
					materialyellowballoon->Shader = shader;
					materialSlide->Shader = shader;
					materialSwing->Shader = shader;
				}

				//Player Movemenet(seperate from camera controls)
				PlayerMovement::player1and2move(objDunce.get<Transform>(), objDuncet.get<Transform>(), time.DeltaTime);

				// Iterate over all the behaviour binding components
				scene->Registry().view<BehaviourBinding>().each([&](entt::entity entity, BehaviourBinding& binding) {
					// Iterate over all the behaviour scripts attached to the entity, and update them in sequence (if enabled)
					for (const auto& behaviour : binding.Behaviours) {
						if (behaviour->Enabled) {
							behaviour->Update(entt::handle(scene->Registry(), entity));
						}
					}
				});

				// Update all world matrices for this frame
				scene->Registry().view<Transform>().each([](entt::entity entity, Transform& t) {
					t.UpdateWorldMatrix();
				});

				// Sort the renderers by shader and material, we will go for a minimizing context switches approach here,
			// but you could for instance sort front to back to optimize for fill rate if you have intensive fragment shaders
				renderGroup.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
					// Sort by render layer first, higher numbers get drawn last
					if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
					if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

					// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
					if (l.Material->Shader < r.Material->Shader) return true;
					if (l.Material->Shader > r.Material->Shader) return false;

					// Sort by material pointer last (so we can minimize switching between materials)
					if (l.Material < r.Material) return true;
					if (l.Material > r.Material) return false;

					return false;
					});

				if (shaderbool)
				{
					basicEffect->BindBuffer(0);
				}
				else {
					glViewport(0, 0, shadowWidth, shadowHeight);
					shadowBuffer->Bind();

					renderGroup.each([&](entt::entity e, RendererComponent& renderer, Transform& transform) {
						// Render the mesh
						if (renderer.CastShadows)
						{
							BackendHandler::RenderVAO(simpleDepthShader, renderer.Mesh, viewProjection, transform, lightSpaceViewProj);
						}
						});

					shadowBuffer->Unbind();
					glfwGetWindowSize(BackendHandler::window, &width, &height);

					glViewport(0, 0, width, height);



					gBuffer->Bind();
				}

				// Iterate over the render group components and draw them
				renderGroup.each([&](entt::entity e, RendererComponent& renderer, Transform& transform) {
					// If the shader has changed, set up it's uniforms
					if (current != renderer.Material->Shader) {
						current = renderer.Material->Shader;
						current->Bind();
						BackendHandler::SetupShaderForFrame(current, view, projection);
					}
					// If the material has changed, apply it
					if (currentMat != renderer.Material) {
						currentMat = renderer.Material;
						currentMat->Apply();
					}

					if (!shaderbool) {
						shadowBuffer->BindDepthAsTexture(30);
					}

					// Render the mesh
					BackendHandler::RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform, lightSpaceViewProj);
				});

				if (shaderbool)
				{
					basicEffect->UnbindBuffer();

					effects[activeEffect]->ApplyEffect(basicEffect);

					effects[activeEffect]->DrawToScreen();
				}
				else {

					gBuffer->Unbind();

					illuminationBuffer->BindBuffer(0);

					skybox->Bind();
					BackendHandler::SetupShaderForFrame(skybox, view, projection);
					skyboxMat->Apply();
					BackendHandler::RenderVAO(skybox, meshVao, viewProjection, skyboxObj.get<Transform>(), lightSpaceViewProj);
					skybox->UnBind();

					illuminationBuffer->UnbindBuffer();

					shadowBuffer->BindDepthAsTexture(30);

					illuminationBuffer->ApplyEffect(gBuffer);

					shadowBuffer->UnbindTexture(30);

					if (drawGBuffer)
					{
						gBuffer->DrawBuffersToScreen();
					}
					else if (drawIllumBuffer)
					{
						illuminationBuffer->DrawIllumBuffer();
					}
					else if (PositionBuffer)
					{
						gBuffer->DrawBuffersToScreenSeperate(PositionBuffer, NormalBuffer, MaterialBuffer, lightaccumbuffer);
					}
					else if (NormalBuffer)
					{
						gBuffer->DrawBuffersToScreenSeperate(PositionBuffer, NormalBuffer, MaterialBuffer, lightaccumbuffer);
					}
					else if (MaterialBuffer)
					{
						gBuffer->DrawBuffersToScreenSeperate(PositionBuffer, NormalBuffer, MaterialBuffer, lightaccumbuffer);
					}
					else if (lightaccumbuffer)
					{
						gBuffer->DrawBuffersToScreenSeperate(PositionBuffer, NormalBuffer, MaterialBuffer, lightaccumbuffer);
					}
					else
					{
						illuminationBuffer->DrawToScreen();
					}
				}

				//gBuffer->DrawBuffersToScreen();

				BackendHandler::RenderImGui();
			}
			#pragma endregion scene(testing)

			#pragma region Arena 1 scene stuff
			if (Application::Instance().ActiveScene == Arena1)
			{
				camTransform = cameraObject.get<Transform>().SetLocalPosition(0, 0, 23.5).SetLocalRotation(0, 0, 180);//17
				view = glm::inverse(camTransform.LocalTransform());
				projection = cameraObject.get<Camera>().GetProjection();
				viewProjection = projection * view;

				if (glfwGetKey(BackendHandler::window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
				{
					Application::Instance().ActiveScene = Pause;
				}

				shader->SetUniform("u_lightoff", lightoff = 0);
				shader->SetUniform("u_ambient", ambientonly = 0);
				shader->SetUniform("u_specular", specularonly = 0);
				shader->SetUniform("u_ambientspecular", ambientandspecular = 0);
				shader->SetUniform("u_ambientspeculartoon", ambientspeculartoon = 1);
				shader->SetUniform("u_Textures", Textures = 2);

				#pragma region BIG MESSY SPAGHETTI CODE AGAIN
				#pragma region animations and switching textures
				//rerenders bottles checks
				if (!renderammoground1)
				{
					bottletime1 += time.DeltaTime;
					if (bottletime1 >= 5.0f)
					{
						renderammoground1 = true;
						bottletime1 = 0.0f;
					}
				}
				if (!renderammoground2)
				{
					bottletime2 += time.DeltaTime;
					if (bottletime2 >= 5.0f)
					{
						renderammoground2 = true;
						bottletime2 = 0.0f;
					}
				}
				if (!renderammoground3)
				{
					bottletime3 += time.DeltaTime;
					if (bottletime3 >= 5.0f)
					{
						renderammoground3 = true;
						bottletime3 = 0.0f;
					}
				}
				if (!renderammoground4)
				{
					bottletime4 += time.DeltaTime;
					if (bottletime4 >= 5.0f)
					{
						renderammoground4 = true;
						bottletime4 = 0.0f;
					}
				}
				if (!renderammoground5)
				{
					bottletime5 += time.DeltaTime;
					if (bottletime5 >= 5.0f)
					{
						renderammoground5 = true;
						bottletime5 = 0.0f;
					}
				}

				objRoundAboutA1.get<Transform>().RotateLocal(0.0f, 90.0f * time.DeltaTime, 0.0f);
				Bottles[0].get<Transform>().RotateLocal(0.0f, 0.0f, 90.0f * time.DeltaTime);
				Bottles[1].get<Transform>().RotateLocal(0.0f, 0.0f, 90.0f * time.DeltaTime);
				Bottles[2].get<Transform>().RotateLocal(0.0f, 0.0f, 90.0f * time.DeltaTime);
				Bottles[3].get<Transform>().RotateLocal(0.0f, 0.0f, 90.0f * time.DeltaTime);
				Bottles[4].get<Transform>().RotateLocal(0.0f, 0.0f, 90.0f * time.DeltaTime);

				//Dunce animation
				DunceTime += time.DeltaTime;

				if (DunceTime < 0.2f)
				{
					objDunceArena.get<RendererComponent>().SetMesh(Dunce1);
				}
				if (DunceTime >= 0.2f && DunceTime < 0.4f) {
					objDunceArena.get<RendererComponent>().SetMesh(Dunce2);
				}
				if (DunceTime >= 0.4f && DunceTime < 0.6f) {
					objDunceArena.get<RendererComponent>().SetMesh(Dunce1);
				}
				if (DunceTime >= 0.6f && DunceTime < 0.8f) {
					objDunceArena.get<RendererComponent>().SetMesh(Dunce4);
				}
				if (DunceTime >= 0.8f && DunceTime < 1.0f) {
					objDunceArena.get<RendererComponent>().SetMesh(Dunce5);
				}
				if (DunceTime >= 1.0f)
				{
					DunceTime = 0.0f;
					objDunceArena.get<RendererComponent>().SetMesh(Dunce1);
				}
				
				//Duncet animation
				DuncetTime += time.DeltaTime;
				if (DuncetTime < 0.2f)
				{
					objDuncetArena.get<RendererComponent>().SetMesh(Duncet1);
				}
				if (DuncetTime >= 0.2f && DuncetTime < 0.4f) {
					objDuncetArena.get<RendererComponent>().SetMesh(Duncet2);
				}
				if (DuncetTime >= 0.4f && DuncetTime < 0.6f) {
					objDuncetArena.get<RendererComponent>().SetMesh(Duncet1);
				}
				if (DuncetTime >= 0.6f && DuncetTime < 0.8f) {
					objDuncetArena.get<RendererComponent>().SetMesh(Duncet4);
				}
				if (DuncetTime >= 0.8f && DuncetTime < 1.0f) {
					objDuncetArena.get<RendererComponent>().SetMesh(Duncet5);
				}
				if (DuncetTime >= 1.0f)
				{
					DuncetTime = 0.0f;
					objDuncetArena.get<RendererComponent>().SetMesh(Duncet1);
				}

				//Swing animation
				SwingTime += time.DeltaTime;
				if (SwingTime < 0.1f)
				{
					objSwingA1.get<RendererComponent>().SetMesh(Swing1);
				}
				if (SwingTime >= 0.1f && SwingTime < 0.2f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing2);
				}
				if (SwingTime >= 0.2f && SwingTime < 0.3f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing3);
				}
				if (SwingTime >= 0.3f && SwingTime < 0.4f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing4);
				}
				if (SwingTime >= 0.4f && SwingTime < 0.5f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing5);
				}
				if (SwingTime >= 0.5f && SwingTime < 0.6f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing6);
				}
				if (SwingTime >= 0.6f && SwingTime < 0.7f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing7);
				}
				if (SwingTime >= 0.7f && SwingTime < 0.8f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing8);
				}
				if (SwingTime >= 0.8f && SwingTime < 0.9f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing9);
				}
				if (SwingTime >= 0.9f && SwingTime < 1.0f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing10);
				}
				if (SwingTime >= 1.0f && SwingTime < 1.1f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing11);
				}
				if (SwingTime >= 1.1f && SwingTime < 1.2f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing12);
				}
				if (SwingTime >= 1.2f && SwingTime < 1.3f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing13);
				}
				if (SwingTime >= 1.3f && SwingTime < 1.4f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing14);
				}
				if (SwingTime >= 1.4f && SwingTime < 1.5f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing15);
				}
				if (SwingTime >= 1.5f && SwingTime < 1.6f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing16);
				}
				if (SwingTime >= 1.6f && SwingTime < 1.7f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing17);
				}
				if (SwingTime >= 1.7f && SwingTime < 1.8f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing18);
				}
				if (SwingTime >= 1.8f && SwingTime < 1.9f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing19);
				}
				if (SwingTime >= 1.9f && SwingTime < 2.0f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing20);
				}
				if (SwingTime >= 2.0f && SwingTime < 2.1f) {
					objSwingA1.get<RendererComponent>().SetMesh(Swing21);
				}
				if (SwingTime >= 2.1f)
				{
					SwingTime = 0.0f;
					objSwingA1.get<RendererComponent>().SetMesh(Swing1);
				}

				#pragma endregion animations and switching textures

				//Player Movemenet(seperate from camera controls) has to be above collisions or wont work
				PlayerMovement::player1and2move(objDunceArena.get<Transform>(), objDuncetArena.get<Transform>(), time.DeltaTime);

				#pragma region Shooting
				//Player 1
				PlayerMovement::Shoot(objBullet.get<Transform>(), objDunceArena.get<Transform>(), time.DeltaTime, shoot);
				int controller1 = glfwJoystickPresent(GLFW_JOYSTICK_1);

				//controller input
				if (1 == controller1) {
					int buttonCount;
					int axesCount;
					const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axesCount);
					const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttonCount);

					if (ammo) {
						if (axes[5] >= 0.3) {
							shoot = true;
							animatebottle = true;
							objBullet.get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
						}
					}
					else {
						objBullet.get<Transform>().SetLocalPosition(objDunceArena.get<Transform>().GetLocalPosition());
						objBullet.get<Transform>().SetLocalScale(0.01f, 0.01f, 0.01f);
					}
					if (p1win)
					{
						if (buttons[7] == GLFW_PRESS)
						{
							highscores.open("highscores.txt", std::ios::app);
							highscores << highscore1 << "\n";
							highscores.close();
							highscore1 = 0.0f;
							highscore2 = 0.0f;
							Application::Instance().ActiveScene = Menu;
						}
					}
					else {
						if (buttons[7] == GLFW_PRESS)
						{
							Application::Instance().ActiveScene = Pause;
						}
					}
					
				}
				//Keyboard input
				else {
					if (ammo) {
						if (glfwGetKey(BackendHandler::window, GLFW_KEY_E) == GLFW_PRESS)
						{
							shoot = true;
							animatebottle = true;
							objBullet.get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
						}
					}
					else {
						objBullet.get<Transform>().SetLocalPosition(objDunceArena.get<Transform>().GetLocalPosition());
						objBullet.get<Transform>().SetLocalScale(0.01f, 0.01f, 0.01f);
					}
				}
				//Resets bullet position
				for (int i = 0; i < NUM_HITBOXES; i++) {
					if (i != 12 && i != 13 && i != 14 && i != 15 && i != 20) {
						if (shoot) {
							if (Collision(objBullet.get<Transform>(), HitboxesArena[i].get<Transform>())) {
								shoot = false;
								ammo = false;
								timesub1 = 0.0f;
							}
						}
					}
				}
				if (shoot) {
					timesub1 += time.DeltaTime * 60;
					if (Collision(objBullet.get<Transform>(), objDuncetArena.get<Transform>())) {
						shoot = false;
						ammo = false;
						score1 += 1;
						highscore1 += 100 + timesub1;
						timesub1 = 0.0f;
					}
				}

				if (score1 >= 1)
				{
					scorecounter[0].get<RendererComponent>().SetMesh(Fullscore);
				}
				else
				{
					scorecounter[0].get<RendererComponent>().SetMesh(Emptyscore);

				}
				if (score1 >= 2)
				{
					scorecounter[1].get<RendererComponent>().SetMesh(Fullscore);
				}
				else {
					scorecounter[1].get<RendererComponent>().SetMesh(Emptyscore);
				}
				if (score1 >= 3)
				{
					scorecounter[2].get<RendererComponent>().SetMesh(Fullscore);
					p1win = true;
				}
				else
				{
					scorecounter[2].get<RendererComponent>().SetMesh(Emptyscore);
				}
				if (p1win)
				{
					player1w.get<Transform>().SetLocalPosition(0.0f, 0.0f, 9.0f);
					if (glfwGetKey(BackendHandler::window, GLFW_KEY_SPACE) == GLFW_PRESS)
					{
						highscores.open("highscores.txt", std::ios::app);
						highscores << highscore1 << "\n";
						highscores.close();
						highscore1 = 0.0f;
						highscore2 = 0.0f;
						Application::Instance().ActiveScene = Menu;
					}
				}

				//Player2
				PlayerMovement::Shoot2(objBullet2.get<Transform>(), objDuncetArena.get<Transform>(), time.DeltaTime, shoot2);
				int controller2 = glfwJoystickPresent(GLFW_JOYSTICK_2);

				//controller input
				if (1 == controller2) {
					int buttonCount2;
					int axesCount2;
					const float* axes2 = glfwGetJoystickAxes(GLFW_JOYSTICK_2, &axesCount2);
					const unsigned char* buttons2 = glfwGetJoystickButtons(GLFW_JOYSTICK_2, &buttonCount2);

					if (ammo2) {
						if (axes2[5] >= 0.3) {
							shoot2 = true;
							animatebottle2 = true;
							objBullet2.get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
						}
					}
					else {
						objBullet2.get<Transform>().SetLocalPosition(objDuncetArena.get<Transform>().GetLocalPosition());
						objBullet2.get<Transform>().SetLocalScale(0.01f, 0.01f, 0.01f);
					}
					if (p2win)
					{
						if (buttons2[7] == GLFW_PRESS)
						{
							highscores.open("highscores.txt", std::ios::app);
							highscores << highscore2 << "\n";
							highscores.close();
							highscore1 = 0.0f;
							highscore2 = 0.0f;
							Application::Instance().ActiveScene = Menu;
						}
					}
					else {
						if (buttons2[7] == GLFW_PRESS)
						{
						Application::Instance().ActiveScene = Pause;
						}
					}
					
				}
				//Keyboard input
				else {
					if (1 == controller1) {
						if (ammo2) {
							if (glfwGetKey(BackendHandler::window, GLFW_KEY_E) == GLFW_PRESS)
							{
								shoot2 = true;
								animatebottle2 = true;
								objBullet2.get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
							}
						}
						else {
							objBullet2.get<Transform>().SetLocalPosition(objDuncetArena.get<Transform>().GetLocalPosition());
							objBullet2.get<Transform>().SetLocalScale(0.01f, 0.01f, 0.01f);
						}
					}
					else {

						if (ammo2) {
							if (glfwGetKey(BackendHandler::window, GLFW_KEY_O) == GLFW_PRESS)
							{
								shoot2 = true;
								animatebottle2 = true;
								objBullet2.get<Transform>().SetLocalScale(1.0f, 1.0f, 1.0f);
							}
						}
						else {
							objBullet2.get<Transform>().SetLocalPosition(objDuncetArena.get<Transform>().GetLocalPosition());
							objBullet2.get<Transform>().SetLocalScale(0.01f, 0.01f, 0.01f);
						}
					}
				}
				for (int i = 0; i < NUM_HITBOXES; i++) {
					if (i != 12 && i != 13 && i != 14 && i != 15 && i !=20) {
						if (shoot2) {
							if (Collision(objBullet2.get<Transform>(), HitboxesArena[i].get<Transform>())) {
								shoot2 = false;
								ammo2 = false;
								timesub2 = 0.0f;
							}
						}
					}
				}
				if (shoot2) {
					timesub2 += time.DeltaTime * 60;
					if (Collision(objBullet2.get<Transform>(), objDunceArena.get<Transform>())) {
						shoot2 = false;
						ammo2 = false;
						score2 += 1;
						highscore2 += 100 + timesub2;
						timesub2 = 0.0f;
					}
				}

				if (score2 >= 1)
				{
					scorecounter[3].get<RendererComponent>().SetMesh(Fullscore);
				}
				else
				{
					scorecounter[3].get<RendererComponent>().SetMesh(Emptyscore);

				}
				if (score2 >= 2)
				{
					scorecounter[4].get<RendererComponent>().SetMesh(Fullscore);
				}
				else
				{
					scorecounter[4].get<RendererComponent>().SetMesh(Emptyscore);

				}
				if (score2 >= 3)
				{
					scorecounter[5].get<RendererComponent>().SetMesh(Fullscore);
					p2win = true;
				}
				else
				{
					scorecounter[5].get<RendererComponent>().SetMesh(Emptyscore);

				}
				if (p2win)
				{
					player2w.get<Transform>().SetLocalPosition(0.0f, 0.0f, 9.0f);
					if (glfwGetKey(BackendHandler::window, GLFW_KEY_SPACE) == GLFW_PRESS)
					{
						highscores.open("highscores.txt", std::ios::app);
						highscores << highscore2 << "\n";
						highscores.close();
						highscore1 = 0.0f;
						highscore2 = 0.0f;
						Application::Instance().ActiveScene = Menu;
					}
				}

				#pragma endregion Shooting

				#pragma region Player 1 and 2 Collision
				//Hit detection test
				for (int i = 0; i < NUM_HITBOXES; i++) {
					if (i == 12 || i == 13 || i == 14 || i == 15 || i == 20) {
						if (Collision(objDuncetArena.get<Transform>(), HitboxesArena[i].get<Transform>())) {
							//yes
						}
					}
					else {
						if (Collision(objDuncetArena.get<Transform>(), HitboxesArena[i].get<Transform>())) {
							PlayerMovement::Player2vswall(objDuncetArena.get<Transform>(), time.DeltaTime);
						}
					}
				}
				
				for (int i = 0; i < NUM_HITBOXES; i++) {
					if (i == 12 || i == 13 || i == 14 || i == 15 || i == 20) {
						if (Collision(objDunceArena.get<Transform>(), HitboxesArena[i].get<Transform>())) {
							//yes
						}
					}
					else {
						if (Collision(objDunceArena.get<Transform>(), HitboxesArena[i].get<Transform>())) {
							PlayerMovement::Player1vswall(objDunceArena.get<Transform>(), time.DeltaTime);
							//objDunceArena.get<Transform>().SetLocalPosition(projection1(objDunceArena.get<Transform>(), HitboxesArena[i].get<Transform>()));
						}
					}
				}

				if (renderammoground1) {
					if (Collision(objDunceArena.get<Transform>(), HitboxesArena[12].get<Transform>()) && ammo == false) {
						renderammoground1 = false;
						ammo = true;
						renderammo = true;
						soundbottle1 = true;
					}
				}
				else
				{
					soundbottle1 = false;
				}
				if (renderammoground2) {
					if (Collision(objDunceArena.get<Transform>(), HitboxesArena[13].get<Transform>()) && ammo == false) {
						renderammoground2 = false;
						ammo = true;
						renderammo = true;
						soundbottle2 = true;
					}
				}
				else
				{
					soundbottle2 = false;
				}
				if (renderammoground3) {
					if (Collision(objDunceArena.get<Transform>(), HitboxesArena[14].get<Transform>()) && ammo == false) {
						renderammoground3 = false;
						ammo = true;
						renderammo = true;
						soundbottle3 = true;
					}
				}
				else
				{
					soundbottle3 = false;
				}
				if (renderammoground4) {
					if (Collision(objDunceArena.get<Transform>(), HitboxesArena[15].get<Transform>()) && ammo == false) {
						renderammoground4 = false;
						ammo = true;
						renderammo = true;
						soundbottle4 = true;
					}
				}
				else
				{
					soundbottle4 = false;
				}
				if (renderammoground5) {
					if (Collision(objDunceArena.get<Transform>(), HitboxesArena[20].get<Transform>()) && ammo == false) {
						renderammoground5 = false;
						ammo = true;
						renderammo = true;
						soundbottle5 = true;
					}
				}
				else
				{
					soundbottle5 = false;
				}
				
				if (renderammoground1) {
					if (Collision(objDuncetArena.get<Transform>(), HitboxesArena[12].get<Transform>()) && ammo2 == false) {
						renderammoground1 = false;
						ammo2 = true;
						renderammo2 = true;
						soundbottle6 = true;
					}
				}
				else
				{
					soundbottle6 = false;
				}
				if (renderammoground2) {
					if (Collision(objDuncetArena.get<Transform>(), HitboxesArena[13].get<Transform>()) && ammo2 == false) {
						renderammoground2 = false;
						ammo2 = true;
						renderammo2 = true;
						soundbottle7 = true;
					}
				}
				else
				{
					soundbottle7 = false;
				}
				if (renderammoground3) {
					if (Collision(objDuncetArena.get<Transform>(), HitboxesArena[14].get<Transform>()) && ammo2 == false) {
						renderammoground3 = false;
						ammo2 = true;
						renderammo2 = true;
						soundbottle8 = true;
					}
				}
				else
				{
					soundbottle8 = false;
				}
				if (renderammoground4) {
					if (Collision(objDuncetArena.get<Transform>(), HitboxesArena[15].get<Transform>()) && ammo2 == false) {
						renderammoground4 = false;
						ammo2 = true;
						renderammo2 = true;
						soundbottle9 = true;
					}
				}
				else
				{
					soundbottle9 = false;
				}
				if (renderammoground5) {
					if (Collision(objDuncetArena.get<Transform>(), HitboxesArena[20].get<Transform>()) && ammo2 == false) {
						renderammoground5 = false;
						ammo2 = true;
						renderammo2 = true;
						soundbottle10 = true;
					}
				}
				else
				{
					soundbottle10 = false;
				}

				if (Collision(objDuncetArena.get<Transform>(), objDunceArena.get<Transform>())) {
					PlayerMovement::Player2vswall(objDuncetArena.get<Transform>(), time.DeltaTime);
				}
				if (Collision(objDuncetArena.get<Transform>(), objDunceArena.get<Transform>())) {
					PlayerMovement::Player1vswall(objDunceArena.get<Transform>(), time.DeltaTime);
				}

				#pragma endregion Player 1 and 2 Collision
				
				if (!renderammoground1)
				{
					Bottles[0].get<RendererComponent>().SetMesh(EmptyBottle);
					Bottles[0].get<Transform>().SetLocalPosition(0.0f, 0.0f, 6.0f);
				}
				else
				{
					Bottles[0].get<RendererComponent>().SetMesh(FullBottle);
					Bottles[0].get<Transform>().SetLocalPosition(0.0f, 0.0f, 6.0f);
				}
				if (!renderammoground2)
				{
					Bottles[1].get<RendererComponent>().SetMesh(EmptyBottle);
					Bottles[1].get<Transform>().SetLocalPosition(-8.0f, -2.5f, 6.0f);
				}
				else
				{
					Bottles[1].get<RendererComponent>().SetMesh(FullBottle);
					Bottles[1].get<Transform>().SetLocalPosition(-8.0f, -2.5f, 6.0f);
				}
				if (!renderammoground3)
				{
					Bottles[2].get<RendererComponent>().SetMesh(EmptyBottle);
					Bottles[2].get<Transform>().SetLocalPosition(10.0f, -5.0f, 6.0f);
				}
				else
				{
					Bottles[2].get<RendererComponent>().SetMesh(FullBottle);
					Bottles[2].get<Transform>().SetLocalPosition(10.0f, -5.0f, 6.0f);
				}
				if (!renderammoground4)
				{
					Bottles[3].get<RendererComponent>().SetMesh(EmptyBottle);
					Bottles[3].get<Transform>().SetLocalPosition(11.5f, 3.6f, 6.0f);
				}
				else
				{
					Bottles[3].get<RendererComponent>().SetMesh(FullBottle);
					Bottles[3].get<Transform>().SetLocalPosition(11.5f, 3.6f, 6.0f);
				}
				if (!renderammoground5)
				{
					Bottles[4].get<RendererComponent>().SetMesh(EmptyBottle);
					Bottles[4].get<Transform>().SetLocalPosition(-13.6f, 5.5f, 6.0f);
				}
				else
				{
					Bottles[4].get<RendererComponent>().SetMesh(FullBottle);
					Bottles[4].get<Transform>().SetLocalPosition(-13.0f, 5.5f, 6.0f);
				}
				if (!ammo)
				{
					Bottles[5].get<RendererComponent>().SetMesh(EmptyBottle);
					Bottles[5].get<Transform>().SetLocalPosition(4.0f, 7.5f, 9.0f);
					AnimBottle = 0.0f;
					animatebottle = false;
				}
				else
				{
					Bottles[5].get<RendererComponent>().SetMesh(FullBottle);
					Bottles[5].get<Transform>().SetLocalPosition(4.0f, 7.5f, 9.0f);
					AnimBottle += time.DeltaTime;

					if (animatebottle) {
						if (AnimBottle < 0.2f) {
							Bottles[5].get<RendererComponent>().SetMesh(Bottle1);
						}
						if (AnimBottle >= 0.2f && AnimBottle < 0.3f) {
							Bottles[5].get<RendererComponent>().SetMesh(Bottle2);
						}
						if (AnimBottle >= 0.3f && AnimBottle < 0.4f) {
							Bottles[5].get<RendererComponent>().SetMesh(Bottle3);
						}
						if (AnimBottle >= 0.4f && AnimBottle < 0.5f) {
							Bottles[5].get<RendererComponent>().SetMesh(Bottle4);
						}
						if (AnimBottle >= 0.5f && AnimBottle < 0.6f) {
							Bottles[5].get<RendererComponent>().SetMesh(Bottle5);
						}
						if (AnimBottle >= 0.6f && AnimBottle < 0.7f) {
							Bottles[5].get<RendererComponent>().SetMesh(Bottle6);
						}
						if (AnimBottle >= 0.7f && AnimBottle < 0.8f) {
							Bottles[5].get<RendererComponent>().SetMesh(Bottle7);
						}
						if (AnimBottle >= 0.8f && AnimBottle < 0.9f) {
							Bottles[5].get<RendererComponent>().SetMesh(Bottle8);
						}
						if (AnimBottle >= 0.9f && AnimBottle < 1.0f) {
							Bottles[5].get<RendererComponent>().SetMesh(Bottle9);
						}
						if (AnimBottle >= 1.0f && AnimBottle < 1.2f) {
							Bottles[5].get<RendererComponent>().SetMesh(Bottle10);
						}
						if (AnimBottle >= 1.2f && AnimBottle < 1.4f) {
							Bottles[5].get<RendererComponent>().SetMesh(Bottle11);
						}
						if (AnimBottle >= 1.4f)
						{
							AnimBottle = 0.0f;
							animatebottle = false;
						}
					}
				}	
				
				if (!ammo2)
				{
					Bottles[6].get<RendererComponent>().SetMesh(EmptyBottle);
					Bottles[6].get<Transform>().SetLocalPosition(-4.0f, 7.5f, 9.0f);
					AnimBottle2 = 0.0f;
					animatebottle2 = false;
				}
				else
				{
					Bottles[6].get<RendererComponent>().SetMesh(FullBottle);
					Bottles[6].get<Transform>().SetLocalPosition(-4.0f, 7.5f, 9.0f);
					AnimBottle2 += time.DeltaTime;

					if (animatebottle2) {
						if (AnimBottle2 < 0.2f) {
							Bottles[6].get<RendererComponent>().SetMesh(Bottle1);
						}
						if (AnimBottle2 >= 0.2f && AnimBottle2 < 0.3f) {
							Bottles[6].get<RendererComponent>().SetMesh(Bottle2);
						}
						if (AnimBottle2 >= 0.3f && AnimBottle2 < 0.4f) {
							Bottles[6].get<RendererComponent>().SetMesh(Bottle3);
						}
						if (AnimBottle2 >= 0.4f && AnimBottle2 < 0.5f) {
							Bottles[6].get<RendererComponent>().SetMesh(Bottle4);
						}
						if (AnimBottle2 >= 0.5f && AnimBottle2 < 0.6f) {
							Bottles[6].get<RendererComponent>().SetMesh(Bottle5);
						}
						if (AnimBottle2 >= 0.6f && AnimBottle2 < 0.7f) {
							Bottles[6].get<RendererComponent>().SetMesh(Bottle6);
						}
						if (AnimBottle2 >= 0.7f && AnimBottle2 < 0.8f) {
							Bottles[6].get<RendererComponent>().SetMesh(Bottle7);
						}
						if (AnimBottle2 >= 0.8f && AnimBottle2 < 0.9f) {
							Bottles[6].get<RendererComponent>().SetMesh(Bottle8);
						}
						if (AnimBottle2 >= 0.9f && AnimBottle2 < 1.0f) {
							Bottles[6].get<RendererComponent>().SetMesh(Bottle9);
						}
						if (AnimBottle2 >= 1.0f && AnimBottle2 < 1.2f) {
							Bottles[6].get<RendererComponent>().SetMesh(Bottle10);
						}
						if (AnimBottle2 >= 1.2f && AnimBottle2 < 1.4f) {
							Bottles[6].get<RendererComponent>().SetMesh(Bottle11);
						}
						if (AnimBottle2 >= 1.4f)
						{
							AnimBottle2 = 0.0f;
							animatebottle2 = false;
						}
					}
				}

				#pragma endregion BIG MESSY SPAGHETTI CODE AGAIN

				// Iterate over all the behaviour binding components
				Arena1->Registry().view<BehaviourBinding>().each([&](entt::entity entity, BehaviourBinding& binding) {
					// Iterate over all the behaviour scripts attached to the entity, and update them in sequence (if enabled)
					for (const auto& behaviour : binding.Behaviours) {
						if (behaviour->Enabled) {
							behaviour->Update(entt::handle(Arena1->Registry(), entity));
						}
					}
				});

				Arena1->Registry().view<Transform>().each([](entt::entity entity, Transform& t) {
					t.UpdateWorldMatrix();
				});

				renderGroupArena.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
					// Sort by render layer first, higher numbers get drawn last
					if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
					if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

					// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
					if (l.Material->Shader < r.Material->Shader) return true;
					if (l.Material->Shader > r.Material->Shader) return false;

					// Sort by material pointer last (so we can minimize switching between materials)
					if (l.Material < r.Material) return true;
					if (l.Material > r.Material) return false;

					return false;
				});

				glViewport(0, 0, shadowWidth, shadowHeight);
				shadowBuffer->Bind();

				renderGroupArena.each([&](entt::entity e, RendererComponent& renderer, Transform& transform) {
					// Render the mesh
					if (renderer.CastShadows)
					{
						BackendHandler::RenderVAO(simpleDepthShader, renderer.Mesh, viewProjection, transform, lightSpaceViewProj);
					}
					});

				shadowBuffer->Unbind();


				glfwGetWindowSize(BackendHandler::window, &width, &height);

				glViewport(0, 0, width, height);

				basicEffect->BindBuffer(0);

				renderGroupArena.each([&](entt::entity e, RendererComponent& renderer, Transform& transform) {
					// If the shader has changed, set up it's uniforms
					if (current != renderer.Material->Shader) {
						current = renderer.Material->Shader;
						current->Bind();
						BackendHandler::SetupShaderForFrame(current, view, projection);
					}
					// If the material has changed, apply it
					if (currentMat != renderer.Material) {
						currentMat = renderer.Material;
						currentMat->Apply();
					}

					shadowBuffer->BindDepthAsTexture(30);
					// Render the mesh
						BackendHandler::RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform, lightSpaceViewProj);
				});
				shadowBuffer->UnbindTexture(30);
				basicEffect->UnbindBuffer();

				effects[activeEffect]->ApplyEffect(basicEffect);

				effects[activeEffect]->DrawToScreen();
			}
			#pragma endregion Arena 1 scene stuff

			#pragma region Pause
			if (Application::Instance().ActiveScene == Pause) {

				camTransform = cameraObject.get<Transform>().SetLocalPosition(0, 0, 23.5).SetLocalRotation(0, 0, 180);
				view = glm::inverse(camTransform.LocalTransform());
				projection = cameraObject.get<Camera>().GetProjection();
				viewProjection = projection * view;

				if (glfwGetKey(BackendHandler::window, GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS)
				{
					Application::Instance().ActiveScene = scene;//just to test change to arena1 later
				}

				int controller1 = glfwJoystickPresent(GLFW_JOYSTICK_1);
				int controller2 = glfwJoystickPresent(GLFW_JOYSTICK_2);

				//player 1 controller
				if (controller1 == 1)
				{
					int axesCount;
					int buttonCount;
					const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axesCount);
					const unsigned char* button = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttonCount);

					if (axes[0] >= -1 && axes[0] <= -0.2) {
						materialPause->Set("u_TextureMix", 1.0f);
					}
					if (axes[0] <= 1 && axes[0] >= 0.2) {
						materialPause->Set("u_TextureMix", 0.0f);
					}

					if (button[1] == GLFW_PRESS)
					{
						Application::Instance().ActiveScene = Arena1;
					}

				}
				
				//player 2 controller
				if (controller2 == 1)
				{
					int axesCount2;
					int buttonCount2;
					const float* axes2 = glfwGetJoystickAxes(GLFW_JOYSTICK_2, &axesCount2);
					const unsigned char* button2 = glfwGetJoystickButtons(GLFW_JOYSTICK_2, &buttonCount2);

					if (axes2[0] >= -1 && axes2[0] <= -0.2) {
						materialPause->Set("u_TextureMix", 1.0f);
					}
					if (axes2[0] <= 1 && axes2[0] >= 0.2) {
						materialPause->Set("u_TextureMix", 0.0f);
					}

					if (button2[1] == GLFW_PRESS)
					{
						Application::Instance().ActiveScene = Arena1;
					}

				}

				if (glfwGetKey(BackendHandler::window, GLFW_KEY_ENTER) == GLFW_PRESS)
				{
					Application::Instance().ActiveScene = Arena1;
				}
				
				if (glfwGetKey(BackendHandler::window, GLFW_KEY_D) == GLFW_PRESS)
				{
					materialPause->Set("u_TextureMix", 0.0f);
				}

				if (glfwGetKey(BackendHandler::window, GLFW_KEY_A) == GLFW_PRESS)
				{
					materialPause->Set("u_TextureMix", 1.0f);
				}

				// Iterate over all the behaviour binding components
				Pause->Registry().view<BehaviourBinding>().each([&](entt::entity entity, BehaviourBinding& binding) {
					// Iterate over all the behaviour scripts attached to the entity, and update them in sequence (if enabled)
					for (const auto& behaviour : binding.Behaviours) {
						if (behaviour->Enabled) {
							behaviour->Update(entt::handle(Pause->Registry(), entity));
						}
					}
					});

				// Update all world matrices for this frame
				Pause->Registry().view<Transform>().each([](entt::entity entity, Transform& t) {
					t.UpdateWorldMatrix();
					});

				// Sort the renderers by shader and material, we will go for a minimizing context switches approach here,
			// but you could for instance sort front to back to optimize for fill rate if you have intensive fragment shaders
				renderGroupPause.sort<RendererComponent>([](const RendererComponent& l, const RendererComponent& r) {
					// Sort by render layer first, higher numbers get drawn last
					if (l.Material->RenderLayer < r.Material->RenderLayer) return true;
					if (l.Material->RenderLayer > r.Material->RenderLayer) return false;

					// Sort by shader pointer next (so materials using the same shader run sequentially where possible)
					if (l.Material->Shader < r.Material->Shader) return true;
					if (l.Material->Shader > r.Material->Shader) return false;

					// Sort by material pointer last (so we can minimize switching between materials)
					if (l.Material < r.Material) return true;
					if (l.Material > r.Material) return false;

					return false;
					});

				basicEffect->BindBuffer(0);

				// Iterate over the render group components and draw them
				renderGroupPause.each([&](entt::entity e, RendererComponent& renderer, Transform& transform) {
					// If the shader has changed, set up it's uniforms
					if (current != renderer.Material->Shader) {
						current = renderer.Material->Shader;
						current->Bind();
						BackendHandler::SetupShaderForFrame(current, view, projection);
					}
					// If the material has changed, apply it
					if (currentMat != renderer.Material) {
						currentMat = renderer.Material;
						currentMat->Apply();
					}
					// Render the mesh
					BackendHandler::RenderVAO(renderer.Material->Shader, renderer.Mesh, viewProjection, transform);
					});
				basicEffect->UnbindBuffer();

				effects[activeEffect]->ApplyEffect(basicEffect);

				effects[activeEffect]->DrawToScreen();

				BackendHandler::RenderImGui();
			}
			#pragma endregion Pause

			#pragma endregion Rendering seperate scenes
			
			scene->Poll();
			glfwSwapBuffers(BackendHandler::window);
			time.LastFrame = time.CurrentFrame;
		}

		directionalLightBuffer.Unbind(0);

		Shutdown();

		// Nullify scene so that we can release references
		Application::Instance().ActiveScene = nullptr;

		for (int i = 0; i < Application::Instance().scenes.size(); i++)
		{
			Application::Instance().scenes[i] = nullptr;
		}

		EnvironmentGenerator::CleanUpPointers();
		BackendHandler::ShutdownImGui();
	}	

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}