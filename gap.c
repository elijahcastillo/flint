#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define INIT_GAP_SIZE 2
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

int gb_delete(GapBuffer* gb, int pos){

	if (pos < 0 || pos >= gb->logical_size) return 0;


    gb_move_gap(gb, pos);

    // Delete the character by expanding the gap backward
    if (gb->gap_start > 0) {
        gb->gap_start -= 1;
		gb->logical_size--;
    }
}

void gb_free(GapBuffer* gb) {
    free(gb->buffer);
}

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




int main(){

	char txt[] = "Hello Elijah";

	GapBuffer gb;
	gb_init(&gb, txt, strlen(txt));
	gb_print(&gb);
	gb_move_gap(&gb, 0);
	gb_print(&gb);
	gb_insert(&gb, 2, 'z');
	gb_insert(&gb, 3, 'y');
	gb_print(&gb);

	gb_insert(&gb, 4, 'v');

	gb_print(&gb);

	gb_insert(&gb, 5, 'm');

	gb_print(&gb);
}


