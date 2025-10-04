#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/bfile.h>
#include <stdio.h>
#include <stdlib.h> // For malloc, realloc, free, qsort, bsearch
#include <string.h> // For memcpy
#include <ctype.h>

typedef struct {
    long x;
    long y;
} Point;

int CELL_SIZE = 4;

Point* live_cells = NULL;
int cell_count = 0;
int capacity = 0;

long cursor_x = 10, cursor_y = 10;
long offset_x = 0, offset_y = 0;

int paused = 1; // Start in Pause mode
int mode = 0; // 0 = EDIT, 1 = MOVE
int speed = 1;
int dark_mode = 0;
int lowspeed = 0;

// Colors
int BG_COLOR = C_WHITE;
int CELL_COLOR = C_BLACK;
int CURSOR_COLOR = C_RED;
int TEXT_COLOR = C_BLACK;


void* bsearch(const void* key, const void* base, size_t nitems, size_t size, int (*compar)(const void*, const void*)) {
    char* p = (char*)base;
    size_t low = 0;
    size_t high = nitems;

    while (low < high) {
        size_t mid = low + (high - low) / 2;
        void* mid_ptr = p + mid * size;
        int cmp = compar(key, mid_ptr);

        if (cmp < 0) {
            high = mid;
        } else if (cmp > 0) {
            low = mid + 1;
        } else {
            return mid_ptr; // Find !
        }
    }
    return NULL; // Not find
}

// Fonction de comparaison pour qsort et bsearch. Trie par Y puis par X.
int compare_points(const void* a, const void* b) {
    Point* p1 = (Point*)a;
    Point* p2 = (Point*)b;
    if (p1->y < p2->y) return -1;
    if (p1->y > p2->y) return 1;
    if (p1->x < p2->x) return -1;
    if (p1->x > p2->x) return 1;
    return 0;
}

// Check if a cell is alive by using a binary resarch (fast)
int is_alive(long x, long y) {
    Point key = {x, y};
    return bsearch(&key, live_cells, cell_count, sizeof(Point), compare_points) != NULL;
}

// Add a new cell, and maintain the sorting
void add_cell(long x, long y) {
    if (is_alive(x, y)) return; // Alive

    if (cell_count >= capacity) {
        capacity = (capacity == 0) ? 16 : capacity * 2; // Double the capacity
        live_cells = realloc(live_cells, capacity * sizeof(Point));
    }

    Point new_cell = {x, y};
    // Find or insert the new cell
    int i = 0;
    while (i < cell_count && compare_points(&live_cells[i], &new_cell) < 0) {
        i++;
    }

    // Move items
    memmove(&live_cells[i + 1], &live_cells[i], (cell_count - i) * sizeof(Point));
    live_cells[i] = new_cell;
    cell_count++;
}

// Delete a cell in the list
void remove_cell(long x, long y) {
    Point key = {x, y};
    Point* found = bsearch(&key, live_cells, cell_count, sizeof(Point), compare_points);
    if (!found) return; // Don't find

    int index = found - live_cells;
    // Move items to erase the deleted cell
    memmove(&live_cells[index], &live_cells[index + 1], (cell_count - index - 1) * sizeof(Point));
    cell_count--;
}

// Can cause Crash, I don't know why
int save_grid(uint16_t *path) {

    // Test if the file exist
    int f = BFile_Open(path, BFile_ReadOnly);
    if(f > 0) {
        // If file exist, we close it
        BFile_Close(f);
    } else {
        BFile_Create(path, BFile_File, 0);
    }

    // Open the file
    f = BFile_Open(path, BFile_ReadWrite);
    if(f < 0) return f;

    // Write the number of cells (alive)
    int bytes = sizeof(int);
    if(bytes % 2 != 0) bytes++;
    BFile_Write(f, &cell_count, bytes);

    // Write the cells
    int total_size = cell_count * sizeof(Point);
    if(total_size % 2 != 0) total_size++;
    BFile_Write(f, live_cells, total_size);

    BFile_Close(f);

    return 0; // Return 0
}


