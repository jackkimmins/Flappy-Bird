#include <SDL.h>
#include <emscripten.h>
#include <vector>
#include <random>

constexpr int WIDTH = 1200;
constexpr int HEIGHT = 800;
constexpr float GRAVITY = 0.5f;
constexpr float BIRD_JUMP = -10.0f;
constexpr int PIPE_SPEED = 1;
constexpr float TERMINAL_VELOCITY = 2.0f;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

std::mt19937 gen(std::random_device{}());
std::uniform_int_distribution<int> dist(0, HEIGHT - 180);

class Bird {
public:
    SDL_Rect rect{ WIDTH / 4, HEIGHT / 2, 20, 20 };
private:
    float velocity = 0.0f;
public:
    inline void Update() {
        velocity = std::min(velocity + GRAVITY, TERMINAL_VELOCITY);
        rect.y += static_cast<int>(velocity);
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

    inline void Update() {
        x -= PIPE_SPEED;
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
private:
    Bird bird;
    std::vector<Pipe> pipes;

    inline void Reset() {
        bird = Bird();
        pipes.clear();
    }

    inline void OnPipePassed() {
        // score++; (currently a stub)
    }
public:
    inline void HandleInput() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                emscripten_cancel_main_loop();
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) {
                bird.Jump();
            }
        }
    }

    inline void Update() {
        bird.Update();

        if (pipes.empty() || WIDTH - pipes.back().x >= 400) {
            pipes.emplace_back();
        }

        for (auto& pipe : pipes) {
            pipe.Update();
        }

        pipes.erase(std::remove_if(pipes.begin(), pipes.end(), [](const Pipe& pipe) {
            return pipe.x + 60 < 0;
        }), pipes.end());

        for (auto& pipe : pipes) {
            if (bird.rect.x < pipe.x + 60 && bird.rect.x + bird.rect.w > pipe.x && 
                (bird.rect.y < pipe.gapY || bird.rect.y + bird.rect.h > pipe.gapY + 180)) {
                pipe.hasPassed = true;
                OnPipePassed();
                Reset();
                return;
            }
        }

        if (bird.rect.y < 0 || bird.rect.y + bird.rect.h > HEIGHT) {
            Reset();
        }
    }

    inline void Render() {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        bird.Draw();
        for (const auto& pipe : pipes) {
            pipe.Draw();
        }
        SDL_RenderPresent(renderer);
    }
};

Game game;

extern "C" void mainloop() {
    game.HandleInput();
    game.Update();
    game.Render();
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Flappy Bird", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, 0);
    emscripten_set_main_loop(mainloop, 0, 1);
    return 0;
}