#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

// File Explorer
#include <dirent.h>
#include <sys/stat.h>

// Linux
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <stdarg.h> // For variadic arguments

void log_to_file(const char *format, ...) {
    FILE *log_file = fopen("debug.log", "a");
    if (!log_file) {
        perror("Error opening log file");
        return;
    }

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args); // Write formatted string to file
    va_end(args);

    fprintf(log_file, "\n"); // Add a newline for each log entry
    fclose(log_file);
}


void disableRawMode(struct termios* original) {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, original);
}

struct termios enableRawMode() {
	struct termios orig_termios;
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
		perror("tcgetattr");
		exit(1);
	}

	struct termios raw = orig_termios;
	raw.c_cflag |= (CS8);
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);   // Turn off echo, canonical mode, and signals
	raw.c_cc[VMIN] = 1;                       // Minimum number of bytes for read
	raw.c_cc[VTIME] = 0;                      // No timeout for read
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
		perror("tcsetattr");
		exit(1);
	}

	return orig_termios; 
}

#define INIT_OUT_BUFF_SZ  10
typedef struct {
	char* buffer;
	size_t cap;
	size_t size;
} OutBuffer;

void ob_init(OutBuffer* ob){
	ob->buffer = malloc(sizeof(char) * INIT_OUT_BUFF_SZ);
	ob->cap = INIT_OUT_BUFF_SZ;
	ob->size = 0;
}

void ob_append(OutBuffer* ob, char* text, int text_size){
	// int text_size = strlen(text);
	if(ob->size + text_size >= ob->cap){
		// Resize 
		int new_cap = (ob->cap * 2) + text_size;
		char* new_buff = malloc(sizeof(char) * new_cap);
		memcpy(new_buff, ob->buffer, ob->size);
		free(ob->buffer);
		ob->buffer = new_buff;
		ob->cap = new_cap;
		
	}

	memcpy(ob->buffer + ob->size, text, text_size);
	ob->size += text_size;
}


#define INIT_GAP_SIZE 5
typedef struct {
	char* buffer;
    int gap_start;           
    int gap_end;            
	size_t cap;
	size_t logical_size; // Size of the text (excluding the gap)
} GapBuffer;



int gb_init(GapBuffer* gb, char* text, int text_size){
	int buffer_cap = text_size + INIT_GAP_SIZE;

	gb->buffer = malloc(sizeof(char) * buffer_cap);
	if(!gb->buffer) return 0;
	
	// Copy the inital text into the buffer
	memcpy(gb->buffer, text, text_size);
	
	gb->gap_start = text_size;
	gb->gap_end = buffer_cap;
	gb->cap = buffer_cap;
	gb->logical_size = text_size;

	return 1;
}

void gb_move_gap(GapBuffer* gb, int pos){
	
	int move_gap_left = pos < gb->gap_start;

	if(move_gap_left){
        while (gb->gap_start > pos) {
			// Decrement gap pointers
			gb->gap_start--;
			gb->gap_end--;

			// Move gap left 
			gb->buffer[gb->gap_end] = gb->buffer[gb->gap_start];
		}
	} else {
        while (gb->gap_start < pos) {
			// Move gap right
			gb->buffer[gb->gap_start] = gb->buffer[gb->gap_end];

			// Increment gap pointers
			gb->gap_start++;
			gb->gap_end++;


		}
	}

}

int gb_insert(GapBuffer* gb, int pos, char c){
	if (pos < 0 || pos > gb->logical_size) return 0;
    gb_move_gap(gb, pos);
	
	// Resize gap
	if(gb->gap_start == gb->gap_end){
		int new_cap = gb->cap * 2;
		char* new_buffer = malloc(sizeof(char)*new_cap);


		// Move text before gap 
		memcpy(new_buffer, gb->buffer, gb->gap_start);
		
		int text_after_gap_size = gb->cap - gb->gap_end;
		int new_gap_end = new_cap - text_after_gap_size;

		// Move text after gap 
		memcpy(new_buffer + new_gap_end, gb->buffer + gb->gap_end, text_after_gap_size);
		
		// Update
		free(gb->buffer);
		gb->cap = new_cap;
		gb->gap_end = new_gap_end;
		gb->buffer = new_buffer;
	}

    // Insert the character and move gap start
    gb->buffer[gb->gap_start++] = c;
	gb->logical_size++;

	return 1;
}



