#define CTRL(x) ((x) & 0x1f)

#define MAX_FILENAME_LENGTH 255
#define MAX_COMMAND_LENGTH 255
#define TAB_SIZE 4

// Colours
#define COL_WHITEBLUE 1
#define COL_GREENBLACK 2

// Line structure (a double linked list)
typedef struct Line {
	size_t length;
	struct Line *prev;
	struct Line *next;
	char *text;
} Line;

typedef struct buffer {
	char *filename;
	Line *first_line;
	Line *current_line;
	Line *first_screen_line;
	ssize_t lines;
	int cx;
	int cy;
	int margin_left;
	int offsetx;
	int offsety;
	bool modified;
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

void check_boundx();
void goto_line(ssize_t line);
void move_lines_up(int count);
void move_lines_down(int count);

void copy_line();
void cut_line();
void paste_line();
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
void resize_window();

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