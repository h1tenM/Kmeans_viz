#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <float.h>
#include "raylib.h"
#include "raymath.h"

// --- Configuration ---
#define WIDTH 800
#define HEIGHT 600

#define MIN_X -100
#define MIN_Y -100
#define MAX_X 100
#define MAX_Y 100

#define SAMPLE_PER_CLUSTER 50
#define NUM_GENERATED_CLUSTERS 5
#define NUM_KMEANS_CENTERS 5

#define POINT_RADIUS 4
#define CENTER_RADIUS 8

// --- Colors & UI ---
#define COLOR_BG GetColor(0x181818)
#define COLOR_UI_BG Fade(BLACK, 0.6f)
#define COLOR_UI_BORDER LIGHTGRAY
#define COLOR_CENTER_GLOW GetColor(0xFFFFFF77)

// --- Structs ---

typedef enum {
    SOURCE_RANDOM,
    SOURCE_CSV
} DataSource;

typedef enum {
    SHAPE_CIRCLE,
    SHAPE_SQUARE,
    SHAPE_TRIANGLE,
    SHAPE_DIAMOND,
    SHAPE_CROSS
} KShape;

// A single data point
typedef struct {
    Vector2 position;
    int ground_truth_id; // The ID of the cluster that generated this point (-1 if CSV)
} Point;

typedef struct {
    Point* items;
    size_t count;
    size_t capacity;
} DataPoints;

// Used for "Ground Truth" generation
typedef struct {
    Vector2 mean;
    float std;
    Color color;
} GeneratorCluster;

// The K-Means Agents
typedef struct {
    Vector2 position;
    float radius; 
    KShape shape;
    Color color; // The color associated with this K-Means center
} KMeansCenter;

// --- Globals ---
DataPoints data = {0};
int* assignments = NULL; // Dynamic array mapping point_index -> center_index
KMeansCenter km_centers[NUM_KMEANS_CENTERS];
GeneratorCluster gen_clusters[NUM_GENERATED_CLUSTERS];

int iteration_count = 0;
DataSource current_source = SOURCE_RANDOM;

// Standard Palette
Color palette[] = {RED, BLUE, GREEN, YELLOW, PURPLE, ORANGE, PINK, LIME};

// --- Function Prototypes ---
void init_random_data();
void load_from_csv(const char* filename);
void reset_kmeans();
void step_kmeans();

// Helpers
float rand_float(float min, float max);
Vector2 project_to_screen(Vector2 world_pos);
Vector2 generate_gaussian_point(Vector2 mean, float std);

// Rendering
void draw_points();
void draw_centers();
void draw_ui_panel();

// --- Main ---

int main(void) {
    srand((unsigned int)time(NULL));

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WIDTH, HEIGHT, "K-Means: Truth vs Belief");
    SetTargetFPS(60);

    // Start with random data
    init_random_data();

    // To load CSV immediately on startup, uncomment this:
    // load_from_csv("data.csv"); 

    while (!WindowShouldClose()) {
        // --- Input ---
        if (IsKeyPressed(KEY_R)) {
            if (current_source == SOURCE_RANDOM) init_random_data();
            else reset_kmeans(); // Keep CSV data, just reset centers
        }
        
        // Helper to switch to CSV mode for testing
        if (IsKeyPressed(KEY_L)) {
            load_from_csv("data.csv");
        }

        if (IsKeyPressed(KEY_SPACE)) {
            step_kmeans();
        }

        // --- Draw ---
        BeginDrawing();
            ClearBackground(COLOR_BG);

            draw_points();
            draw_centers();
            draw_ui_panel();

        EndDrawing();
    }

    if (data.items) free(data.items);
    if (assignments) free(assignments);
    
    CloseWindow();
    return 0;
}

// --- Implementation ---

void init_random_data() {
    current_source = SOURCE_RANDOM;
    data.count = 0;
    
    // Ensure memory
    if (data.capacity == 0) {
        data.capacity = NUM_GENERATED_CLUSTERS * SAMPLE_PER_CLUSTER;
        data.items = (Point*)malloc(sizeof(Point) * data.capacity);
    }

    // Generate Ground Truth Clusters
    for (int i = 0; i < NUM_GENERATED_CLUSTERS; ++i) {
        gen_clusters[i].mean = (Vector2){
            rand_float(MIN_X * 0.8f, MAX_X * 0.8f),
            rand_float(MIN_Y * 0.8f, MAX_Y * 0.8f)
        };
        gen_clusters[i].std = rand_float(5, 15);
        gen_clusters[i].color = palette[i % 8];

        // Generate Points
        for (int j = 0; j < SAMPLE_PER_CLUSTER; ++j) {
            data.items[data.count].position = generate_gaussian_point(gen_clusters[i].mean, gen_clusters[i].std);
            data.items[data.count].ground_truth_id = i; // SAVE THE TRUTH
            data.count++;
        }
    }
    
    // Resize assignments array if needed
    if (assignments) free(assignments);
    assignments = (int*)malloc(sizeof(int) * data.count);

    reset_kmeans();
}