int gb_insert_chunk(GapBuffer* gb, int pos, const char* text, int text_size) {
    if (pos < 0 || pos > gb->logical_size) return 0;
    gb_move_gap(gb, pos);

    // Resize gap if necessary
    while (gb->gap_start + text_size > gb->gap_end) {
        int new_cap = gb->cap * 2;
        char* new_buffer = malloc(sizeof(char) * new_cap);

        // Move text before gap
        memcpy(new_buffer, gb->buffer, gb->gap_start);

        int text_after_gap_size = gb->cap - gb->gap_end;
        int new_gap_end = new_cap - text_after_gap_size;

        // Move text after gap
        memcpy(new_buffer + new_gap_end, gb->buffer + gb->gap_end, text_after_gap_size);

        free(gb->buffer);
        gb->buffer = new_buffer;
        gb->cap = new_cap;
        gb->gap_end = new_gap_end;
    }

    // Insert the chunk
    memcpy(gb->buffer + gb->gap_start, text, text_size);
    gb->gap_start += text_size;
    gb->logical_size += text_size;

    return 1;
}

int gb_delete(GapBuffer* gb, int pos){

	if (pos < 0 || pos >= gb->logical_size) return 0;


    gb_move_gap(gb, pos);

    // Delete the character by expanding the gap backward
    if (gb->gap_start > 0) {
        gb->gap_start -= 1;
		gb->logical_size--;
    }
	return 1;
}

void gb_free(GapBuffer* gb) {
    free(gb->buffer);
}


char* gb_render(GapBuffer* gb) {
    char* rendered_text = malloc(sizeof(char) * (gb->logical_size + 1));
    if (!rendered_text) return NULL;

    // Copy text before the gap
    memcpy(rendered_text, gb->buffer, gb->gap_start);

    // Copy text after the gap
    memcpy(rendered_text + gb->gap_start, gb->buffer + gb->gap_end, gb->logical_size - gb->gap_start);

    rendered_text[gb->logical_size] = '\0'; // Null-terminate
    return rendered_text;
}

// char* gb_render(GapBuffer* gb) {
//     char* rendered_text = malloc(sizeof(char) * (gb->logical_size + 1));
//     if (!rendered_text) return NULL;
//
//     // Iterate through the buffer and copy characters to rendered_text
//     int write_pos = 0; // Position in rendered_text
//     for (int i = 0; i < gb->cap; i++) {
//         if (i < gb->gap_start || i >= gb->gap_end) {
//             // Outside the gap
//             if (gb->buffer[i] == ' ') {
//                 rendered_text[write_pos++] = '*'; // Replace space with '*'
//             } else {
//                 rendered_text[write_pos++] = gb->buffer[i];
//             }
//         }
//     }
//
//     rendered_text[gb->logical_size] = '\0'; // Null-terminate
//     return rendered_text;
// }


int gb_print(GapBuffer* gb) {
    for (int i = 0; i < gb->cap; i++) {
        if (i == gb->gap_start) {
            // printf("<");
        }
        if (i >= gb->gap_start && i < gb->gap_end) {
            printf("_");
        } else {
            printf("%c", gb->buffer[i]);
        }
        if (i == gb->gap_end - 1) {
            // printf(">");
        }
    }
    printf("\n");
    printf("Capacity: %zu, Gap Start: %d, Gap End: %d\n\n", gb->cap, gb->gap_start, gb->gap_end);
    return 1;
}



#define TAB_WIDTH 4
#define LINE_NUM_WIDTH 5
typedef struct LineNode {
	GapBuffer* text;
    struct LineNode* prev;   // Pointer to the previous line
    struct LineNode* next;   // Pointer to the next line
} LineNode;


typedef struct {
    LineNode* head;             // Head of the doubly linked list of lines
    int line_count;				// # of nodes in the linked list of lines
	
	int line_number_width;     // Amount of columns that the line numbers take up
	
	int term_width;
	int term_height;

	int row_offset;
	int col_offset;

	LineNode* cursor_line_ref;  // Reference to the LineNode the cursor is on
    int cursor_line_num;        // Line number where the cursor is
    int cursor_pos;             // Position within the current line where the cursor is
} TextEditor;


void editor_update_terminal_dim(TextEditor* te){
	
	struct winsize ws;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
	te->term_width = ws.ws_col;
	te->term_height = ws.ws_row;
}

