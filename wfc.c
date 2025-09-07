#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>

#define PATTERN_SIZE 3
#define OUTPUT_WIDTH 80
#define OUTPUT_HEIGHT 80
#define MAX_PATTERNS 255
#define SCALE 8
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
    // Progress tracking
    bool patterns_extracted;
    int extraction_x;
    int extraction_y;
    int extraction_total;
    int extraction_progress;
    bool adjacency_built;
    int adjacency_i;
    int adjacency_j;
    int adjacency_d;
    int adjacency_total;
    int adjacency_progress;
    bool grid_initialized;
    int grid_init_x;
    int grid_init_y;
    int grid_init_total;
    int grid_init_progress;
    Color *input_pixels;
    char current_operation[256];
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

// Initialize pattern extraction
void init_pattern_extraction(WFC *wfc) {
    wfc->input_pixels = LoadImageColors(wfc->input_image);
    int width = wfc->input_image.width;
    int height = wfc->input_image.height;

    wfc->pattern_count = 0;
    wfc->extraction_x = 0;
    wfc->extraction_y = 0;
    wfc->extraction_total = (height - PATTERN_SIZE + 1) * (width - PATTERN_SIZE + 1);
    wfc->extraction_progress = 0;
    wfc->patterns_extracted = false;
    sprintf(wfc->current_operation, "Extracting patterns from input image...");
}

