#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "write.h"

// Window size
int windowx, windowy;
WINDOW *textscr;
WINDOW *statusscr;
WINDOW *commandscr;

// Current mode
int mode;

char *filename;

// Modes
#define MODE_EDIT 1
#define MODE_COMMAND 2

bool show_linenumbers = false;
int display_cx;

buffer *current_buffer = NULL;
buffer *paste_buffer = NULL;
buffer *message_buffer= NULL;
buffer *first_buffer = NULL;
int message_timer = 0;

// Cut and paste buffer
Line *pastebuffer;

// Last pressed key
int ch;

int main(int argc, char *argv[])
{
	char s[MAX_COMMAND_LENGTH];
	load_options();

	if (argc >= 2)
	{
		if (!open_file(argv[1]))
			new_file(argv[1]);
	}
	else
	{
		new_file("blank.txt");
	}

	first_buffer = current_buffer;

	// Create message buffer
	message_buffer = add_sbuffer();
	message_buffer->first_line = NULL;
	message_buffer->lines = 0;

	message("");
	// char *message = "";
	// message_buffer->first_line = insert_line(NULL, message_buffer->first_line, message, strlen(message) + 1);

	init();
	move_file_home();
	
	while(current_buffer != NULL)
	{
		refresh_screen();
		ch = getch();
		if (ch == CTRL('q'))
			break;
		switch (ch)
		{
			// Movement keys
			case KEY_RIGHT:
				move_right();
				break;
			case KEY_LEFT:
				move_left();
				break;
			case KEY_UP:
				move_up();
				break;
			case KEY_DOWN:
				move_down();
				break;
			case 561: // CTRL-right
				move_word_right();
				break;
			case 546: // CTRL-right
				move_word_left();
				break;
			case KEY_NPAGE:
				move_page_down();
				break;
			case KEY_PPAGE:
				move_page_up();
				break;
			case KEY_END:
				move_end();
				break;
			case KEY_HOME:
				move_home();
				break;
			case 536: // CTRL-HOME
				move_file_home();
				break;
			case 531: // CTRL-END
				move_file_end();
				break;

			case 551: // CTRL-PGDOWN
				if (current_buffer->next != NULL) current_buffer = current_buffer->next;
				else current_buffer = first_buffer;
				break;

			case CTRL('l'): // Line numbers
				toggle_linenumbers();
				break;
			case CTRL('g'): // Goto line
				if (get_input("Goto line ", "", s, 10))
				{
					if (atoi(s) > 0)
						goto_line(atoi(s));
				}
				break;
			case CTRL('f'): // Find
				if (get_input("Search for ", "", s, MAX_COMMAND_LENGTH))
				{
					if (find(s, current_buffer->current_line, current_buffer->cx))
					{
						message("Press ENTER to search again");
						refresh_screen();
						ch = getch();
						while (ch == 10) // While ENTER is pressed, find again
						{
							find(s, current_buffer->current_line, current_buffer->cx);
							message("Press ENTER to search again");
							refresh_screen();
							ch = getch();
						}
						message("");
						ungetch(ch); // Push character back into input buffer if not ENTER
					}

					break;
				}
				break;

			// Saving and loading
			case CTRL('s'): // Save
				prompt_save();
				break;
			case CTRL('o'): // Open
				if (get_input("Load ", "", s, MAX_FILENAME_LENGTH))
				{
					if (!open_file(s))
						new_file(s);
					move_file_home();				
				}
				break;
			case KEY_F(4): // Close
				if (current_buffer->modified) 
					prompt_save();
				close(current_buffer);
				break;
			case CTRL('n'): // New
				new_file("new.txt");
				move_file_home();
				break;

			// Cut and paste
			case CTRL('c'): // Cut
				copy_line();
				break;
			case CTRL('x'): // Cut
				cut_line();
				current_buffer->modified = true;
				break;
			case CTRL('v'): // Paste
				paste_line();
				current_buffer->modified = true;
				break;

			// Command mode
			case 27: // ESC
				if (get_input("? ", "", s, MAX_COMMAND_LENGTH)) run_command(s);
				break;
			case KEY_RESIZE:
				resize_window();
				break;

			// Editing
			case 10: // ENTER
				enter();
				current_buffer->modified = true;
				break;
			case KEY_BACKSPACE:
				backspace();
				current_buffer->modified = true;
				break;
			case KEY_DC:
				delete();
				current_buffer->modified = true;
				break;
			default:
				if ((ch > 27 && ch < 256) || (ch == '\t')) // Ignore control characters
				{
					insert_char(current_buffer->current_line, current_buffer->cx, ch);
					current_buffer->cx++;
					check_boundx();
					current_buffer->modified = true;
				}
				break;
		}
	}

	shutdown();
	return 0;
}