void editor_init(TextEditor* te){
	te->head = NULL;	
	te->line_count = 0;
	te->line_number_width = LINE_NUM_WIDTH;

	te->cursor_line_ref = NULL;
	te->cursor_line_num = 0;
	te->cursor_pos = 0;
	te->row_offset = 0;
	te->col_offset = 0;

	editor_update_terminal_dim(te);
}

void editor_free(TextEditor* te) {
    LineNode* current = te->head;
    while (current != NULL) {
        LineNode* next = current->next;
        gb_free(current->text);      // Free gap buffer text 
        free(current->text);        // Free gap buffer object
        free(current);                // Free line node
        current = next;
    }
    te->head = NULL;
    te->line_count = 0;
}



char* editor_sanitize_line(char* text, int text_size, int* new_line_size){

	// Count # of tabs
	int num_tabs = 0;
	for(int i = 0; i < text_size; i++){
		if(text[i] == '\t') num_tabs++;
	}

	// Render text
	int rendered_size = ((text_size - num_tabs) + (num_tabs * TAB_WIDTH));
	char* rendered_text = malloc(sizeof(char) * rendered_size);
    if (!rendered_text) {
        perror("malloc");
        exit(1);
    }

	int index = 0;
	for(int i = 0; i < text_size; i++){
		// Tabs as spaces
		if(text[i] == '\t'){
			for(int j = 0; j < TAB_WIDTH; j++){
				rendered_text[index] = ' ';
				index++;
			}
		} else {
			rendered_text[index] = text[i];
			index++;
		}
	}
	
	*new_line_size = index;
	return rendered_text;
}

void editor_set_text(TextEditor* te, char* text, int text_size) {
    int line_start = 0;
    int current_pos = 0;
    LineNode* current_line = NULL;

    while (current_pos <= text_size) {

        // Treat end of text as a newline
        if (current_pos == text_size || text[current_pos] == '\n') {

			// Create new line
            LineNode* new_line = malloc(sizeof(LineNode));
            GapBuffer* gb = malloc(sizeof(GapBuffer));

			int new_line_size = 0;
			char* new_line_text = editor_sanitize_line(text + line_start, current_pos - line_start, &new_line_size);
            gb_init(gb, new_line_text, new_line_size);
			free(new_line_text);
            new_line->text = gb;

            // Add new line node to linked list
            if (current_line == NULL) {
                // First line
                new_line->prev = NULL;
                new_line->next = NULL;
                te->head = new_line;
            } else {
                // Link to previous line
                new_line->next = NULL;
                new_line->prev = current_line;
                current_line->next = new_line;
            }

            current_line = new_line;
            te->line_count++;

            // Move to the next line
            current_pos++; // Skip newline or move past the end of text
            line_start = current_pos;
        } else {
            current_pos++;
        }
    }
}


void editor_set_cursor_to_first_line(TextEditor* te) {
    if (te->head) {
        te->cursor_line_ref = te->head;
        te->cursor_line_num = 0; 
        te->cursor_pos = 0;
    }
}


void editor_insert_char(TextEditor* te, char c){
	gb_insert(te->cursor_line_ref->text, te->cursor_pos, c);
}

void editor_remove_char(TextEditor* te){
	gb_delete(te->cursor_line_ref->text, te->cursor_pos);
}

void editor_insert_newline(TextEditor* te){
	// Create new line
	LineNode* new_line = malloc(sizeof(LineNode));
	GapBuffer* gb = malloc(sizeof(GapBuffer));
	
    // Split index
    int split_index = te->cursor_pos;

    // Move the gap in the current line to the split index
    gb_move_gap(te->cursor_line_ref->text, split_index);

    // Initialize new line's gap buffer with text after the split
    char* after_split_text = te->cursor_line_ref->text->buffer + te->cursor_line_ref->text->gap_end;
    int after_split_size = te->cursor_line_ref->text->cap - te->cursor_line_ref->text->gap_end;
    gb_init(gb, after_split_text, after_split_size);
    new_line->text = gb;

    // Truncate the current line's logical size to the split index
    te->cursor_line_ref->text->logical_size = split_index;
    te->cursor_line_ref->text->gap_end = te->cursor_line_ref->text->cap;

    // Update the linked list
    if (te->cursor_line_ref->next) {
        te->cursor_line_ref->next->prev = new_line;
        new_line->next = te->cursor_line_ref->next;
    } else {
        new_line->next = NULL;
    }
    new_line->prev = te->cursor_line_ref;
    te->cursor_line_ref->next = new_line;


	// Update Editor fields
	te->cursor_line_ref = new_line;
	te->cursor_line_num++;
	te->line_count++;
    te->cursor_pos = 0;

	// No offset 
	te->col_offset = 0;


}

