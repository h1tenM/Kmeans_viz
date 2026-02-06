# K-Means Visualizer

An interactive sandbox for exploring the **K-Means clustering algorithm**. This tool visualizes how centroids converge on synthetic Gaussian data or custom datasets provided via CSV.

---

## Directory Structure

The project is organized to separate source code, dependencies, and build artifacts.

.
├── Makefile # Build instructions for the project
├── assets/ # Storage for data.csv and other resources
├── bin/ # Compiled executable output
├── build/ # Intermediate object files
├── include/ # Header files (raylib.h, raymath.h)
├── lib/ # Static libraries (libraylib.a)
└── src/ # Source code (main.c)

---

## Implementation and Semantics

The visualizer distinguishes between the mathematical reality of the data and the algorithm's interpretation of it.

### Truth vs Belief Philosophy

When generating random clusters, the program tracks which cluster a point actually belongs to.

- **Color (Truth):** Original cluster index that generated the point
- **Shape (Belief):** Cluster currently assigned by K-Means

If the algorithm converges correctly, all points of the same color will eventually share the same shape.

---

## Technical Details

- Synthetic clusters are generated using the **Box–Muller transform** to sample from a standard normal distribution.
- The coordinate system runs in a **virtual world space** (default range: `-100` to `100`).
- World coordinates are projected to screen pixels.
- This keeps behavior consistent even when the window is resized.

---

## Using Custom Data

### CSV Format

To load your own dataset:

- Place a file named `data.csv` inside the `assets/` folder.
- Each line should contain numeric coordinates separated by a comma or space.

Example:

12.5, 45.0
-20.1, 10.3

### CSV Mode Behavior

When loading from CSV:

- Ground truth clusters are unknown.
- Both **color** and **shape** are based on the K-Means assignment.

---

## Key Variables and Constants

### `cap` (in `load_from_csv`)

- Controls dynamic memory capacity.
- Starts at **1000 points**.
- Automatically doubles using `realloc` if capacity is exceeded.
- Allows efficient handling of large datasets.

### `NUM_KMEANS_CENTERS`

- Number of centroids initialized by the algorithm.

### `MIN_X / MAX_X`

- Define world coordinate bounds.
- If your CSV contains values like `500` or `-500`, update these constants.
- Otherwise points may be clamped or rendered off-screen.

---

## Build Instructions

From the project root:

```bash
make
```
