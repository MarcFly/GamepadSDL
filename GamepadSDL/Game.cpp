#include "Game.h"
#include <math.h>

Game::Game() {}
Game::~Game(){}

bool Game::Init()
{
	bool change_for_merge = 1;

	bool change_for_merge_2 = 2;
	
	int legs = 2;

	//Initialize SDL with all subsystems
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
		return false;
	}
	//Create our window: title, x, y, w, h, flags
	Window = SDL_CreateWindow("Spaceship: arrow keys + space, f1: god mode", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
	if (Window == NULL)
	{
		SDL_Log("Unable to create window: %s", SDL_GetError());
		return false;
	}
	//Create a 2D rendering context for a window: window, device index, flags
	Renderer = SDL_CreateRenderer(Window, -1, 0);
	if (Renderer == NULL)
	{
		SDL_Log("Unable to create rendering context: %s", SDL_GetError());
		return false;
	}
	//Initialize keys array
	for (int i = 0; i < MAX_KEYS; ++i)
		keys[i] = KEY_IDLE;

	//Load images
	if (!LoadImages())
		return false;

	// Init SDL_Mixer
	int flags = MIX_INIT_OGG;
	if (Mix_Init(flags) != flags) {
		SDL_Log("Failed to init OGG module for SDL_Mixer!\n");
		SDL_Log("Mix_Init: %s\n", Mix_GetError());
		return false;
	}
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) == -1) {
		SDL_Log("Failed to init SDL_Mixer!\n");
		SDL_Log("Mix_OpenAudio: %s\n", Mix_GetError());
		return false;
	}
	
	if (!LoadAudios())
		return false;

	// Initialize Controller
	num_controllers = SDL_NumJoysticks();
	for (int i = 0; i < num_controllers; ++i)
		if (SDL_IsGameController(i)) 
			sdl_controllers[i] = SDL_GameControllerOpen(i);
	// Here we don't really have access to the controller
	// SDL_GameController* is obfuscated, it is to know they are there.
	// We will pass data to our own array of controllers
	

	//Init variables
	//size: 104x82
	Player.Init(20, WINDOW_HEIGHT >> 1, 104, 82, 5);
	idx_shot = 0;

	int w;
	SDL_QueryTexture(img_background, NULL, NULL, &w, NULL);
	Scene.Init(0, 0, w, WINDOW_HEIGHT, 4);
	god_mode = false;

	return true;
}

bool Game::LoadAudios() {
	num_tracks = 0;
	tracks[num_tracks++] = Mix_LoadMUS("sample_ogg.ogg");

	Mix_PlayMusic(tracks[0], -1);

	sfxs[num_sfxs++] = Mix_LoadWAV("sample_wav.wav");

	return true;
}

