#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <emscripten.h>
#include <vector>
#include <random>
#include <iostream>

Uint32 lastTime = 0;

constexpr int WIDTH = 1280;
constexpr int HEIGHT = 720;

constexpr float GRAVITY = 0.005f;
constexpr float BIRD_JUMP = -0.8f;
constexpr float PIPE_SPEED = 0.2f;
constexpr float TERMINAL_VELOCITY = 0.5f;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

TTF_Font* font = nullptr;
SDL_Color textColor = { 255, 255, 255 };

std::mt19937 gen(std::random_device{}());
std::uniform_int_distribution<int> dist(0, HEIGHT - 180);

enum class GameState {
    START,
    RUNNING,
    GAMEOVER
};

class Bird {
public:
    SDL_Rect rect{ WIDTH / 4, HEIGHT / 2, 20, 20 };
private:
    float velocity = 0.0f;
public:
    inline void Update(float deltaTime) {
        velocity = std::min(velocity + GRAVITY * deltaTime, TERMINAL_VELOCITY);
        rect.y += static_cast<int>(velocity * deltaTime);
    }

    inline void Jump() {
        velocity = BIRD_JUMP;
    }

    inline void Draw() {
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        SDL_RenderFillRect(renderer, &rect);
    }
};

class Pipe {
public:
    bool hasPassed = false;
    int gapY = dist(gen);
    int x = WIDTH;
    float xAccumulator = static_cast<float>(WIDTH);  // NEW: x position accumulator

    inline void Update(float deltaTime) {
        xAccumulator -= PIPE_SPEED * deltaTime;
        x = static_cast<int>(xAccumulator);  // Set the integer x position based on accumulator
    }

    inline void Draw() const {
        SDL_Rect upperPipe{ x, 0, 60, gapY };
        SDL_Rect lowerPipe{ x, gapY + 180, 60, HEIGHT - gapY - 180 };
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderFillRect(renderer, &upperPipe);
        SDL_RenderFillRect(renderer, &lowerPipe);
    }
};

class Game {
public:
    GameState gameState = GameState::START;
private:
    Bird bird;
    std::vector<Pipe> pipes;
    int score = 0;

    inline void StartGame() {
        gameState = GameState::RUNNING;
        score = 0;
    }

    inline void EndGame() {
        if (smackSound) Mix_PlayChannel(-1, smackSound, 0);
        gameState = GameState::GAMEOVER;
    }

    inline void ResetGame() {
        Reset();
        gameState = GameState::START;
    }

    inline void Reset() {
        bird = Bird();
        pipes.clear();
    }

    inline void OnPipePassed() {
        if (successSound) Mix_PlayChannel(-1, successSound, 0);
        score++;
    }
public:
    Mix_Chunk* successSound = nullptr;
    Mix_Chunk* smackSound = nullptr;