void init()
{
	// Initialise screen and get dimensions
	initscr();
	set_escdelay(1);
	resize_window();

	textscr = subwin(stdscr, windowy, windowx, 0, 0);
	statusscr = subwin(stdscr, 1, windowx, windowy, 0);
	commandscr = subwin(stdscr, 1, windowx, windowy + 1, 0);
	current_buffer->margin_left = 0;
	
	start_color();
	init_pair(COL_WHITEBLUE, COLOR_WHITE, COLOR_BLUE);
	init_pair(COL_GREENBLACK, COLOR_GREEN, COLOR_BLACK);
	wbkgd(statusscr, COLOR_PAIR(COL_WHITEBLUE));

	// Set up ncurses
	raw();
	noecho();
	keypad(stdscr, TRUE);

	// Start cursor at top left
	current_buffer->cx = 0;
	current_buffer->cy = 0;

	// Initialise offsets - first line will be the first one shown on the screen on init
	current_buffer->offsetx = 0;
	current_buffer->offsety = 0;

	mode = MODE_EDIT;

	return;
}

void shutdown()
{
	delete_lines(pastebuffer); // Clear the copy buffer
	// close(message_buffer); // Close the message buffer

	// Close any open buffers
	while (current_buffer != NULL)
	{
		if (current_buffer->modified) 
		{
			draw_screen();
			wrefresh(textscr);
			update_status();
			prompt_save();
		}
		close(current_buffer);
	}

	set_escdelay(1000);
	endwin();
	return;
}

void move_right()
{
	current_buffer->cx++;
	if (current_buffer->cx > current_buffer->current_line->length)
	{
		if (current_buffer->current_line->next == NULL)
			current_buffer->cx = current_buffer->current_line->length;
		else
		{
			current_buffer->current_line = current_buffer->current_line->next;
			current_buffer->cy++;
			current_buffer->cx = 0;
			current_buffer->offsetx = 0;
		}
	}

	check_boundx();

	return;
}

void move_left()
{
	if (current_buffer->cx > 0)
		current_buffer->cx--;
	else if (current_buffer->current_line->prev != NULL)
	{
		current_buffer->current_line = current_buffer->current_line->prev;
		current_buffer->cy--;
		move_end();
	}

	if (current_buffer->cx - current_buffer->offsetx < 0)
		current_buffer->offsetx = current_buffer->cx;

	return;
}

void move_up()
{	
	move_lines_up(1);
	check_boundx();
	return;
}

void move_down()
{
	move_lines_down(1);
	check_boundx();
	return;
}

void move_word_right()
{
	while (current_buffer->cx != current_buffer->current_line->length && current_buffer->current_line->text[current_buffer->cx] != ' ')
		current_buffer->cx++;
	current_buffer->cx++;
	check_boundx();
	return;
}

void move_word_left()
{
	if (current_buffer->cx > 1)
		current_buffer->cx -= 2;
	while (current_buffer->cx != 0 && current_buffer->current_line->text[current_buffer->cx] != ' ')
		current_buffer->cx--;
	if (current_buffer->cx != 0)
		current_buffer->cx++;
	check_boundx();
	return;
}