bool Game::LoadImages()
{
	if(IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG)
	{
		SDL_Log("IMG_Init, failed to init required png support: %s\n", IMG_GetError());
		return false;
	}
	img_background = SDL_CreateTextureFromSurface(Renderer, IMG_Load("background.png"));
	if (img_background == NULL) {
		SDL_Log("CreateTextureFromSurface failed: %s\n", SDL_GetError());
		return false;
	}
	img_player = SDL_CreateTextureFromSurface(Renderer, IMG_Load("spaceship.png"));
	if (img_player == NULL) {
		SDL_Log("CreateTextureFromSurface failed: %s\n", SDL_GetError());
		return false;
	}
	img_shot = SDL_CreateTextureFromSurface(Renderer, IMG_Load("shot.png"));
	if (img_shot == NULL) {
		SDL_Log("CreateTextureFromSurface failed: %s\n", SDL_GetError());
		return false;
	}
	return true;
}
void Game::Release()
{
	//Release images
	SDL_DestroyTexture(img_background);
	SDL_DestroyTexture(img_player);
	SDL_DestroyTexture(img_shot);
	IMG_Quit();
	
	// Free Audios
	for (int i = 0; i < num_tracks; ++i)
		Mix_FreeMusic(tracks[i]);
	for (int i = 0; i < num_sfxs; ++i)
		Mix_FreeChunk(sfxs[i]);

	// Close SDL_Mixer
	Mix_CloseAudio();
	while(Mix_Init(0))
		Mix_Quit();
		
	//Clean up all SDL initialized subsystems
	SDL_Quit();
}
bool Game::Input()
{
	SDL_Event event;
	if (SDL_PollEvent(&event))
	{
		if (event.type == SDL_QUIT)	return false;
	}

	SDL_PumpEvents();
	const Uint8* keyboard = SDL_GetKeyboardState(NULL);
	for (int i = 0; i < MAX_KEYS; ++i)
	{
		if (keyboard[i])
			keys[i] = (keys[i] == KEY_IDLE) ? KEY_DOWN : KEY_REPEAT;
		else
			keys[i] = (keys[i] == KEY_REPEAT || keys[i] == KEY_DOWN) ? KEY_UP : KEY_IDLE;
	}

	// Parse Controller button stats
	SDL_GameControllerUpdate();
	for (int i = 0; i < num_controllers; ++i)
	{
		for (int j = 0; j < SDL_CONTROLLER_BUTTON_MAX; ++j)
		{
			if (SDL_GameControllerGetButton(sdl_controllers[i], (SDL_GameControllerButton)j))
				controllers[i].buttons[j] = (controllers[i].buttons[j] == KEY_IDLE) ? KEY_DOWN : KEY_REPEAT;
			else
				controllers[i].buttons[j] = (controllers[i].buttons[j] == KEY_REPEAT || controllers[i].buttons[j] == KEY_DOWN) ? KEY_UP : KEY_IDLE;
		}

		controllers[i].j1_x = SDL_GameControllerGetAxis(sdl_controllers[i], SDL_CONTROLLER_AXIS_LEFTX);
		controllers[i].j1_y = SDL_GameControllerGetAxis(sdl_controllers[i], SDL_CONTROLLER_AXIS_LEFTY);
		controllers[i].j2_x = SDL_GameControllerGetAxis(sdl_controllers[i], SDL_CONTROLLER_AXIS_RIGHTX);
		controllers[i].j2_y = SDL_GameControllerGetAxis(sdl_controllers[i], SDL_CONTROLLER_AXIS_RIGHTY);
		controllers[i].RT = SDL_GameControllerGetAxis(sdl_controllers[i], SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
		controllers[i].LT = SDL_GameControllerGetAxis(sdl_controllers[i], SDL_CONTROLLER_AXIS_TRIGGERLEFT);
	}

	return true;
}

// GAMEPAD: Helper function to get a proper range despite deadzone
//
// Min Is used as the Deadzone, area where the controller reports movement althought it might not be being used
// It then substract to set to 0 and then sets the range from {-1,1}
// Div by MAX_Sigend integer which is 32767, because SDL uses internally also a max of 32767 to represent axis value
// Then multiply by the expected range, in this case we want 1 so leave clamp_to=1
float reduce_val(float v1, float min, float clamp_to) {
	float sign = v1 / fabs(v1);
	float reduced = v1 - ((fabs(v1) > min) ? sign * min : v1);
	float to_1 = reduced / (float)(SDL_MAX_SINT16);
	float reclamped = to_1 * clamp_to;
	return reclamped;
}

bool Game::Update()
{
	//Read Input
	if (!Input())	return true;

	//Process Input
	float fx = 0, fy = 0;

	if (keys[SDL_SCANCODE_ESCAPE] == KEY_DOWN)	return true;
	if (keys[SDL_SCANCODE_F1] == KEY_DOWN)		god_mode = !god_mode;
	if (keys[SDL_SCANCODE_UP] == KEY_REPEAT)	fy = -1;
	if (keys[SDL_SCANCODE_DOWN] == KEY_REPEAT)	fy = 1;
	if (keys[SDL_SCANCODE_LEFT] == KEY_REPEAT)	fx = -1;
	if (keys[SDL_SCANCODE_RIGHT] == KEY_REPEAT)	fx = 1;


	// GAMEPAD: Avoid Deadzones and use the values in you own way
	fx += reduce_val(controllers[0].j1_x, 3000, 2);
	fy += reduce_val(controllers[0].j1_y, 3000, 2);
	fx += reduce_val(controllers[0].j2_x, 3000, 2);
	fy += reduce_val(controllers[0].j2_y, 3000, 2);

	// GAMEPAD: Triggers Count as axis, have specific values
	if (controllers[0].LT > SDL_MAX_SINT16 / 2) {
		fx *= 2;
		fy *= 2;
	}
	if (controllers[0].RT > SDL_MAX_SINT16 / 2) {
		fx *= 3;
		fy *= 3;
	}

	// GAMEPAD: Fire with any button for now to check they all work
	bool button_press = false;
	for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i)
		if (controllers[0].buttons[i] == KEY_DOWN)
		{
			button_press = true; break;
		}

	if (keys[SDL_SCANCODE_SPACE] == KEY_DOWN || button_press)
	{
		int x, y, w, h;
		Player.GetRect(&x, &y, &w, &h);
		//size: 56x20
		//offset from player: dx, dy = [(29, 3), (29, 59)]
		Shots[idx_shot].Init(x + 29, y + 3, 56, 20, 10);
		idx_shot++;
		idx_shot %= MAX_SHOTS;
		Shots[idx_shot].Init(x + 29, y + 59, 56, 20, 10);
		idx_shot++;
		idx_shot %= MAX_SHOTS;

		// Play a single Sound
		Mix_PlayChannel(-1, sfxs[0], 0);
	}

	//Logic
	//Scene scroll
	Scene.Move(-1, 0);
	if (Scene.GetX() <= -Scene.GetWidth())	Scene.SetX(0);
	//Player update
	if (Player.GetX() > 0 && Player.GetX() < WINDOW_WIDTH && Player.GetY() > 0 && Player.GetY() < WINDOW_HEIGHT)
		Player.Move(fx, fy);
	else
		Player.Init(20, WINDOW_HEIGHT >> 1, 104, 82, 5);

	//Shots update
	for (int i = 0; i < MAX_SHOTS; ++i)
	{
		if (Shots[i].IsAlive())
		{
			Shots[i].Move(1, 0);
			if (Shots[i].GetX() > WINDOW_WIDTH)	Shots[i].ShutDown();
		}
	}
		
	return false;
}
void Game::Draw()
{
	SDL_Rect rc;

	//Set the color used for drawing operations
	SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);
	//Clear rendering target
	SDL_RenderClear(Renderer);

	//God mode uses red wireframe rectangles for physical objects
	if (god_mode) SDL_SetRenderDrawColor(Renderer, 192, 0, 0, 255);

	//Draw scene
	Scene.GetRect(&rc.x, &rc.y, &rc.w, &rc.h);
	SDL_RenderCopy(Renderer, img_background, NULL, &rc);
	rc.x += rc.w;
	SDL_RenderCopy(Renderer, img_background, NULL, &rc);
	
	//Draw player
	Player.GetRect(&rc.x, &rc.y, &rc.w, &rc.h);
	SDL_RenderCopy(Renderer, img_player, NULL, &rc);
	if (god_mode) SDL_RenderDrawRect(Renderer, &rc);
	
	//Draw shots
	for (int i = 0; i < MAX_SHOTS; ++i)
	{
		if (Shots[i].IsAlive())
		{
			Shots[i].GetRect(&rc.x, &rc.y, &rc.w, &rc.h);
			SDL_RenderCopy(Renderer, img_shot, NULL, &rc);
			if (god_mode) SDL_RenderDrawRect(Renderer, &rc);
		}
	}

	//Update screen
	SDL_RenderPresent(Renderer);

	SDL_Delay(10);	// 1000/10 = 100 fps max
}