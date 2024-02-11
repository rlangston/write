#define CTRL(x) ((x) & 0x1f)

#define MAX_FILENAME_LENGTH 255
#define MAX_COMMAND_LENGTH 255

// Options
int o_tabsize;
int o_messagecooldown;
bool o_show_linenumbers;

// Colours
#define COL_WHITEBLUE 1
#define COL_GREENBLACK 2
#define COL_BLACKWHITE 3
#define COL_BLUEWHITE 4

// Line structure (a double linked list)
typedef struct Line {
	int length;
	struct Line *prev;
	struct Line *next;
	char *text;
} Line;

typedef struct Select_mark {
	Line *line;
	int x;
	int y;
} Select_mark;

typedef struct Undo_mark {
	Line *line;
	int x;
} Undo_mark;

typedef struct buffer {
	char *filename;
	Line *first_line;
	Line *current_line;
	Line *first_screen_line;
	int lines;
	int cx;
	int cy;
	int margin_left;
	int offsetx;
	int offsety;
	bool modified;
	Select_mark select_mark;
	struct buffer *next;
} buffer;

// Functions
void move_right();
void move_left();
void move_up();
void move_down();
void move_word_right();
void move_word_left();
void move_page_down();
void move_page_up();
void move_end();
void move_home();
void move_file_home();
void move_file_end();

int cxtodx(Line *line, int cx);
int dxtocx(Line *line, int dx);

void check_boundx();
void goto_line(int line);
void move_lines_up(int count);
void move_lines_down(int count);

void mark(buffer *b);
void clear_mark(buffer *b);
void copy_line();
void cut_line();
void copy();
void paste();
void get_select_extents(buffer *b, Select_mark *start, Select_mark *end);

bool find(char *find_string, Line *start_line, int start_x);
void delete_line(Line *line);
void delete_lines(Line *start_line);

bool get_input(char *prompt, char *placeholder, char *response, size_t max_length);
bool open_file(char *open_filename);
void save_file(char *save_filename);
void new_file(char *new_filename);
void init();
void shutdown();
void close(buffer *close_buffer);
void prompt_save();
void load_options();
void resize_window();
bool run_command();

void draw_screen();
void refresh_screen();
void update_status();
void message(char *msg);
void draw_line(int y, Line *line);
void toggle_linenumbers();

void insert_string(Line *line, int pos, char *src, int length);
void allocate_string(Line *line, int length);
Line *insert_line(Line *prev, Line *next, char *src, size_t length);
void insert_char(Line *line, int position, char c);
void enter();
void backspace();
void delete();

buffer *add_buffer();
buffer *add_sbuffer();