void handle_cursor_line_move(TextEditor* te, LineNode* current, LineNode* goal){
	int current_len = current->text->logical_size;
	int goal_len= goal->text->logical_size;

    if (te->cursor_pos > goal_len) {
        te->cursor_pos = goal_len;
    }

	// If when moving the cursor ot new line, the text is not visible due to the col_offset
    // Adjust col_offset to ensure the cursor is visible
    if (te->cursor_pos < te->col_offset) {
        te->col_offset = te->cursor_pos; // Scroll left
    } else if (te->cursor_pos >= te->col_offset + (te->term_width - te->line_number_width)) {
        te->col_offset = te->cursor_pos - (te->term_width - te->line_number_width) + 1; // Scroll right
    }
}

void editor_cursor_up(TextEditor* te){
	// Bounds Check
	if(te->cursor_line_num <= 0) return;

	handle_cursor_line_move(te, te->cursor_line_ref, te->cursor_line_ref->prev );
	te->cursor_line_ref = te->cursor_line_ref->prev;
	te->cursor_line_num--;
	
	// Handle Scrolling up
	if (te->cursor_line_num < te->row_offset) {
		te->row_offset--;
	}
}

void editor_cursor_down(TextEditor* te){
	
	handle_cursor_line_move(te, te->cursor_line_ref, te->cursor_line_ref->next );
	te->cursor_line_ref = te->cursor_line_ref->next;
	te->cursor_line_num++;


	// Handle Scrolling down
	if (te->cursor_line_num >= te->row_offset + te->term_height) {
		te->row_offset++;
	}
}


void editor_print_text(TextEditor* te) {
    LineNode* current = te->head;
    int line_number = 1;

    while (current != NULL) {
        printf("Line %d: ", line_number);
		printf("%s\n", gb_render(current->text));
        // gb_print(current->text);
        current = current->next;
        line_number++;
    }
}


typedef enum {
    HL_NORMAL,
    HL_KEYWORD,
    HL_STRING,
    HL_COMMENT,
    HL_NUMBER,
} HighlightType;

void editor_add_highlight(OutBuffer* ob, HighlightType type, char c){
	switch (type) {
		case HL_KEYWORD:
			ob_append(ob, "\033[1;32m", 7); // Green
			break;
		case HL_STRING:
			ob_append(ob, "\033[1;33m", 7); // Yellow
			break;
		case HL_COMMENT:
			ob_append(ob, "\033[1;30m", 7); // Gray
			break;
		case HL_NUMBER:
			ob_append(ob, "\033[1;31m", 7); // Red
			break;
		default:
			ob_append(ob, "\033[0m", 4); // Reset
			break;
	}

        ob_append(ob, &c, 1); // Add the character
        ob_append(ob, "\033[0m", 4);    // Reset after each character
}

void editor_render_line(TextEditor* te, OutBuffer* ob, LineNode* line){
        // Render the line text from gap buffer
        char* line_text = gb_render(line->text);
		int line_length = strlen(line_text);

		// Outside visible range
		if(line_length < te->col_offset){
			free(line_text);
			return;
		}


        // Only render the visible range
		int render_start = te->col_offset;
		int render_length = (line_length - render_start > te->term_width - te->line_number_width)
								? te->term_width - te->line_number_width
								: line_length - render_start;
		//
		// for(int i = 0; i < render_length; i++){
		// 	char curr_char = line_text[render_start + i];
		//
		//
		//
  //           if (curr_char == '"') {
  //               // Start of a string
  //               editor_add_highlight(ob, HL_STRING, curr_char);
		// 		i++; // Move to the next character
		// 		// Parse the string
		// 		while (i < render_length) {
		// 			curr_char = line_text[render_start + i];
		//
		// 			 if (curr_char == '"') {
		// 				editor_add_highlight(ob, HL_STRING, curr_char); // Highlight closing quote
		// 				break; // End of string
		// 			} else {
		// 				editor_add_highlight(ob, HL_STRING, curr_char); // Highlight string content
		// 			}
		//
		// 			i++;
		// 		}
  //           } else {
  //               // Handle other types of syntax highlighting (e.g., keywords, numbers)
		// 		 if (isdigit(curr_char)) {
  //                   editor_add_highlight(ob, HL_NUMBER, curr_char);
  //               } else {
  //                   editor_add_highlight(ob, HL_NORMAL, curr_char);
  //               }
  //           }
		// }

		ob_append(ob, line_text + render_start, render_length);
        

        free(line_text);
}