void move_page_down()
{
	move_lines_down(windowy);
	check_boundx();
	return;
}

void move_page_up()
{
	move_lines_up(windowy);
	check_boundx();
	return;
}

void move_end()
{
	// Force current_buffer->cx beyond the end of the line to reset current_buffer->cx and current_buffer->offsetx
	current_buffer->cx = current_buffer->current_line->length + 1;
	check_boundx();
	return;
}

void move_home()
{
	current_buffer->cx = 0;
	current_buffer->offsetx = 0;
	return;
}

void move_file_home()
{
	current_buffer->current_line = current_buffer->first_line;
	current_buffer->first_screen_line = current_buffer->first_line;
	current_buffer->offsetx = 0;
	current_buffer->offsety = 0;
	current_buffer->cx = 0;
	current_buffer->cy = 0;
	return;
}

void move_file_end()
{
	move_lines_down(current_buffer->lines - current_buffer->cy - current_buffer->offsety);
	move_end();
	return;
}

void check_boundx()
{
	if (current_buffer->cx > current_buffer->current_line->length)
	{
		current_buffer->cx = current_buffer->current_line->length;
		if (current_buffer->current_line->length < windowx - current_buffer->margin_left - 1)
			current_buffer->offsetx = 0;
		else
			current_buffer->offsetx = current_buffer->current_line->length - windowx - current_buffer->margin_left + 1;
	}
	if (current_buffer->cx < 0)
	{
		current_buffer->offsetx = 0;
		current_buffer->cx = 0;
	}
	if (cxtodx(current_buffer->current_line, current_buffer->cx) - current_buffer->offsetx > windowx - current_buffer->margin_left - 1)
		current_buffer->offsetx = current_buffer->cx - windowx - current_buffer->margin_left + 1;

	return;
}

void goto_line(ssize_t line)
{
	if (line > current_buffer->lines) return;
	move_file_home();
	move_lines_down(line - 1);
	return;
}

void move_lines_up(int count)
{
	int d = cxtodx(current_buffer->current_line, current_buffer->cx);
	while (count-- > 0 && current_buffer->current_line->prev != NULL)
	{
		if (current_buffer->cy > 0)
			current_buffer->cy--;
		else
		{
			current_buffer->first_screen_line = current_buffer->first_screen_line->prev;
			current_buffer->offsety -= 1;
		}
		current_buffer->current_line = current_buffer->current_line->prev;
	}
	current_buffer->cx = dxtocx(current_buffer->current_line, d);
	return;
}

void move_lines_down(int count)
{
	int d = cxtodx(current_buffer->current_line, current_buffer->cx);
	while (count-- > 0 && current_buffer->current_line->next != NULL)
	{
		if (current_buffer->cy < windowy - 1)
			current_buffer->cy++;
		else
		{
			current_buffer->first_screen_line = current_buffer->first_screen_line->next;
			current_buffer->offsety += 1;
		}
		current_buffer->current_line = current_buffer->current_line->next;
	}
	current_buffer->cx = dxtocx(current_buffer->current_line, d);
	return;
}

void update_status()
{
	char modified_indicator = ' ';
	if (current_buffer->modified) modified_indicator = '*';
	mvwprintw(statusscr, 0, 0, "%s%c L%d/%lu C%d", current_buffer->filename, modified_indicator, current_buffer->cy + +current_buffer->offsety + 1, current_buffer->lines, current_buffer->cx);
	wclrtoeol(statusscr);

	if (message_timer > 0)
	{
		message_timer--;
		mvwprintw(statusscr, 0, 30, "%s", message_buffer->first_line->text);
	}

	wrefresh(statusscr);
	return;
}

void message(char *msg)
{
	message_buffer->first_line = insert_line(NULL, message_buffer->first_line, msg, strlen(msg) + 1);
	message_timer = o_messagecooldown;
	return;
}

