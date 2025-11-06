// TODO: 
// - highlight children on hover
//  - store multiple highlighted nodes and edges ids in graph state
//  - on every frame do not free the highlighted arrays, just clear them and repopulate incresing their size if needed

#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"
#include "raymath.h"

#include "sqlite3.h"

#define FILELIB_IMPLEMENTATION
#include "file.h"
#include <time.h>

// window props
#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 1200

#define VIEWPORT_MARGIN 50

// graph props
#define NODE_COUNT 2000
#define EDGE_COUNT NODE_COUNT * 2
#define NODE_MAX_CHILDREN 3

// elements geometry
#define NODE_RADIUS 10
#define ARROWHEAD_LENGTH NODE_RADIUS
#define ARROWHEAD_ANGLE 20
#define ARROWHEAD_WINGSPREAD NODE_RADIUS

// elements colors
#define ELEMENT_COLOR GRAY
#define ELEMENT_COLOR_HOVER LIGHTGRAY

static Vector2 PAN = {0.0f, 0.0f};
static float ZOOM = 1.0f;

typedef struct {
    bool triggered;
    int node_id;
} StateDragging;

typedef struct {
    int hovered_node_id;
    bool panning;
    float zoom_delta;
    StateDragging dragging;
} State;

State frame_init_state(State *state) {
    if (!state) {
        return (State){-1, false, 0., {false, -1}};
    }

    return (State){-1, state->panning, 0., state->dragging};
};

typedef struct Node {
    int id;
    Vector2 pos;
} Node;

typedef struct {
    int id;

    Node* from;
    Node* to;
} Edge;

typedef struct {
    Node* nodes;
    Edge* edges;

    int node_count;
    int edge_count;
} Graph;

// create a random graph with given number of nodes and edges
Graph create_random_graph(int node_count, int edge_count) {
    Graph graph;

    graph.node_count = node_count;
    graph.edge_count = edge_count;

    graph.nodes = malloc(sizeof(Node) * node_count);
    graph.edges = malloc(sizeof(Edge) * edge_count);

    // create random nodes
    for (int i = 0; i < node_count; i++) {
        graph.nodes[i].id = i;
        graph.nodes[i].pos.x = VIEWPORT_MARGIN + rand() % (WINDOW_WIDTH - 2*VIEWPORT_MARGIN);
        graph.nodes[i].pos.y = VIEWPORT_MARGIN + rand() % (WINDOW_HEIGHT - 2*VIEWPORT_MARGIN);
    }

    // create random edges
    for (int i = 0; i < edge_count; i++) {
        int start_idx = rand() % node_count;
        int end_idx = rand() % node_count;
        graph.edges[i].id = i;
        graph.edges[i].from = &graph.nodes[start_idx];
        graph.edges[i].to = &graph.nodes[end_idx];
    }

    return graph;
}

Vector2 get_screen_position(Vector2 pos) {
    return Vector2Scale(Vector2Add(pos, PAN), ZOOM);
}

Vector2 get_graph_position(Vector2 pos) {
    return Vector2Subtract(Vector2Scale(pos, 1.0f / ZOOM), PAN);
}

void draw_node(Node *node, Color color) {
    Vector2 pos = get_screen_position(node->pos);
    float radius = NODE_RADIUS * ZOOM;
    DrawCircle(pos.x, pos.y, radius, color);
}

void draw_edge(Edge *edge, Color color) {
    Vector2 pos_from = get_screen_position(edge->from->pos); Vector2 pos_to = get_screen_position(edge->to->pos);

    // compute dir vector
    Vector2 dir_abs = Vector2Subtract(pos_to, pos_from);
    Vector2 dir = Vector2Normalize(dir_abs);

    // compute arrowhead points
    float radius = NODE_RADIUS * ZOOM;
    float arrowhead_length = ARROWHEAD_LENGTH * ZOOM;
    Vector2 arrowhead_point = Vector2Subtract(pos_to, Vector2Scale(dir, radius));
    Vector2 left_wing_dir = Vector2Rotate(dir, (ARROWHEAD_ANGLE +180) * DEG2RAD);
    Vector2 right_wing_dir = Vector2Rotate(dir, -(ARROWHEAD_ANGLE + 180) * DEG2RAD);
    Vector2 left_wing_point = Vector2Add(arrowhead_point, Vector2Scale(left_wing_dir,arrowhead_length));
    Vector2 right_wing_point = Vector2Add(arrowhead_point, Vector2Scale(right_wing_dir, arrowhead_length));

    DrawTriangle(right_wing_point, arrowhead_point, left_wing_point, color); 
    DrawLine(pos_from.x, pos_from.y, arrowhead_point.x, arrowhead_point.y, color);
}