#define MOVE_HOME "\033[H"
#define CLEAR_HOME "\033[H\033[2J"
#define NEW_LINE "\033[1E"
#define CLEAR_LINE "\033[K"
void editor_render(TextEditor* te){

    OutBuffer ob;
    ob_init(&ob);


    // Set the cursor type to vertical bar 
    write(STDOUT_FILENO, "\033[5 q", strlen("\033[5 q"));

	
    LineNode* current = te->head;
    int current_line_num = 0;
    int visible_lines = 0;

    // Skip lines until the viewport starts
    while (current && current_line_num < te->row_offset) {
        current = current->next;
        current_line_num++;
    }

    while (current != NULL && visible_lines < te->term_height) {
        // Move cursor to the start of the line
        char cursor_move[32];
        snprintf(cursor_move, sizeof(cursor_move), "\033[%d;1H", visible_lines + 1);
        ob_append(&ob, cursor_move, strlen(cursor_move));

        // Clear the line
        ob_append(&ob, CLEAR_LINE, strlen(CLEAR_LINE));

		// Add line number to editor 
		char editor_line_num[64];

		if (current_line_num == te->cursor_line_num) {
			snprintf(editor_line_num, sizeof(editor_line_num), "\033[93;1m%4d \033[0m", current_line_num + 1); // Bright yellow, bold
		} else {
			snprintf(editor_line_num, sizeof(editor_line_num), "\033[90m%4d \033[0m", current_line_num + 1); // Dark gray
		}
		ob_append(&ob, editor_line_num, strlen(editor_line_num));

  //       // Render the line text
  //       char* line_text = gb_render(current->text);
		// int line_length = strlen(line_text);
		//
  //       // Calculate the visible range
  //       if (te->col_offset < line_length) {
  //           int render_start = te->col_offset;
  //           int render_length = (line_length - render_start > te->term_width - te->line_number_width)
  //                                   ? te->term_width - te->line_number_width
  //                                   : line_length - render_start;
  //           ob_append(&ob, line_text + render_start, render_length);
  //       }
		//
  //       free(line_text);

		editor_render_line(te, &ob, current);

        current = current->next;
        current_line_num++;
		visible_lines++;
	}

    // Set cursor position
    int adjusted_cursor_row = te->cursor_line_num - te->row_offset + 1;
    int adjusted_cursor_col = te->cursor_pos  - te->col_offset + te->line_number_width + 1;
    char cursor_position[32];
    snprintf(cursor_position, sizeof(cursor_position), "\033[%d;%dH", adjusted_cursor_row, adjusted_cursor_col);
    ob_append(&ob, cursor_position, strlen(cursor_position));


    // Write the buffer to the terminal
    write(STDOUT_FILENO, ob.buffer, ob.size);
    free(ob.buffer);


}