void load_from_csv(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Could not open file %s\n", filename);
        return;
    }

    current_source = SOURCE_CSV;
    size_t cap = 1000;
    size_t count = 0;
    Point* temp_pts = (Point*)malloc(sizeof(Point) * cap);

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        float x, y;
        if (sscanf(line, "%f%*c%f", &x, &y) >= 2) {
            if (count >= cap) {
                cap *= 2;
                temp_pts = (Point*)realloc(temp_pts, sizeof(Point) * cap);
            }
            if(x > MAX_X) x = MAX_X; if(x < MIN_X) x = MIN_X;
            if(y > MAX_Y) y = MAX_Y; if(y < MIN_Y) y = MIN_Y;
            
            temp_pts[count].position = (Vector2){x, y};
            temp_pts[count].ground_truth_id = -1; // UNKNOWN TRUTH
            count++;
        }
    }
    fclose(f);

    if (data.items) free(data.items);
    data.items = temp_pts;
    data.count = count;
    data.capacity = cap;

    if (assignments) free(assignments);
    assignments = (int*)malloc(sizeof(int) * data.count);

    reset_kmeans();
}

void reset_kmeans() {
    iteration_count = 0;
    
    for (int i = 0; i < NUM_KMEANS_CENTERS; ++i) {
        km_centers[i].position = (Vector2){ rand_float(MIN_X, MAX_X), rand_float(MIN_Y, MAX_Y) };
        km_centers[i].radius = 0;
        km_centers[i].shape = (KShape)(i % 5);
        // In CSV mode, the center determines color. In Random mode, the point determines color.
        km_centers[i].color = palette[i % 8]; 
    }

    for(size_t i=0; i<data.count; i++) assignments[i] = -1;
}

void step_kmeans() {
    // 1. Assign
    for (size_t i = 0; i < data.count; ++i) {
        float min_dist_sq = FLT_MAX;
        int best_k = -1;

        for (int k = 0; k < NUM_KMEANS_CENTERS; ++k) {
            float d = Vector2LengthSqr(Vector2Subtract(data.items[i].position, km_centers[k].position));
            if (d < min_dist_sq) {
                min_dist_sq = d;
                best_k = k;
            }
        }
        assignments[i] = best_k;
    }

    // 2. Update
    Vector2 sums[NUM_KMEANS_CENTERS] = {0};
    int counts[NUM_KMEANS_CENTERS] = {0};
    float dist_sums[NUM_KMEANS_CENTERS] = {0};

    for (size_t i = 0; i < data.count; ++i) {
        int k = assignments[i];
        if (k >= 0) {
            sums[k] = Vector2Add(sums[k], data.items[i].position);
            counts[k]++;
            dist_sums[k] += Vector2LengthSqr(Vector2Subtract(data.items[i].position, km_centers[k].position));
        }
    }

    for (int k = 0; k < NUM_KMEANS_CENTERS; ++k) {
        if (counts[k] > 0) {
            km_centers[k].position = Vector2Scale(sums[k], 1.0f / counts[k]);
            km_centers[k].radius = sqrtf(dist_sums[k] / counts[k]);
        } else {
            // Re-seed
            km_centers[k].position = (Vector2){ rand_float(MIN_X, MAX_X), rand_float(MIN_Y, MAX_Y) };
            km_centers[k].radius = 0;
        }
    }
    iteration_count++;
}

// --- Rendering ---

void draw_points() {
    for (size_t i = 0; i < data.count; ++i) {
        Vector2 screen_pos = project_to_screen(data.items[i].position);
        int k = assignments[i]; // The "Belief" index
        int truth = data.items[i].ground_truth_id; // The "Truth" index
        
        Color render_color;
        KShape render_shape;

        // --- VISUAL PHILOSOPHY ---
        if (current_source == SOURCE_RANDOM) {
            // Truth = Color (from generation), Belief = Shape (from K-means)
            render_color = (truth >= 0) ? palette[truth % 8] : DARKGRAY;
            render_shape = (k >= 0) ? km_centers[k].shape : SHAPE_CIRCLE;
        } else {
            // CSV Mode: No truth known.
            // Belief = Color AND Shape (standard visualization)
            render_color = (k >= 0) ? km_centers[k].color : DARKGRAY;
            render_shape = (k >= 0) ? km_centers[k].shape : SHAPE_CIRCLE;
        }
        
        if (k < 0 && current_source == SOURCE_CSV) { 
            // If CSV and unassigned, dim it out
            DrawPixelV(screen_pos, GRAY); 
            continue; 
        }

        // Draw based on calculated Shape and Color
        switch (render_shape) {
            case SHAPE_CIRCLE: DrawCircleV(screen_pos, POINT_RADIUS, render_color); break;
            case SHAPE_SQUARE: DrawRectangleV(Vector2Subtract(screen_pos, (Vector2){3,3}), (Vector2){6,6}, render_color); break;
            case SHAPE_TRIANGLE: DrawTriangle((Vector2){screen_pos.x, screen_pos.y-4}, (Vector2){screen_pos.x-4, screen_pos.y+4}, (Vector2){screen_pos.x+4, screen_pos.y+4}, render_color); break;
            case SHAPE_DIAMOND: DrawPoly(screen_pos, 4, 4, 0.0f, render_color); break;
            case SHAPE_CROSS: 
                DrawLine(screen_pos.x-4, screen_pos.y, screen_pos.x+4, screen_pos.y, render_color);
                DrawLine(screen_pos.x, screen_pos.y-4, screen_pos.x, screen_pos.y+4, render_color);
                break;
        }
    }
}

