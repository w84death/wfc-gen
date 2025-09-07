#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>

#define PATTERN_SIZE 3
#define OUTPUT_WIDTH 96
#define OUTPUT_HEIGHT 96
#define MAX_PATTERNS 512
#define SCALE 6
#define DEFAULT_FILE "brick.png"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

typedef struct {
    Color pixels[PATTERN_SIZE][PATTERN_SIZE];
    int frequency;
    int index;
} Pattern;

typedef struct {
    bool possible[MAX_PATTERNS];
    int num_possible;
    bool collapsed;
    int final_pattern;
} Cell;

typedef struct {
    Pattern patterns[MAX_PATTERNS];
    int pattern_count;
    Cell grid[OUTPUT_WIDTH][OUTPUT_HEIGHT];
    Image input_image;
    Texture2D input_texture;
    bool adjacency[MAX_PATTERNS][MAX_PATTERNS][4]; // [pattern1][pattern2][direction]
    int generation_step;
    bool generation_complete;
} WFC;

// Direction helpers: 0=up, 1=right, 2=down, 3=left
int dx[] = {0, 1, 0, -1};
int dy[] = {-1, 0, 1, 0};

// Check if two patterns can be adjacent in given direction
bool patterns_compatible(Pattern *p1, Pattern *p2, int direction) {
    int overlap = PATTERN_SIZE - 1;

    switch(direction) {
        case 0: // p2 is above p1
            for(int x = 0; x < PATTERN_SIZE; x++) {
                for(int y = 0; y < overlap; y++) {
                    if(p1->pixels[y][x].r != p2->pixels[y+1][x].r ||
                       p1->pixels[y][x].g != p2->pixels[y+1][x].g ||
                       p1->pixels[y][x].b != p2->pixels[y+1][x].b) {
                        return false;
                    }
                }
            }
            break;
        case 1: // p2 is to the right of p1
            for(int y = 0; y < PATTERN_SIZE; y++) {
                for(int x = 0; x < overlap; x++) {
                    if(p1->pixels[y][x+1].r != p2->pixels[y][x].r ||
                       p1->pixels[y][x+1].g != p2->pixels[y][x].g ||
                       p1->pixels[y][x+1].b != p2->pixels[y][x].b) {
                        return false;
                    }
                }
            }
            break;
        case 2: // p2 is below p1
            for(int x = 0; x < PATTERN_SIZE; x++) {
                for(int y = 0; y < overlap; y++) {
                    if(p1->pixels[y+1][x].r != p2->pixels[y][x].r ||
                       p1->pixels[y+1][x].g != p2->pixels[y][x].g ||
                       p1->pixels[y+1][x].b != p2->pixels[y][x].b) {
                        return false;
                    }
                }
            }
            break;
        case 3: // p2 is to the left of p1
            for(int y = 0; y < PATTERN_SIZE; y++) {
                for(int x = 0; x < overlap; x++) {
                    if(p1->pixels[y][x].r != p2->pixels[y][x+1].r ||
                       p1->pixels[y][x].g != p2->pixels[y][x+1].g ||
                       p1->pixels[y][x].b != p2->pixels[y][x+1].b) {
                        return false;
                    }
                }
            }
            break;
    }
    return true;
}

// Check if pattern already exists in the list
int find_pattern(WFC *wfc, Pattern *p) {
    for(int i = 0; i < wfc->pattern_count; i++) {
        bool match = true;
        for(int y = 0; y < PATTERN_SIZE && match; y++) {
            for(int x = 0; x < PATTERN_SIZE && match; x++) {
                if(wfc->patterns[i].pixels[y][x].r != p->pixels[y][x].r ||
                   wfc->patterns[i].pixels[y][x].g != p->pixels[y][x].g ||
                   wfc->patterns[i].pixels[y][x].b != p->pixels[y][x].b) {
                    match = false;
                }
            }
        }
        if(match) return i;
    }
    return -1;
}

// Extract patterns from input image
void extract_patterns(WFC *wfc) {
    Color *pixels = LoadImageColors(wfc->input_image);
    int width = wfc->input_image.width;
    int height = wfc->input_image.height;

    wfc->pattern_count = 0;

    // Extract overlapping patterns
    for(int y = 0; y <= height - PATTERN_SIZE; y++) {
        for(int x = 0; x <= width - PATTERN_SIZE; x++) {
            Pattern p;
            p.frequency = 1;

            // Copy pattern pixels
            for(int py = 0; py < PATTERN_SIZE; py++) {
                for(int px = 0; px < PATTERN_SIZE; px++) {
                    p.pixels[py][px] = pixels[(y + py) * width + (x + px)];
                }
            }

            // Check if pattern already exists
            int existing = find_pattern(wfc, &p);
            if(existing >= 0) {
                wfc->patterns[existing].frequency++;
            } else if(wfc->pattern_count < MAX_PATTERNS) {
                p.index = wfc->pattern_count;
                wfc->patterns[wfc->pattern_count] = p;
                wfc->pattern_count++;
            }
        }
    }

    UnloadImageColors(pixels);

    // Build adjacency rules
    for(int i = 0; i < wfc->pattern_count; i++) {
        for(int j = 0; j < wfc->pattern_count; j++) {
            for(int d = 0; d < 4; d++) {
                wfc->adjacency[i][j][d] = patterns_compatible(&wfc->patterns[i], &wfc->patterns[j], d);
            }
        }
    }

    printf("Extracted %d unique patterns\n", wfc->pattern_count);
}