void toggle_linenumbers()
{
	show_linenumbers = !show_linenumbers;
	return;
}

void draw_screen()
{
	werase(textscr);

	// Calculate current_buffer->margin_left
	if (show_linenumbers)
	{
		current_buffer->margin_left = 2;
		int temp_lines = current_buffer->lines;
		while (temp_lines /= 10)
			current_buffer->margin_left += 1;
	}
	else
	{
		current_buffer->margin_left = 0;
	}

	int y = 0;
	Line *line = current_buffer->first_screen_line;
	while (y < windowy && line != NULL)
	{
		draw_line(y, line);
		line = line->next;
		y++;
	}

	return;
}

void refresh_screen()
{
	draw_screen();
	// wmove(stdscr, current_buffer->cy, current_buffer->cx - current_buffer->offsetx + current_buffer->margin_left); // Move cursor to position
	wmove(stdscr, current_buffer->cy, display_cx); // Move cursor to position
	wrefresh(textscr);
	update_status();
	return;
}

void draw_line(int y, Line *line)
{
	// Draw line number
	if (show_linenumbers)
	{
		wmove(textscr, y, 0);
		mvwprintw(textscr, y, 0, "%d", y + current_buffer->offsety + 1);
	}

	wmove(textscr, y, current_buffer->margin_left);
	int x = 0;

	while ((x + current_buffer->offsetx < line->length) && x < windowx - current_buffer->margin_left)
	{
		if (isdigit(line->text[x + current_buffer->offsetx]))
		{
			wattron(textscr, COLOR_PAIR(2));
			waddch(textscr, line->text[x + current_buffer->offsetx]);
			wattroff(textscr, COLOR_PAIR(2));
		}
		else if (line->text[x + current_buffer->offsetx] == '\t')
			for (int i = 0; i < o_tabsize; i++) waddch(textscr, ' ');
		else
			waddch(textscr, line->text[x + current_buffer->offsetx]);
		x++;
	}
	wclrtoeol(textscr);

	if (line == current_buffer->current_line)
		display_cx = cxtodx(line, current_buffer->cx) - current_buffer->offsetx + current_buffer->margin_left;

	return;
}

int cxtodx(Line *line, int cx)
{
	int dx = 0;
	for (int c = 0; c < cx; c++)
		if (line->text[c] == '\t') dx += o_tabsize;
		else dx += 1;
	return dx;
}

int dxtocx(Line *line, int dx)
{
	int cx = 0;
	int c = 0;
	while (c < dx)
	{
		if (line->text[c] == '\t')
			c += o_tabsize;
		else
			c += 1;
		cx++;
	}
	return cx;
}

bool get_input(char *prompt, char *placeholder, char *response, size_t max_length)
{
	// Copy placeholder into response buffer
	strcpy(response, placeholder);

	int icx = strlen(response);

	while (1)
	{
		mvwprintw(commandscr, 0, 0, "%s", prompt);
		wprintw(commandscr, "%s", response);
		wclrtoeol(commandscr);
		wmove(commandscr, 0, icx + strlen(prompt));
		wrefresh(commandscr);

		int c = getch();
		switch (c)
		{
			case KEY_RIGHT:
				if (icx < strlen(response)) icx++;
				break;
			case KEY_LEFT:
				if (icx > 0) icx--;
				break;
			case KEY_END:
				icx = strlen(response) - 1;
				break;
			case KEY_HOME:
				icx = 0;
				break;
			case 27: // ESCAPE
				werase(commandscr);
				wrefresh(commandscr);
				return false;
			case 10: // ENTER
				werase(commandscr);
				wrefresh(commandscr);
				return true;
			case KEY_BACKSPACE:
				if (icx > 0)
				{
					memmove(response + icx - 1, response + icx, strlen(response) - icx + 1);
					icx--;
				}
				break;
			case KEY_DC:
				memmove(response + icx, response + icx + 1, strlen(response) - icx + 1);
				break;
			default:
				if ((icx < max_length -1) && (c > 27 && c < 256))
				{
					memmove(response + icx + 1, response + icx, strlen(response) - icx + 1);
					response[icx] = (char)c;
					icx++;
				}
				break;
		}

	}
}