void draw_graph(Graph *graph, State *state) {
    for (int i = 0; i < graph->edge_count; i++) {
        Edge *edge = &graph->edges[i];
        draw_edge(edge, ELEMENT_COLOR);
    }

    for (int i = 0; i < graph->node_count; i++) {
        if ((state->dragging.triggered && i == state->dragging.node_id ) || (i == state->hovered_node_id)) {
            // skip dragged or hovered node to draw it last
            continue;
        }

        draw_node(&graph->nodes[i], ELEMENT_COLOR);
    }

    if (state->dragging.triggered) {
        draw_node(&graph->nodes[state->dragging.node_id], ELEMENT_COLOR_HOVER);
    }

    if (state->hovered_node_id != -1) {
        draw_node(&graph->nodes[state->hovered_node_id], ELEMENT_COLOR_HOVER);
    }
}

bool is_inside_node(Node *node, int x, int y) {
    float radius = NODE_RADIUS * ZOOM;
    Vector2 pos = get_screen_position(node->pos);
    float dx = pos.x - x;
    float dy = pos.y - y;
    return (dx * dx + dy * dy) <= (radius * radius);
}

void draw_fps(int posX, int posY, Color color) {
    int fps = GetFPS();
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d FPS", fps);
    DrawText(buffer, posX, posY, 20, color);
}

void compute_state(Graph *g, State *state) {
    *state = frame_init_state(state);

    if (state->panning) {
        if (IsMouseButtonUp(MOUSE_BUTTON_MIDDLE)) {
            state->panning = false;
        }
        return;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        state->panning = true;
        return;
    }

    Vector2 zoom_delta_vec = GetMouseWheelMoveV();
    state->zoom_delta -= zoom_delta_vec.x;
    state->zoom_delta += zoom_delta_vec.y;

    if (state->dragging.triggered) {
        if (IsMouseButtonUp(MOUSE_BUTTON_LEFT) && state->dragging.triggered) {
            state->dragging = (StateDragging){false, -1};
        }
        return;
    }

    for (int i = 0; i < g->node_count; i++) {
        if (is_inside_node(&g->nodes[i], GetMouseX(), GetMouseY())) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                state->dragging = (StateDragging){true, i};
                return;
            }

            state->hovered_node_id = i;
            return;
        }
    }
}

void draw_cursor(State *state) {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (state->hovered_node_id != -1) {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    }
    if (state->dragging.triggered) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
    }
}

void apply_forces(Graph *g, State *state) {
    for (int i = 0; i < g->node_count; i++) {
        // skipp updating dragged node
        if (state->dragging.triggered && i == state->dragging.node_id) {
            continue;
        }

        Node *node = &g->nodes[i];
        Vector2 total_force = {0, 0};

        // repulsion from other nodes
        for (int j = 0; j < g->node_count; j++) {
            if (i == j) {
                continue;
            }

            Node *other_node = &g->nodes[j];
            Vector2 dir = Vector2Subtract(node->pos, other_node->pos);
            float distance = Vector2Length(dir);
            if (distance < 1.0f) {
                distance = 1.0f; // prevent division by zero
            }
            Vector2 force_dir = Vector2Normalize(dir);
            float force_magnitude = 100000.0f / (distance * distance); // inverse square law
            Vector2 force = Vector2Scale(force_dir, force_magnitude);
            total_force = Vector2Add(total_force, force);
        }

        // attraction to connected nodes
        for (int j = 0; j < g->edge_count; j++) {
            Edge *edge = &g->edges[j];
            if (edge->from->id != node->id && edge->to->id != node->id) {continue;}

            Node *other_node = (edge->from->id == node->id) ? edge->to : edge->from;
            Vector2 dir = Vector2Subtract(other_node->pos, node->pos);
            float distance = Vector2Length(dir);
            Vector2 force_dir = Vector2Normalize(dir);
            float force_magnitude = distance * 0.1f; // linear attraction
            Vector2 force = Vector2Scale(force_dir, force_magnitude);
            total_force = Vector2Add(total_force, force);
        }

        node->pos = Vector2Add(node->pos, Vector2Scale(total_force, GetFrameTime()));
    }
}
 // how many edges allocated