int load_grid(uint16_t *path) {
    // Open the file
    int f = BFile_Open(path, BFile_ReadOnly);
    if(f < 0) return f; // Error : not found, (in the most case)

    // Load the number of cells
    int count = 0;
    int bytes = sizeof(int);
    if(bytes % 2 != 0) bytes++;
    int res = BFile_Read(f, &count, bytes, -1);
    if(res < 0) {
        BFile_Close(f);
        return res;
    }

    // Prepare memory
    Point *cells = malloc(count * sizeof(Point));
    if(!cells) {
        BFile_Close(f);
        return -1; // Memory error
    }

    // Read alive cells.
    int total_size = count * sizeof(Point);
    if(total_size % 2 != 0) total_size++;
    res = BFile_Read(f, cells, total_size, -1);
    if(res < 0) {
        free(cells);
        BFile_Close(f);
        return res;
    }

    // Close the file
    BFile_Close(f);

    // Replace the old grid
    if(live_cells) free(live_cells);
    live_cells = cells;
    cell_count = count;
    capacity = count;

    return 0; // OK
}

void add_rle_cell(int start_x, int start_y, const char* rle) {
    int x = start_x;
    int y = start_y;
    int count = 0;

    for (const char* p = rle; *p; p++) {
        if (isdigit(*p)) {
            count = count * 10 + (*p - '0');
        } else if (*p == 'b') {
            if (count == 0) count = 1;
            x += count;
            count = 0;
        } else if (*p == 'o') {
            if (count == 0) count = 1;
            for (int i = 0; i < count; i++) {
                add_cell(x + i, y);
            }
            x += count;
            count = 0;
        } else if (*p == '$') {
            if (count == 0) count = 1;
            y += count;
            x = start_x;
            count = 0;
        } else if (*p == '!') {
            break;
        }
    }
}


void init_grid() {
    // Clear the old grid
    if(live_cells) free(live_cells);
    live_cells = NULL;
    cell_count = 0;
    capacity = 0;
    cursor_x = 5;
    cursor_y = 5;
    offset_x = 0;
    offset_y = 0;
}

void update_grid() {
    Point* candidates = NULL;
    int cand_count = 0;
    int cand_capacity = 0;

    // 1. Prepare alive cells : chaque cellule vivante et ses 8 voisins
    for (int i = 0; i < cell_count; i++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (cand_count >= cand_capacity) {
                    cand_capacity = (cand_capacity == 0) ? 256 : cand_capacity * 2;
                    candidates = realloc(candidates, cand_capacity * sizeof(Point));
                }
                candidates[cand_count++] = (Point){live_cells[i].x + dx, live_cells[i].y + dy};
            }
        }
    }

    // 2. Sort candidates
    qsort(candidates, cand_count, sizeof(Point), compare_points);

    Point* next_gen = NULL;
    int next_gen_count = 0;
    int next_gen_capacity = 0;

    // 3. Loop on the sorted candidates to count his neighbors and apply the rules
    int i = 0;
    while (i < cand_count) {
        Point current_cand = candidates[i];
        int neighbors = 0;
        int j = i;
        while (j < cand_count && compare_points(&candidates[j], &current_cand) == 0) {
            neighbors++;
            j++;
        }

        int was_alive = is_alive(current_cand.x, current_cand.y);

        if (was_alive) neighbors--; 

        // Apply the rules of The Game of life
        if ((neighbors == 3) || (was_alive && neighbors == 2)) {
            if (next_gen_count >= next_gen_capacity) {
                next_gen_capacity = (next_gen_capacity == 0) ? cell_count : next_gen_capacity * 2;
                next_gen = realloc(next_gen, next_gen_capacity * sizeof(Point));
            }
            next_gen[next_gen_count++] = current_cand;
        }
        i = j;
    }

    // 4. Replace the old generation with the new one.
    free(live_cells);
    free(candidates);
    live_cells = next_gen;
    cell_count = next_gen_count;
    capacity = next_gen_capacity;
}