// Process pattern extraction step by step
bool extract_patterns_step(WFC *wfc, int steps_per_frame) {
    int width = wfc->input_image.width;
    int height = wfc->input_image.height;

    for(int step = 0; step < steps_per_frame; step++) {
        if(wfc->extraction_y > height - PATTERN_SIZE) {
            // Extraction complete, clean up
            UnloadImageColors(wfc->input_pixels);
            wfc->input_pixels = NULL;
            wfc->patterns_extracted = true;
            sprintf(wfc->current_operation, "Building adjacency rules...");

            // Initialize adjacency building
            wfc->adjacency_i = 0;
            wfc->adjacency_j = 0;
            wfc->adjacency_d = 0;
            wfc->adjacency_total = wfc->pattern_count * wfc->pattern_count * 4;
            wfc->adjacency_progress = 0;
            wfc->adjacency_built = false;

            printf("Extracted %d unique patterns\n", wfc->pattern_count);
            return true;
        }

        Pattern p;
        p.frequency = 1;

        // Copy pattern pixels
        for(int py = 0; py < PATTERN_SIZE; py++) {
            for(int px = 0; px < PATTERN_SIZE; px++) {
                p.pixels[py][px] = wfc->input_pixels[(wfc->extraction_y + py) * width + (wfc->extraction_x + px)];
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

        wfc->extraction_progress++;

        // Move to next position
        wfc->extraction_x++;
        if(wfc->extraction_x > width - PATTERN_SIZE) {
            wfc->extraction_x = 0;
            wfc->extraction_y++;
        }
    }

    return false; // Not done yet
}

// Build adjacency rules step by step
bool build_adjacency_step(WFC *wfc, int steps_per_frame) {
    for(int step = 0; step < steps_per_frame; step++) {
        if(wfc->adjacency_i >= wfc->pattern_count) {
            // Adjacency building complete
            wfc->adjacency_built = true;
            sprintf(wfc->current_operation, "Ready");
            return true;
        }

        wfc->adjacency[wfc->adjacency_i][wfc->adjacency_j][wfc->adjacency_d] =
            patterns_compatible(&wfc->patterns[wfc->adjacency_i],
                              &wfc->patterns[wfc->adjacency_j],
                              wfc->adjacency_d);
        wfc->adjacency_progress++;

        // Move to next adjacency
        wfc->adjacency_d++;
        if(wfc->adjacency_d >= 4) {
            wfc->adjacency_d = 0;
            wfc->adjacency_j++;
            if(wfc->adjacency_j >= wfc->pattern_count) {
                wfc->adjacency_j = 0;
                wfc->adjacency_i++;
            }
        }
    }

    return false; // Not done yet
}

// Start grid initialization
void init_grid_start(WFC *wfc) {
    wfc->grid_init_x = 0;
    wfc->grid_init_y = 0;
    wfc->grid_init_total = OUTPUT_WIDTH * OUTPUT_HEIGHT;
    wfc->grid_init_progress = 0;
    wfc->grid_initialized = false;
    wfc->generation_step = 0;
    wfc->generation_complete = false;
    sprintf(wfc->current_operation, "Initializing grid...");
}

// Process grid initialization step by step
bool init_grid_step(WFC *wfc, int cells_per_frame) {
    for(int step = 0; step < cells_per_frame; step++) {
        if(wfc->grid_init_y >= OUTPUT_HEIGHT) {
            // Grid initialization complete
            wfc->grid_initialized = true;
            sprintf(wfc->current_operation, "Ready");
            return true;
        }

        // Initialize current cell
        wfc->grid[wfc->grid_init_y][wfc->grid_init_x].collapsed = false;
        wfc->grid[wfc->grid_init_y][wfc->grid_init_x].num_possible = wfc->pattern_count;
        wfc->grid[wfc->grid_init_y][wfc->grid_init_x].final_pattern = -1;
        for(int p = 0; p < wfc->pattern_count; p++) {
            wfc->grid[wfc->grid_init_y][wfc->grid_init_x].possible[p] = true;
        }

        wfc->grid_init_progress++;

        // Move to next cell
        wfc->grid_init_x++;
        if(wfc->grid_init_x >= OUTPUT_WIDTH) {
            wfc->grid_init_x = 0;
            wfc->grid_init_y++;
        }
    }

    return false; // Not done yet
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
            Color color = BLACK;

            if(cell->collapsed && cell->final_pattern >= 0) {
                // Use center pixel of the pattern as representative color
                color = wfc->patterns[cell->final_pattern].pixels[PATTERN_SIZE/2][PATTERN_SIZE/2];
            } else if(cell->num_possible > 0) {
                // Show entropy as very dark grayscale for better blending
                int brightness = 30 * cell->num_possible / wfc->pattern_count;  // Max 30 instead of 255
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

    // Initialize window FIRST so we can show progress
    int screenWidth = WINDOW_WIDTH;
    int screenHeight = WINDOW_HEIGHT;
    InitWindow(screenWidth, screenHeight, "Wave Function Collapse - RayLib");
    SetTargetFPS(60);

    // Initialize WFC
    WFC wfc = {0};
    sprintf(wfc.current_operation, "Loading input image...");

    // Load input image
    wfc.input_image = LoadImage(input_file);
    if(wfc.input_image.data == NULL) {
        printf("Failed to load image: %s\n", input_file);
        CloseWindow();
        return 1;
    }

    // Create texture from input image
    wfc.input_texture = LoadTextureFromImage(wfc.input_image);

    // Initialize pattern extraction (will be done incrementally in main loop)
    init_pattern_extraction(&wfc);

    // Control variables
    bool auto_generate = false;
    bool initialization_complete = false;
    bool needs_grid_reset = false;
    bool needs_new_patterns = false;

    while(!WindowShouldClose()) {
        // Handle initialization phases first
        if(!wfc.patterns_extracted || needs_new_patterns) {
            // Extract patterns incrementally
            if(needs_new_patterns && wfc.patterns_extracted) {
                // Re-initialize extraction for new patterns
                init_pattern_extraction(&wfc);
                needs_new_patterns = false;
                auto_generate = false;  // Stop auto generation during re-extraction
            }
            extract_patterns_step(&wfc, 100);  // Process 100 patterns per frame
        } else if(!wfc.adjacency_built) {
            // Build adjacency rules incrementally
            build_adjacency_step(&wfc, 500);  // Process 500 rules per frame
        } else if(!initialization_complete) {
            // Initialize grid after patterns and adjacency are ready
            init_grid_start(&wfc);
            initialization_complete = true;
            wfc.grid_initialized = false;  // Force grid initialization
        } else if(!wfc.grid_initialized || needs_grid_reset) {
            // Initialize or reset grid incrementally
            if(needs_grid_reset) {
                init_grid_start(&wfc);
                needs_grid_reset = false;
            }
            init_grid_step(&wfc, 200);  // Process 200 cells per frame
        } else {
            // Normal operation - only process input after initialization
            if(IsKeyPressed(KEY_SPACE)) {
                auto_generate = !auto_generate;
                if(!auto_generate && !wfc.generation_complete) {
                    sprintf(wfc.current_operation, "Paused");
                }
            }
            if(IsKeyPressed(KEY_R) && wfc.grid_initialized) {
                needs_grid_reset = true;
                auto_generate = false;  // Stop auto generation during reset
            }
            if(IsKeyPressed(KEY_N) && wfc.grid_initialized) {
                // Extract new patterns from input
                needs_new_patterns = true;
                wfc.patterns_extracted = false;
                wfc.adjacency_built = false;
                wfc.grid_initialized = false;
                initialization_complete = false;
                auto_generate = false;
            }

            // Run generation at max speed
            if(IsKeyPressed(KEY_S)) {
                sprintf(wfc.current_operation, "Generating (Step mode)");
                wfc_step(&wfc);
                if(wfc.generation_complete) {
                    sprintf(wfc.current_operation, "Generation complete!");
                }
            } else if(auto_generate && !wfc.generation_complete) {
                sprintf(wfc.current_operation, "Generating (Auto mode)");
                // Run multiple steps per frame for maximum speed
                for(int i = 0; i < 25 && !wfc.generation_complete; i++) {
                    wfc_step(&wfc);
                }
                if(wfc.generation_complete) {
                    sprintf(wfc.current_operation, "Generation complete!");
                }
            }
        }

        // Drawing
        BeginDrawing();
        ClearBackground(BLACK);

        // Show different UI based on initialization state
        if(!wfc.patterns_extracted) {
            // Show pattern extraction progress
            DrawText(wfc.current_operation, 50, 200, 20, WHITE);

            char progress_text[256];
            sprintf(progress_text, "Scanning patterns: %d of %d locations",
                    wfc.extraction_progress, wfc.extraction_total);
            DrawText(progress_text, 50, 230, 16, LIGHTGRAY);

            sprintf(progress_text, "Unique patterns found: %d", wfc.pattern_count);
            DrawText(progress_text, 50, 250, 16, LIGHTGRAY);

            // Draw progress bar
            int bar_width = 400;
            int bar_height = 20;
            float progress = wfc.extraction_total > 0 ? (float)wfc.extraction_progress / wfc.extraction_total : 0;

            DrawRectangle(50, 280, bar_width, bar_height, DARKGRAY);
            DrawRectangle(50, 280, (int)(bar_width * progress), bar_height, GREEN);
            DrawRectangleLines(50, 280, bar_width, bar_height, WHITE);

            sprintf(progress_text, "%.1f%%", progress * 100);
            DrawText(progress_text, 50 + bar_width + 10, 280, 16, WHITE);

        } else if(!wfc.adjacency_built) {
            // Show adjacency building progress
            DrawText(wfc.current_operation, 50, 200, 20, WHITE);

            char progress_text[256];
            sprintf(progress_text, "Processing rule %d of %d",
                    wfc.adjacency_progress, wfc.adjacency_total);
            DrawText(progress_text, 50, 230, 16, LIGHTGRAY);

            // Draw progress bar
            int bar_width = 400;
            int bar_height = 20;
            float progress = wfc.adjacency_total > 0 ? (float)wfc.adjacency_progress / wfc.adjacency_total : 0;

            DrawRectangle(50, 260, bar_width, bar_height, DARKGRAY);
            DrawRectangle(50, 260, (int)(bar_width * progress), bar_height, GREEN);
            DrawRectangleLines(50, 260, bar_width, bar_height, WHITE);

            sprintf(progress_text, "%.1f%%", progress * 100);
            DrawText(progress_text, 50 + bar_width + 10, 260, 16, WHITE);

        } else if(!wfc.grid_initialized) {
            // Show grid initialization progress
            DrawText(wfc.current_operation, 50, 200, 20, WHITE);

            char progress_text[256];
            sprintf(progress_text, "Initializing cell %d of %d",
                    wfc.grid_init_progress, wfc.grid_init_total);
            DrawText(progress_text, 50, 230, 16, LIGHTGRAY);

            sprintf(progress_text, "Setting up %d possible patterns per cell", wfc.pattern_count);
            DrawText(progress_text, 50, 250, 16, LIGHTGRAY);

            // Draw progress bar
            int bar_width = 400;
            int bar_height = 20;
            float progress = wfc.grid_init_total > 0 ? (float)wfc.grid_init_progress / wfc.grid_init_total : 0;

            DrawRectangle(50, 280, bar_width, bar_height, DARKGRAY);
            DrawRectangle(50, 280, (int)(bar_width * progress), bar_height, ORANGE);
            DrawRectangleLines(50, 280, bar_width, bar_height, WHITE);

            sprintf(progress_text, "%.1f%%", progress * 100);
            DrawText(progress_text, 50 + bar_width + 10, 280, 16, WHITE);

        } else {
            // Normal UI after initialization
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
            DrawText("R - Reset grid (keep same patterns)", 50, 360, 14, LIGHTGRAY);
            DrawText("N - Extract NEW patterns from input", 50, 380, 14, LIGHTGRAY);

            // Draw status
            char status[256];
            sprintf(status, "Step: %d | Auto: %s | Status: %s",
                    wfc.generation_step,
                    auto_generate ? "ON" : "OFF",
                    wfc.generation_complete ? "COMPLETE" : "GENERATING");
            DrawText(status, 50, 420, 14, GREEN);

            sprintf(status, "Patterns: %d | Grid: %dx%d | Operation: %s",
                    wfc.pattern_count, OUTPUT_WIDTH, OUTPUT_HEIGHT, wfc.current_operation);
            DrawText(status, 50, 440, 14, GREEN);

            // Show progress bar during generation
            if(!wfc.generation_complete && (auto_generate || wfc.generation_step > 0)) {
                int total_cells = OUTPUT_WIDTH * OUTPUT_HEIGHT;
                float progress = (float)wfc.generation_step / total_cells;
                int bar_width = 300;
                int bar_height = 10;

                DrawRectangle(50, 465, bar_width, bar_height, DARKGRAY);
                DrawRectangle(50, 465, (int)(bar_width * progress), bar_height, BLUE);
                DrawRectangleLines(50, 465, bar_width, bar_height, WHITE);

                sprintf(status, "Progress: %d/%d cells", wfc.generation_step, total_cells);
                DrawText(status, 360, 462, 12, LIGHTGRAY);
            }
        }

        EndDrawing();
    }

    // Cleanup
    UnloadTexture(wfc.input_texture);
    UnloadImage(wfc.input_image);
    CloseWindow();

    return 0;
}