void insert_char(Line *line, int pos, char c)
{
	insert_string(line, pos, &c, 1);
	return;
}

void insert_string(Line *line, int pos, char *src, int length)
{
	allocate_string(line, line->length + length);
	memmove(line->text + pos + length, line->text + pos, line->length - pos);
	memcpy(line->text + pos, src, length);
	line->length += length;
	return;
}

void allocate_string(Line *line, int length)
{
	char *new_ptr = (char *)realloc(line->text, sizeof(char) * length);
	line->text = new_ptr;
	return;
}

Line *insert_line(Line *prev, Line *next, char *src, size_t length)
{
	Line *line = (Line *) malloc(sizeof(Line));
	line->text = NULL;
	allocate_string(line, length);
	line->length = length;
	memcpy(line->text, src, length);

	line->prev = prev;
	line->next = next;
	if (prev != NULL)
		prev->next = line;
	if (next != NULL)
		next->prev = line;
	return line;
}

void enter()
{
	insert_line(current_buffer->current_line, current_buffer->current_line->next, current_buffer->current_line->text + current_buffer->cx, current_buffer->current_line->length - current_buffer->cx);
	current_buffer->lines++;
	current_buffer->current_line->length = current_buffer->cx;
	allocate_string(current_buffer->current_line, current_buffer->current_line->length + 1);

	move_lines_down(1);
	move_home();

	return;
}

void backspace()
{
	if (current_buffer->cx > 0)
	{
		memmove(current_buffer->current_line->text + current_buffer->cx - 1, current_buffer->current_line->text + current_buffer->cx, current_buffer->current_line->length - current_buffer->cx + 1);
		current_buffer->current_line->length--;
		allocate_string(current_buffer->current_line, current_buffer->current_line->length);
		current_buffer->cx--;
		check_boundx();
	}
	else if (current_buffer->current_line != current_buffer->first_line)
	{
		move_lines_up(1);
		move_end();
		insert_string(current_buffer->current_line, current_buffer->current_line->length, current_buffer->current_line->next->text, current_buffer->current_line->next->length);
		delete_line(current_buffer->current_line->next);
		current_buffer->lines--;
	}
	return;
}

void delete()
{
	if (current_buffer->cx < current_buffer->current_line->length)
	{
		memmove(current_buffer->current_line->text + current_buffer->cx, current_buffer->current_line->text + current_buffer->cx + 1, current_buffer->current_line->length - current_buffer->cx + 1);
		current_buffer->current_line->length--;
		allocate_string(current_buffer->current_line, current_buffer->current_line->length);
	}
	else if (current_buffer->current_line->next != NULL)
	{
		insert_string(current_buffer->current_line, current_buffer->current_line->length, current_buffer->current_line->next->text, current_buffer->current_line->next->length);
		delete_line(current_buffer->current_line->next);
		current_buffer->lines--;
	}
	return;
}

void copy_line()
{
	delete_lines(pastebuffer); // clear the buffer completely
	pastebuffer = insert_line(NULL, NULL, current_buffer->current_line->text, current_buffer->current_line->length);
}

void cut_line()
{
	delete_lines(pastebuffer); // clear the buffer completely
	pastebuffer = insert_line(NULL, NULL, current_buffer->current_line->text, current_buffer->current_line->length);

	if (current_buffer->current_line->next != NULL) // not the last line
	{
		current_buffer->current_line = current_buffer->current_line->next;
		delete_line(current_buffer->current_line->prev);
		current_buffer->lines--;
		move_home();
	}
	else if (current_buffer->current_line->prev != NULL) // last line but not the first line
	{
		current_buffer->current_line = current_buffer->current_line->prev;
		delete_line(current_buffer->current_line->next);
		current_buffer->cy--;
		current_buffer->lines--;
		move_home();
	}
	else // last line and first line ie only line
	{
		delete_line(current_buffer->current_line);
		current_buffer->current_line = insert_line(NULL, NULL, NULL, 0);
		current_buffer->first_line = current_buffer->current_line;
		move_file_home();
	}
	return;
}