void draw_centers() {
    if (iteration_count == 0) return;

    for (int k = 0; k < NUM_KMEANS_CENTERS; ++k) {
        Vector2 pos = project_to_screen(km_centers[k].position);
        
        // Draw Range
        Vector2 edge = project_to_screen(Vector2Add(km_centers[k].position, (Vector2){km_centers[k].radius, 0}));
        float screen_r = fabsf(edge.x - pos.x);
        DrawCircleLines((int)pos.x, (int)pos.y, screen_r, COLOR_CENTER_GLOW);

        // Center Marker
        DrawCircleV(pos, CENTER_RADIUS + 2, BLACK);
        DrawCircleV(pos, CENTER_RADIUS, WHITE);
        
        // The Center always draws its own "Belief" color and shape 
        // to act as a legend for the shapes
        Color c = km_centers[k].color;
        
        // In Random mode, the center icon stays white/black or uses a neutral color 
        // to avoid confusing the "Truth" colors of the points. 
        // But to visualize which shape maps to which ID, we can keep the color or make it Gray.
        // Let's keep it colored so the user knows "Red Center = Circle Belief".
        
        switch (km_centers[k].shape) {
            case SHAPE_CIRCLE: DrawCircleV(pos, 4, c); break;
            case SHAPE_SQUARE: DrawRectangle(pos.x-4, pos.y-4, 8, 8, c); break;
            case SHAPE_TRIANGLE: DrawTriangle((Vector2){pos.x, pos.y-5}, (Vector2){pos.x-5, pos.y+5}, (Vector2){pos.x+5, pos.y+5}, c); break;
            default: DrawCircleV(pos, 3, c); break;
        }
    }
}

void draw_ui_panel() {
    const char* text = TextFormat("Iteration: %d", iteration_count);
    const char* mode = (current_source == SOURCE_RANDOM) ? "Mode: RANDOM (Color=Truth)" : "Mode: CSV (Color=Belief)";
    const char* subtext = "SPACE: Step | R: Reset | L: Load CSV";
    
    int fontSize = 20;
    int padding = 10;
    int w = MeasureText(subtext, fontSize) + padding * 2;
    int h = fontSize * 3 + padding * 4;
    int x = 10;
    int y = 10;

    DrawRectangle(x, y, w, h, COLOR_UI_BG);
    DrawRectangleLines(x, y, w, h, COLOR_UI_BORDER);
    
    DrawText(text, x + padding, y + padding, fontSize, WHITE);
    DrawText(mode, x + padding, y + padding * 2 + 5, 10, GREEN);
    DrawText(subtext, x + padding, y + padding * 3 + fontSize, 10, LIGHTGRAY);
}

// --- Math Helpers ---

Vector2 project_to_screen(Vector2 v) {
    float lx = MAX_X - MIN_X;
    float ly = MAX_Y - MIN_Y;
    return (Vector2){
        .x = GetScreenWidth() * (v.x - MIN_X) / lx,
        .y = GetScreenHeight() * (1 - (v.y - MIN_Y) / ly)
    };
}

float rand_float(float min, float max) {
    return ((float)rand() / (float)RAND_MAX) * (max - min) + min;
}

Vector2 generate_gaussian_point(Vector2 mean, float std) {
    float u1 = rand_float(0.0001f, 1.0f);
    float u2 = rand_float(0.0001f, 1.0f);
    float z0 = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * PI * u2);
    float z1 = sqrtf(-2.0f * logf(u1)) * sinf(2.0f * PI * u2);
    float x = mean.x + std * z0;
    float y = mean.y + std * z1;
    if(x > MAX_X) x = MAX_X; if(x < MIN_X) x = MIN_X;
    if(y > MAX_Y) y = MAX_Y; if(y < MIN_Y) y = MIN_Y;
    return (Vector2){x, y};
}