// Initialize the grid with all possibilities
void init_grid(WFC *wfc) {
    for(int y = 0; y < OUTPUT_HEIGHT; y++) {
        for(int x = 0; x < OUTPUT_WIDTH; x++) {
            wfc->grid[y][x].collapsed = false;
            wfc->grid[y][x].num_possible = wfc->pattern_count;
            wfc->grid[y][x].final_pattern = -1;
            for(int p = 0; p < wfc->pattern_count; p++) {
                wfc->grid[y][x].possible[p] = true;
            }
        }
    }
    wfc->generation_step = 0;
    wfc->generation_complete = false;
}

// Calculate entropy (number of possible patterns) for a cell
int calculate_entropy(Cell *cell) {
    if(cell->collapsed) return 1000000;
    return cell->num_possible;
}

// Find the cell with minimum entropy
bool find_min_entropy_cell(WFC *wfc, int *min_x, int *min_y) {
    int min_entropy = 1000000;
    bool found = false;

    // Add some randomness to tie-breaking
    int start_x = rand() % OUTPUT_WIDTH;
    int start_y = rand() % OUTPUT_HEIGHT;

    for(int i = 0; i < OUTPUT_HEIGHT; i++) {
        int y = (start_y + i) % OUTPUT_HEIGHT;
        for(int j = 0; j < OUTPUT_WIDTH; j++) {
            int x = (start_x + j) % OUTPUT_WIDTH;
            if(!wfc->grid[y][x].collapsed) {
                int entropy = calculate_entropy(&wfc->grid[y][x]);
                if(entropy > 0 && entropy < min_entropy) {
                    min_entropy = entropy;
                    *min_x = x;
                    *min_y = y;
                    found = true;
                }
            }
        }
    }
    return found;
}

// Collapse a cell to a specific pattern
void collapse_cell(WFC *wfc, int x, int y) {
    Cell *cell = &wfc->grid[y][x];
    if(cell->collapsed || cell->num_possible == 0) return;

    // Build weighted list of possible patterns
    int total_weight = 0;
    int weights[MAX_PATTERNS];
    int valid_patterns[MAX_PATTERNS];
    int valid_count = 0;

    for(int p = 0; p < wfc->pattern_count; p++) {
        if(cell->possible[p]) {
            valid_patterns[valid_count] = p;
            weights[valid_count] = wfc->patterns[p].frequency;
            total_weight += weights[valid_count];
            valid_count++;
        }
    }

    if(valid_count == 0) return;

    // Choose random pattern weighted by frequency
    int r = rand() % total_weight;
    int chosen = 0;
    for(int i = 0; i < valid_count; i++) {
        r -= weights[i];
        if(r < 0) {
            chosen = valid_patterns[i];
            break;
        }
    }

    // Collapse to chosen pattern
    for(int p = 0; p < wfc->pattern_count; p++) {
        cell->possible[p] = (p == chosen);
    }
    cell->num_possible = 1;
    cell->collapsed = true;
    cell->final_pattern = chosen;
}

// Propagate constraints from a collapsed cell
void propagate(WFC *wfc, int x, int y) {
    bool changed[OUTPUT_HEIGHT][OUTPUT_WIDTH];
    memset(changed, 0, sizeof(changed));
    changed[y][x] = true;

    bool any_changed = true;
    while(any_changed) {
        any_changed = false;

        for(int cy = 0; cy < OUTPUT_HEIGHT; cy++) {
            for(int cx = 0; cx < OUTPUT_WIDTH; cx++) {
                if(!changed[cy][cx]) continue;
                changed[cy][cx] = false;

                // Check each neighbor
                for(int d = 0; d < 4; d++) {
                    int nx = cx + dx[d];
                    int ny = cy + dy[d];

                    if(nx < 0 || nx >= OUTPUT_WIDTH || ny < 0 || ny >= OUTPUT_HEIGHT) continue;
                    if(wfc->grid[ny][nx].collapsed) continue;

                    Cell *neighbor = &wfc->grid[ny][nx];
                    bool neighbor_changed = false;

                    // Check which patterns are still valid for neighbor
                    for(int np = 0; np < wfc->pattern_count; np++) {
                        if(!neighbor->possible[np]) continue;

                        bool valid = false;
                        // Check if any pattern in current cell allows this neighbor pattern
                        for(int cp = 0; cp < wfc->pattern_count; cp++) {
                            if(wfc->grid[cy][cx].possible[cp] && wfc->adjacency[cp][np][d]) {
                                valid = true;
                                break;
                            }
                        }

                        if(!valid) {
                            neighbor->possible[np] = false;
                            neighbor->num_possible--;
                            neighbor_changed = true;
                        }
                    }

                    if(neighbor_changed) {
                        changed[ny][nx] = true;
                        any_changed = true;
                    }
                }
            }
        }
    }
}