void editor_action_loop(TextEditor* te){


	int text_area_width = (te->term_width - te->line_number_width);
	char c;
	while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
		if (c == '\033') { // Escape sequence
			char seq[2];
			if (read(STDIN_FILENO, &seq[0], 1) == 0) break;
			if (read(STDIN_FILENO, &seq[1], 1) == 0) break;

			if (seq[0] == '[') {
				switch (seq[1]) {
					case 'A': // Up arrow
						editor_cursor_up(te);
						break;
					case 'B': // Down arrow
						editor_cursor_down(te);
						break;
					case 'D': // Left arrow
                        if (te->cursor_pos > 0) {
                            te->cursor_pos--;
                            if (te->cursor_pos < te->col_offset) {
                                te->col_offset = te->cursor_pos; // Scroll left
                            }
                        }

						break;
					case 'C': // Right arrow
                        if (te->cursor_pos < te->cursor_line_ref->text->logical_size) {
                            te->cursor_pos++;

							if (te->cursor_pos >= te->col_offset + text_area_width) {
								te->col_offset++; // Scroll right
							}

                        }
						break;
					default:
						printf("Unknown escape sequence: \\033[%c\n", seq[1]);
						break;
				}
			}
		} else if (iscntrl(c)) {
			if (c == 127) { // Backspace
                if (te->cursor_pos > 0) {
                    editor_remove_char(te);
                    te->cursor_pos--;
                    if (te->cursor_pos < te->col_offset) {
                        te->col_offset = te->cursor_pos; // Scroll left
                    }
                } else {
					// Append anything before cursor on the line to prev line
					LineNode* current_line = te->cursor_line_ref;
					LineNode* prev_line = current_line->prev;

					// Append current line's text to the previous line
					char* current_text = gb_render(current_line->text);
					int current_text_size = strlen(current_text);
					gb_insert_chunk(prev_line->text, prev_line->text->logical_size, current_text, current_line->text->logical_size);
					free(current_text);

					// Update line references
					prev_line->next = current_line->next;
					if (current_line->next) {
						current_line->next->prev = prev_line;
					}

					// Free the current line
					gb_free(current_line->text);
					free(current_line->text);
					free(current_line);

					// Horizontal Scrolling
					if(prev_line->text->logical_size - current_text_size >= text_area_width){
						te->col_offset = (prev_line->text->logical_size - current_text_size) - text_area_width; // If prev line needs scrolling when moving to it
					}

					te->cursor_line_ref = prev_line;
					te->cursor_line_num--;
					te->cursor_pos = prev_line->text->logical_size - current_text_size;
					te->line_count--;


				   // Adjust scrolling
					if (te->cursor_line_num < te->row_offset) {
						te->row_offset--;
					}
				}
			}

			if(c == 13){ // Enter
				editor_insert_newline(te);
			}

			if(c == 9){ // Tab
				
				int spaces_to_insert = TAB_WIDTH - (te->cursor_pos % TAB_WIDTH);
				for (int i = 0; i < spaces_to_insert; i++) {
					editor_insert_char(te, ' ');
					te->cursor_pos++;
					if (te->cursor_pos >= te->col_offset + text_area_width) {
						te->col_offset++;
					}
				}

			}
			printf("%d (control)\r\n", c);

		} else {
			log_to_file("Char inserted: %d ('%c')", c, c);
			editor_insert_char(te, c);
			te->cursor_pos++;

			// Handle horrizontal scrolling
			if (te->cursor_pos >= te->col_offset + text_area_width) {
				te->col_offset++;
				// te->col_offset = te->cursor_pos - (te->term_width - te->line_number_width) + 1; // Scroll right
			}
		}

		editor_render(te);
	}
}




char* read_file_to_str(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return NULL;
    }

    // Seek to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);

    // Seek back to the beginning of the file
    fseek(file, 0, SEEK_SET);

    char *buffer = (char*)malloc((fileSize + 1) * sizeof(char));
    if (buffer == NULL) {
        perror("Error allocating memory");
        fclose(file);
        return NULL;
    }

    // Read the file content into the buffer
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead != fileSize) {
        perror("Error reading file");
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[fileSize] = '\0';

    fclose(file);
    return buffer;
}


void exp_read_dir(){
    DIR* dirFile = opendir( "." );
    struct dirent* hFile;
    if ( dirFile ) {
		while (( hFile = readdir( dirFile )) != NULL ) 
		{
		   if ( !strcmp( hFile->d_name, "."  )) continue;
		   if ( !strcmp( hFile->d_name, ".." )) continue;

		 // in linux hidden files all start with '.'
		   if ( gIgnoreHidden && ( hFile->d_name[0] == '.' )) continue;

		 // dirFile.name is the name of the file. Do whatever string comparison 
		 // you want here. Something like:
			if ( strstr( hFile->d_name, ".c" ))
			   printf( "found an .txt file: %s", hFile->d_name );
		} 
		closedir( dirFile );
	}
}


int main() {

	struct termios original = enableRawMode(); 

    TextEditor te;
    editor_init(&te);

    // char txt[] = "my name is elijah\n\t\tthis is really cool\n\tanother line without newline";
	char* txt =  read_file_to_str("main.c");
    editor_set_text(&te, txt, strlen(txt));
	editor_set_cursor_to_first_line(&te);

	editor_render(&te); // Inital render of screen
	editor_action_loop(&te);



    write(STDOUT_FILENO, CLEAR_HOME, strlen(CLEAR_HOME));
    editor_free(&te);
	disableRawMode(&original);
	return 0;
}