void draw_grid() {
    dclear(BG_COLOR);

    // Only alive cells
    for (int i = 0; i < cell_count; i++) {
        long px = (live_cells[i].x - offset_x) * CELL_SIZE;
        long py = (live_cells[i].y - offset_y) * CELL_SIZE;

        // Only draw things that we see
        if (px + CELL_SIZE > 0 && px < 396 && py + CELL_SIZE > 0 && py < 224) {
            drect(px, py, px + CELL_SIZE - 1, py + CELL_SIZE - 1, CELL_COLOR);
        }
    }

    // EDIT mode cursor
    if (paused && mode == 0) {
        long px = (cursor_x - offset_x) * CELL_SIZE;
        long py = (cursor_y - offset_y) * CELL_SIZE;
        drect(px, py, px + CELL_SIZE - 1, py + CELL_SIZE - 1, CURSOR_COLOR);
        if(CELL_SIZE > 2) {
             drect(px + 1, py + 1, px + CELL_SIZE - 2, py + CELL_SIZE - 2, is_alive(cursor_x, cursor_y) ? CELL_COLOR : BG_COLOR);
        }
    }
    
    // Infos in the bottom
    char info[128];
    char speed_info[16];
    if (lowspeed==0){ sprintf(speed_info, "x%d", speed); }
    else{
        if (lowspeed==1){ sprintf(speed_info, "x0.5"); }
        else if (lowspeed==10){ sprintf(speed_info, "x0.1"); }
        else if (lowspeed==20){ sprintf(speed_info, "x0.05"); }
    }
    sprintf(info, "%s | Speed: %s | Zoom: %d | Mode: %s | Cells: %d ",
            paused?"PAUSE":"RUN", speed_info, CELL_SIZE, mode==0?"EDIT":"MOVE", cell_count);
    dtext_opt(0, 214, TEXT_COLOR, BG_COLOR, 0, 0, info, -1);

    dupdate();
}

// List 4 files
static const uint16_t *save_files[4] = {
    u"\\\\fls0\\grid1.gol1",
    u"\\\\fls0\\grid2.gol1",
    u"\\\\fls0\\grid3.gol1",
    u"\\\\fls0\\grid4.gol1"
};

// Menu for select a slot
int select_slot(const char *title) {
    int choice = 0;
    while(1) {
        dclear(BG_COLOR);
        dtext(10, 10, CELL_COLOR, title);
        for(int i = 0; i < 4; i++) {
            if(i == choice)
                dprint(10, 30 + i*10, CELL_COLOR, "> Slot %d", i+1);
            else
                dprint(10, 30 + i*10, CELL_COLOR, "  Slot %d", i+1);
        }
        dupdate();

        int key = getkey().key;
        if(key == KEY_UP)    choice = (choice+3)%4;
        if(key == KEY_DOWN)  choice = (choice+1)%4;
        if(key == KEY_EXE)   return choice;
        if(key == KEY_EXIT)  return -1;
    }
}

void save_menu() {
    int slot = select_slot("Save :");
    if(slot < 0) return;

    int err = save_grid((uint16_t*)save_files[slot]);
    dclear(BG_COLOR);
    if(err == 0)
        dtext(10, 30, CELL_COLOR, "Save OK !");
    else {
        char buf[32];
        sprintf(buf, "Error: %d", err);
        dtext(10, 30, CELL_COLOR, buf);
    }
    dupdate();
    getkey();
}

void load_menu() {
    int slot = select_slot("Charger :");
    if(slot < 0) return;

    int err = load_grid((uint16_t*)save_files[slot]);
    dclear(BG_COLOR);
    if(err == 0)
        dtext(10, 30, CELL_COLOR, "Load OK !");
    else
        dtext(10, 30, CELL_COLOR, "Load error (is the file missing ?) !");
    dupdate();
    getkey();
}

// [MENU] to help open menu
void help_menu() {
    dclear(BG_COLOR);
    dtext(10, 10, CELL_COLOR, "Help:");
    dtext(10, 30, CELL_COLOR, "F1: Reset");
    dtext(10, 40, CELL_COLOR, "F2: Save");
    dtext(10, 50, CELL_COLOR, "F3: Load");
    dtext(10, 60, CELL_COLOR, "F4: Random Soup");
    dtext(10, 70, CELL_COLOR, "F5: Spaceship");
    dtext(10, 80, CELL_COLOR, "F6: Gosper Glider Gun");
    dtext(10, 100, CELL_COLOR, "Shift: Pause/Run");
    dtext(10, 110, CELL_COLOR, "Alpha: Edit/Move Mode");
    dtext(10, 120, CELL_COLOR, "Optn: Toggle Dark Mode");
    dtext(10, 130, CELL_COLOR, "+/-: Zoom In/Out");
    dtext(10, 140, CELL_COLOR, "*: Increase Simultation Speed");
    dtext(10, 150, CELL_COLOR, "Div: Decrease Simultation Speed");
    dtext(10, 170, CELL_COLOR, "In Edit Mode:");
    dtext(10, 180, CELL_COLOR, "Arrow Keys: Move Cursor");
    dtext(10, 190, CELL_COLOR, "EXE: Toggle Cell State in Edit Mode");
    dtext(10, 210, CELL_COLOR, "Press any key to return...");
    dupdate();
    getkey();
}