void update_graph(Graph *g, State *state) {
    apply_forces(g, state);

    // handle pan
    if (state->panning) {
        Vector2 pan_vector = (Vector2){GetMouseDelta().x, GetMouseDelta().y};
        PAN = Vector2Add(PAN, Vector2Scale(pan_vector, 1.0f / ZOOM));
    }

    // handle zoom
    if (state->zoom_delta != 0.0f) {
        ZOOM += state->zoom_delta;
    }

    // update dragged node position
    if (state->dragging.triggered) {
        Vector2 cursor_pos = (Vector2){GetMouseX(), GetMouseY()};
        Vector2 graph_cursor_pos = get_graph_position(cursor_pos);

        Node *dragged_node = &g->nodes[state->dragging.node_id];
        dragged_node->pos = graph_cursor_pos;
    }
}

int main() {
    /*printf("Using sqlite3 version: %s\n", sqlite3_libversion());

    printf("Initializing sqlite3...\n");
    int init_result;
    if ((init_result = sqlite3_initialize()) != SQLITE_OK) {
        printf("Failed to initialize SQLite: %s\n", sqlite3_errstr(init_result));
        return 1;
    }
    printf("sqlite3 initialized successfully.\n");

    printf("Shutting down sqlite3...\n");
    int shutdown_result;
    if ((shutdown_result = sqlite3_shutdown()) != SQLITE_OK) {
        printf("Failed to shut down SQLite: %s\n", sqlite3_errstr(shutdown_result));
        return 1;
    }
    printf("sqlite3 shut down successfully.\n");

    printf("Opening database connection...\n");
    sqlite3 *conn;
    int open_result;
    if ((open_result = sqlite3_open("db", &conn)) != SQLITE_OK) {
        printf("Failed to open database: %s\n", sqlite3_errstr(open_result));
        return 1;
    }
    printf("Database connection opened successfully.\n");

    char *migration_sql = read_file("migrations/init.sql");
    if (migration_sql == NULL) {
        printf("Failed to read migration file.\n");
        sqlite3_close(conn);
        return 1;
    }

    printf("Executing SQL statement: %s\n", migration_sql);
    char *err_msg = 0;
    int exec_result;
    if ((exec_result = sqlite3_exec(conn, migration_sql, 0, 0, &err_msg)) != SQLITE_OK) {
        printf("Failed to execute SQL statement: %s\n", err_msg);
        sqlite3_free(err_msg);
        free(migration_sql);
        sqlite3_close(conn);
        return 1;
    }
    printf("SQL statement executed successfully.\n");

    free(migration_sql);

    printf("Closing database connection...\n");
    int close_result;
    if ((close_result = sqlite3_close(conn)) != SQLITE_OK) {
        printf("Failed to close database connection: %s\n", sqlite3_errstr(close_result));
        return 1;
    }
    printf("Database connection closed successfully.\n");
    */

    ClearWindowState(FLAG_VSYNC_HINT);

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "graphs");

    // remove any fps limitations
    SetTargetFPS(0);

    // seed random
    srand((unsigned)time(NULL));

    Graph g = create_random_graph(NODE_COUNT, EDGE_COUNT);
    State state = frame_init_state(NULL);

    while (!WindowShouldClose()) {
        BeginDrawing();

        ClearBackground(DARKGRAY);

        compute_state(&g, &state);

        draw_graph(&g, &state);
        draw_cursor(&state);

        update_graph(&g, &state);

        draw_fps(5, 5, ELEMENT_COLOR_HOVER);

        EndDrawing();
    }

    free(g.nodes);
    free(g.edges);

    return 0;
}