// Perform one step of WFC generation
void wfc_step(WFC *wfc) {
    if(wfc->generation_complete) return;

    int x, y;
    if(find_min_entropy_cell(wfc, &x, &y)) {
        collapse_cell(wfc, x, y);
        propagate(wfc, x, y);
        wfc->generation_step++;
    } else {
        wfc->generation_complete = true;
        printf("Generation complete after %d steps\n", wfc->generation_step);
    }
}

// Draw the current state of the grid
void draw_output(WFC *wfc, int offset_x, int offset_y) {
    for(int y = 0; y < OUTPUT_HEIGHT; y++) {
        for(int x = 0; x < OUTPUT_WIDTH; x++) {
            Cell *cell = &wfc->grid[y][x];
            Color color = LIGHTGRAY;

            if(cell->collapsed && cell->final_pattern >= 0) {
                // Use center pixel of the pattern as representative color
                color = wfc->patterns[cell->final_pattern].pixels[PATTERN_SIZE/2][PATTERN_SIZE/2];
            } else if(cell->num_possible > 0) {
                // Show entropy as grayscale
                int brightness = 255 * cell->num_possible / wfc->pattern_count;
                color = (Color){brightness, brightness, brightness, 255};
            } else {
                color = RED; // Error state
            }

            DrawRectangle(offset_x + x * SCALE, offset_y + y * SCALE,
                         SCALE, SCALE, color);
        }
    }
}

int main(int argc, char *argv[]) {
    const char *input_file = DEFAULT_FILE;
    if(argc > 1) {
        input_file = argv[1];
    }

    // Initialize random seed
    srand(time(NULL));

    // Initialize WFC
    WFC wfc = {0};

    // Load input image
    wfc.input_image = LoadImage(input_file);
    if(wfc.input_image.data == NULL) {
        printf("Failed to load image: %s\n", input_file);
        return 1;
    }

    // Initialize window
    int screenWidth = WINDOW_WIDTH;
    int screenHeight = WINDOW_HEIGHT;
    InitWindow(screenWidth, screenHeight, "Wave Function Collapse - RayLib");
    SetTargetFPS(60);

    // Create texture from input image
    wfc.input_texture = LoadTextureFromImage(wfc.input_image);

    // Extract patterns and initialize grid
    extract_patterns(&wfc);
    init_grid(&wfc);

    // Control variables
    bool auto_generate = false;

    while(!WindowShouldClose()) {
        // Input handling
        if(IsKeyPressed(KEY_SPACE)) {
            auto_generate = !auto_generate;
        }
        if(IsKeyPressed(KEY_R)) {
            init_grid(&wfc);
        }

        // Run generation at max speed
        if(IsKeyPressed(KEY_S)) {
            wfc_step(&wfc);
        } else if(auto_generate && !wfc.generation_complete) {
            // Run multiple steps per frame for maximum speed
            for(int i = 0; i < 25 && !wfc.generation_complete; i++) {
                wfc_step(&wfc);
            }
        }

        // Drawing
        BeginDrawing();
        ClearBackground(BLACK);

        // Draw input image
        DrawText("Input Image", 50, 20, 20, WHITE);
        float scale = 200.0f / fmax(wfc.input_image.width, wfc.input_image.height);
        DrawTextureEx(wfc.input_texture, (Vector2){50, 50}, 0, scale, WHITE);

        // Draw output
        DrawText("WFC Output", 600, 20, 20, WHITE);
        draw_output(&wfc, 600, 50);

        // Draw controls
        DrawText("Controls:", 50, 300, 16, WHITE);
        DrawText("SPACE - Toggle auto generation", 50, 320, 14, LIGHTGRAY);
        DrawText("S - Single step", 50, 340, 14, LIGHTGRAY);
        DrawText("R - Reset", 50, 360, 14, LIGHTGRAY);

        // Draw status
        char status[256];
        sprintf(status, "Step: %d | Auto: %s | Status: %s",
                wfc.generation_step,
                auto_generate ? "ON" : "OFF",
                wfc.generation_complete ? "COMPLETE" : "GENERATING");
        DrawText(status, 50, 400, 14, GREEN);

        sprintf(status, "Patterns: %d | Grid: %dx%d",
                wfc.pattern_count, OUTPUT_WIDTH, OUTPUT_HEIGHT);
        DrawText(status, 50, 420, 14, GREEN);

        EndDrawing();
    }

    // Cleanup
    UnloadTexture(wfc.input_texture);
    UnloadImage(wfc.input_image);
    CloseWindow();

    return 0;
}