    inline void HandleInput() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                emscripten_cancel_main_loop();
            } else if (gameState == GameState::RUNNING && 
                       (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE)) {
                bird.Jump();
            } else if (gameState == GameState::RUNNING && event.type == SDL_FINGERDOWN) {
                bird.Jump();
            } else if (gameState == GameState::START && (event.type == SDL_KEYDOWN || event.type == SDL_FINGERDOWN)) {
                StartGame();
            } else if (gameState == GameState::GAMEOVER && (event.type == SDL_KEYDOWN || event.type == SDL_FINGERDOWN)) {
                ResetGame();
            }
        }
    }

    inline void Update(float deltaTime) {
        bird.Update(deltaTime);

        if (pipes.empty() || WIDTH - pipes.back().x >= 400) pipes.emplace_back();

        for (auto& pipe : pipes) pipe.Update(deltaTime);

        pipes.erase(std::remove_if(pipes.begin(), pipes.end(), [](const Pipe& pipe) {
            return pipe.x + 60 < 0;
        }), pipes.end());

        for (auto& pipe : pipes) {
            if (bird.rect.x < pipe.x + 60 && bird.rect.x + bird.rect.w > pipe.x && 
                (bird.rect.y < pipe.gapY || bird.rect.y + bird.rect.h > pipe.gapY + 180)) {
                // Collision detected
                EndGame();
                return;
            } else if (bird.rect.x > pipe.x + 60 && !pipe.hasPassed) {
                // Bird has passed through the pipe successfully
                pipe.hasPassed = true;
                OnPipePassed();
            }
        }

        if (bird.rect.y < 0 || bird.rect.y + bird.rect.h > HEIGHT) {
            EndGame();
        }
    }

    inline void Render() {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        bird.Draw();
        for (const auto& pipe : pipes) {
            pipe.Draw();
        }
        
        // Format the score string
        char formattedScore[50]; // Assuming the score string will never exceed this length.
        snprintf(formattedScore, sizeof(formattedScore), "SCORE   %04d", score);

        // Render the score
        SDL_Surface* scoreSurface = TTF_RenderText_Solid(font, formattedScore, textColor);
        SDL_Texture* scoreTexture = SDL_CreateTextureFromSurface(renderer, scoreSurface);
        SDL_Rect scoreRect = { 10, HEIGHT - scoreSurface->h - 10, scoreSurface->w, scoreSurface->h }; 
        SDL_RenderCopy(renderer, scoreTexture, nullptr, &scoreRect);

        SDL_DestroyTexture(scoreTexture);
        SDL_FreeSurface(scoreSurface);

        // Add messages based on game state
        if (gameState == GameState::START) {
            SDL_Surface* startSurface = TTF_RenderText_Solid(font, "Tap or Press Any Key to Start", textColor);
            SDL_Texture* startTexture = SDL_CreateTextureFromSurface(renderer, startSurface);
            SDL_Rect startRect = { WIDTH / 2 - startSurface->w / 2, HEIGHT / 2 - startSurface->h / 2, startSurface->w, startSurface->h };
            SDL_RenderCopy(renderer, startTexture, nullptr, &startRect);
            SDL_DestroyTexture(startTexture);
            SDL_FreeSurface(startSurface);
        } else if (gameState == GameState::GAMEOVER) {
            SDL_Surface* gameOverSurface = TTF_RenderText_Solid(font, "Game Over!", textColor);
            SDL_Texture* gameOverTexture = SDL_CreateTextureFromSurface(renderer, gameOverSurface);
            SDL_Rect gameOverRect = { WIDTH / 2 - gameOverSurface->w / 2, HEIGHT / 4, gameOverSurface->w, gameOverSurface->h };
            SDL_RenderCopy(renderer, gameOverTexture, nullptr, &gameOverRect);
            SDL_DestroyTexture(gameOverTexture);
            SDL_FreeSurface(gameOverSurface);

            SDL_Surface* restartSurface = TTF_RenderText_Solid(font, "Press any key to restart", textColor);
            SDL_Texture* restartTexture = SDL_CreateTextureFromSurface(renderer, restartSurface);
            SDL_Rect restartRect = { WIDTH / 2 - restartSurface->w / 2, HEIGHT / 2 - restartSurface->h / 2, restartSurface->w, restartSurface->h };
            SDL_RenderCopy(renderer, restartTexture, nullptr, &restartRect);
            SDL_DestroyTexture(restartTexture);
            SDL_FreeSurface(restartSurface);
        }

        SDL_RenderPresent(renderer);
    }
};

Game game;

extern "C" void mainloop() {
    Uint32 currentTime = SDL_GetTicks();
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    game.HandleInput();
    
    if (game.gameState == GameState::RUNNING) { // Ensure the game updates only in RUNNING state
        game.Update(deltaTime);
    }

    game.Render();
}

int main() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096);

    window = SDL_CreateWindow("Flappy Bird", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, 0);

    game.successSound = Mix_LoadWAV("assets/pass.ogg");
    game.smackSound = Mix_LoadWAV("assets/smack.ogg");

    if (TTF_Init() == -1) std::cerr << "SDL_ttf could not initialize! SDL_ttf Error: " << TTF_GetError() << std::endl;

    font = TTF_OpenFont("assets/ArcadeFont.ttf", 28);
    if (!font) std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;

    emscripten_set_main_loop(mainloop, 0, 1);
    
    Mix_FreeChunk(game.successSound);
    Mix_CloseAudio();
    SDL_DestroyRenderer(renderer);
    return 0;
}