void update_colors() {
    if(dark_mode) { BG_COLOR=C_BLACK; CELL_COLOR=C_WHITE; CURSOR_COLOR=C_RED; TEXT_COLOR=C_WHITE; }
    else { BG_COLOR=C_WHITE; CELL_COLOR=C_BLACK; CURSOR_COLOR=C_RED; TEXT_COLOR=C_BLACK; }
}

// --- MAIN ---
int main(void) {
    init_grid();
    update_colors();
    int timeout = 20;
    int skip_frames = 0;

    while (1) {
        draw_grid();
        key_event_t key = getkey_opt(GETKEY_REP_ARROWS, &timeout);

        if (key.type == KEYEV_NONE) {
            if (!paused) {
                if (lowspeed != 0) {
                    if (++skip_frames >= lowspeed) {
                        skip_frames = 0;
                        for (int i=0; i<speed; i++) update_grid();
                    }
                } else {
                    for (int i=0; i<speed; i++) update_grid();
                }
            }
            timeout = 20;
            continue;
        }

        if (key.key == KEY_EXIT) break;
        if (key.key == KEY_F1) init_grid(); // F1 to reset grid
        if (key.key == KEY_SHIFT) paused = !paused;
        if (key.key == KEY_ALPHA) mode = 1 - mode;
        if (key.key == KEY_OPTN) { dark_mode = 1 - dark_mode; update_colors(); }

        if (key.key == KEY_ADD && CELL_SIZE < 20) CELL_SIZE++;
        if (key.key == KEY_SUB && CELL_SIZE > 1) CELL_SIZE--;
        
        // Speed
        if (key.key==KEY_MUL){ if(lowspeed>0){ if(lowspeed==1) lowspeed=0; else if(lowspeed==10) lowspeed=1; else if(lowspeed==20) lowspeed=10; } else if(lowspeed==0) speed++; }
        if(key.key==KEY_DIV){ if(speed>1) speed--; else if(speed==1){ if(lowspeed==0) lowspeed=1; else if(lowspeed==1) lowspeed=10; else if(lowspeed==10) lowspeed=20; } }

        // Save and load (problems with save)
        if (key.key == KEY_F2) save_menu();
        if (key.key == KEY_F3) load_menu();

        if (key.key == KEY_F4){
            // Create a random soup of 100x100 cells
            for(int i=0; i<100; i++) {
                long rx = (rand() % 100) - 50; // -50 to +49
                long ry = (rand() % 100) - 50; // -50 to +49
                add_cell(cursor_x + rx, cursor_y + ry);
            }

        }

        if (key.key == KEY_F5) add_rle_cell(cursor_x, cursor_y, "21bo3b$18b4o3b$13bo2bob2o5b$13bo11b$4o8bo3bob2o5b$o3bo5b2ob2obobob5o$o9b2obobobo2b5o$bo2bo2b2o2bo3b3o2bob2ob$6bo2bob2o12b$6bo4b2o12b$6bo2bob2o12b$bo2bo2b2o2bo3b3o2bob2ob$o9b2obobobo2b5o$o3bo5b2ob2obobob5o$4o8bo3bob2o5b$13bo11b$13bo2bob2o5b$18b4o3b$21bo!");
        if (key.key == KEY_F6) add_rle_cell(cursor_x, cursor_y, "24bo11b$22bobo11b$12b2o6b2o12b2o$11bo3bo4b2o12b2o$2o8bo5bo3b2o14b$2o8bo3bob2o4bobo11b$10bo5bo7bo11b$11bo3bo20b$12b2o!"); //Gosper_glider_gun

        if (key.key == KEY_MENU) help_menu(); // Help


        // Move
        if (mode == 0 && paused) { // EDIT
            if (key.key == KEY_UP) cursor_y--;
            if (key.key == KEY_DOWN) cursor_y++;
            if (key.key == KEY_LEFT) cursor_x--;
            if (key.key == KEY_RIGHT) cursor_x++;
            if (key.key == KEY_EXE) {
                if(is_alive(cursor_x, cursor_y)) remove_cell(cursor_x, cursor_y);
                else add_cell(cursor_x, cursor_y);
            }
        } else { // MOVE
            if (key.key == KEY_UP) offset_y--;
            if (key.key == KEY_DOWN) offset_y++;
            if (key.key == KEY_LEFT) offset_x--;
            if (key.key == KEY_RIGHT) offset_x++;
        }
    }
    
    free(live_cells); // Free memory
    return 0;
}