void paste_line()
{
	current_buffer->current_line = insert_line(current_buffer->current_line->prev, current_buffer->current_line, pastebuffer->text, pastebuffer->length);
	current_buffer->lines++;
	if (current_buffer->current_line->prev == NULL)
	{
		current_buffer->first_line = current_buffer->current_line;
	}

	if (current_buffer->cy == 0) // If on top row of screen, have just pasted above
	{
		current_buffer->first_screen_line = current_buffer->first_screen_line->prev;
		current_buffer->offsety -= 1;
	}

	move_home();
	return;
}

void delete_line(Line *line)
{
	if (line == current_buffer->first_line) current_buffer->first_line = line->next;
	if (line == current_buffer->first_screen_line) current_buffer->first_screen_line = line->next;

	if (line->prev != NULL)
		line->prev->next = line->next;
	if (line->next != NULL)
		line->next->prev = line->prev;

	free(line->text);
	free(line);
}

void delete_lines(Line *start_line)
{
	Line *l;
	while (start_line != NULL)
	{
		l = start_line;
		start_line = start_line->next;
		free (l->text);
		free(l);
	}
	return;
}

void save_file(char *save_filename)
{
	FILE *fp = fopen(save_filename, "w");
	if (!fp)
		return;

	Line *line = current_buffer->first_line;
	while (line != NULL)
	{
		fwrite(line->text, sizeof(char), line->length, fp);
		fputc('\n', fp);
		line = line->next;
	}
	fclose(fp);
	current_buffer->modified = false;

	return;
}

bool open_file(char *open_filename)
{
	char *read_line = NULL;
	Line *line = NULL;
	ssize_t length = 0;
	size_t max_length = 0;

	FILE *fp = fopen(open_filename, "r");
	if (!fp)
		return false;

	current_buffer = add_buffer();
	current_buffer->filename = (char *)realloc(current_buffer->filename, sizeof(char) * strlen(open_filename) + 1);
	strcpy(current_buffer->filename, open_filename);

	while ((length = getline(&read_line, &max_length, fp)) != -1)
	{
		// Trim trailing newlines
		while (length > 0 && (read_line[length - 1] == '\n' || read_line[length - 1] == '\r'))
			length--;
		current_buffer->lines += 1;
		line = insert_line(line, NULL, read_line, length);
		if (current_buffer->first_line == NULL)
			current_buffer->first_line = line;
	}

	free(read_line);
	fclose(fp);
	current_buffer->modified = false;

	return true;
}

void new_file(char *new_filename)
{
	current_buffer = add_buffer();
	current_buffer->filename = (char *)realloc(current_buffer->filename, sizeof(char) * strlen(new_filename) + 1);
	strcpy(current_buffer->filename, new_filename);
	current_buffer->first_line = insert_line(NULL, NULL, NULL, 0);
	current_buffer->lines = 1;
	current_buffer->modified = false;
	return;
}

void load_options()
{
    char *read_line = NULL;
    char option[20], value[20];
	size_t max_length = 0;

    // Set default options
    o_tabsize = 4;
    o_messagecooldown = 2;

    char filename[256];
    strcat(strcpy(filename, getenv("HOME")), "/.write");

	FILE *fp = fopen(filename, "r");
	if (!fp) return;

	while (getline(&read_line, &max_length, fp) != -1)
	{
        // Skip blank lines and comments prefaced with '#'
        if (strlen(read_line) == 1) continue;
        if (read_line[0] == '#') continue;

        sscanf(read_line, "%20s %20s", option, value);

        if (strcmp(option, "TABSIZE") == 0) o_tabsize = atoi(value);
        else if (strcmp(option, "MESSAGE_COOLDOWN") == 0) o_messagecooldown = atoi(value);
	}

 	free(read_line);
	fclose(fp);
    return;
}

