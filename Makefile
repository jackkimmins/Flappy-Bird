CXX = em++
CXXFLAGS = -O3 --std=c++20 -s USE_SDL=2 -s USE_SDL_TTF=2 -s USE_SDL_MIXER=2
TARGET = web/index.html
SOURCE = code.cpp
SHELL_FILE = shell.html
STYLE = style.css
ASSETS = assets

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $^ -o $@ --shell-file $(SHELL_FILE) --preload-file $(ASSETS)
	cp $(STYLE) web/

clean:
	rm -rf $(TARGET)
	rm -f web/$(STYLE)

.PHONY: all clean