buffer *add_buffer()
{
	buffer *new_buffer;
	new_buffer = add_sbuffer();
	if (current_buffer == NULL)
		new_buffer->next = NULL;
	else
	{
		new_buffer->next = current_buffer->next;
		current_buffer->next = new_buffer;
	}
	return new_buffer;
}

buffer *add_sbuffer()
{
	buffer *new_buffer;
	new_buffer = (buffer *) malloc(sizeof(buffer));
	new_buffer->first_line = NULL;
	new_buffer->current_line = NULL;
	new_buffer->first_screen_line = NULL;
	new_buffer->lines = 0;
	new_buffer->filename = NULL;
	return new_buffer;
}

void close(buffer *close_buffer)
{
	if (close_buffer == first_buffer)
	{
		first_buffer = close_buffer->next;
		current_buffer = first_buffer;
	}
	else
	{
		// Loop through buffers until we find the preceding
		current_buffer = first_buffer;
		while (current_buffer->next != close_buffer)
			current_buffer = current_buffer->next;
		current_buffer->next = close_buffer->next;
	}
	delete_lines(close_buffer->first_line); // Clear the text buffer starting at the first line
	free(close_buffer->filename);
	free(close_buffer);
	return;
}

void prompt_save()
{
	char s[MAX_FILENAME_LENGTH];
	if (get_input("Save as ", current_buffer->filename, s, MAX_FILENAME_LENGTH))
	{
		current_buffer->filename = (char *)realloc(current_buffer->filename, sizeof(char) * strlen(s) + 1);
		strcpy(current_buffer->filename, s);
		save_file(current_buffer->filename);

		message("Saved");
	}
}

bool find(char *find_string, Line *start_line, int start_x)
{
	Line *l = start_line;
	int find_y = current_buffer->cy + current_buffer->offsety;
	int find_x = start_x + 1;

	while (l != start_line->prev)
	{
		char *match = strstr(l->text + find_x, find_string);
		if (match)
		{
			goto_line(find_y + 1);
			current_buffer->cx = match - l->text;
			check_boundx();
			return true;
		}

		l = l->next;
		find_y += 1;
		find_x = 0;

		// If last line then jump back to first
		if ((l == NULL) && (start_line != current_buffer->first_line))
		{
			l = current_buffer->first_line;
			find_y = 0;
		}
	}
	return false;
}

void resize_window()
{
	getmaxyx(stdscr, windowy, windowx);
	windowy -= 2; // Reduce for status bar and message buffer
}

bool run_command(char *s)
{
	char *token;
	char msg[255];
	token = strtok(s, " ");
	if (token == NULL) return false;

	if (strcmp(token, "count") == 0)
	{
		token = strtok(NULL, " ");
		if (token == NULL) return false;
		if (strcmp(token, "loc") == 0)
		{
			int lines = 0;
			Line *l = current_buffer->first_line;
			while (l != NULL)
			{
				if (l->length <= 0)  // Blank line
				{
					l = l->next;
					continue;
				}

				int trim_start = 0;
				while ((trim_start < l->length) && (isspace(l->text[trim_start]))) trim_start++;

				if (trim_start == l->length) // Blank line of whitespace
				{
					l = l->next;
					continue;
				}

				if (l->text[trim_start] == '#') // Comment
				{
					l = l->next;
					continue;
				}

				lines++;
				l = l->next;
			}
			sprintf(msg, "LOC: %d", lines);
			message(msg);
		}
		else
		{
			// default count words
		}
	}

	// Return true if executed command
	return true;
}


