#include <stdlib.h>
#include <string.h>
#include "mmemory.h"

char* _g_allocator_memory = NULL;
int _g_allocator_memory_size = 0;
int _g_bytes_allocated = 0;

void set_values_to_block(page *page, block *block, int size_of_chunk) { 
	block->block_size = size_of_chunk;
	page->reserved_page_size += size_of_chunk;
	page->blocks_number++;
}

m_id add_first_block(m_err_code *error, page *page, int size_of_chunk) {
	page->block = calloc(1, sizeof(block));
	m_id ptr = (m_id)page->block;
	page->page_is_reserved = 1;
	set_values_to_block(page, page->block, size_of_chunk);
	*error = M_ERR_OK;
	return ptr;
}

m_id add_new_block(m_err_code *error, page *page, block *block, int size_of_chunk) { 
	block = calloc(1, sizeof(struct block));
	m_id ptr = (m_id)block;
	page->page_is_reserved = 1;
	set_values_to_block(page, block, size_of_chunk);
	*error = M_ERR_OK;
	return ptr;
}

page *init_segment_pages(segment *segment) {  
	segment->first_page = calloc(1, sizeof(page));
	segment->pages_number++;

	page* temp = segment->first_page;
	temp->page_size = mem->page_size;
	for (int i = 1; i < mem->pages_number; ++i) {
		page* next = calloc(1, sizeof(page));
		next->page_size = mem->page_size;
		temp->next = next;
		temp = temp->next;
		mem->segments->segment->pages_number++;
	}
	return segment->first_page;
}

page *find_page_with_block(m_id ptr) {
	segment* temp_segment = mem->segments->segment;
	for (int i = 0; i < mem->segments->segments_number; ++i) {
		page* temp_page = temp_segment->first_page;
		for (int j = 0; j < mem->pages_number; j++) {
			block* temp_block = temp_page->block;
			if (temp_block == NULL) {
				temp_page = temp_page->next;
				continue;
			}
			while (strcmp((char*)temp_block, (char*)ptr) != 0) {
				temp_block = temp_block->next;
				if (temp_block == NULL) {
					temp_page = temp_page->next;
					break;
				}
			}
			if (temp_block == NULL) {
				continue;
			}
			return temp_page;
		}
		temp_segment = temp_segment->next;
	}
	return NULL;
}

block *find_block(m_id ptr) {
	page* page = find_page_with_block(ptr);
	if (page == NULL) {
		return NULL;
	}
	block* temp_block = page->block;
	while (strcmp((char*)temp_block, (char*)ptr) != 0) {
		temp_block = temp_block->next;
	}
	return temp_block;
}

m_id m_malloc(int size_of_chunk, m_err_code *error) {
	if (_g_bytes_allocated + size_of_chunk > _g_allocator_memory_size) {
		*error = M_ERR_ALLOCATION_OUT_OF_MEMORY;
		return NULL;
	}

	_g_bytes_allocated += size_of_chunk;

	segment* temp_segment = mem->segments->segment;
	segment* previous_segment = temp_segment;
	for (int i = 0; i < mem->segments->segments_number; ++i) {
		page* temp_page = temp_segment->first_page;
		for (int j = 0; j < mem->pages_number; j++) {
			if (temp_page->page_is_reserved == 1) {
				temp_page = temp_page->next;
				continue;
			}

			if (temp_page->block == NULL) {
				return add_first_block(error, temp_page, size_of_chunk);
			}

			block* temp_block = temp_page->block;
			while (temp_block->next != NULL) { 
				temp_block = temp_block->next;
			}
			return add_new_block(error, temp_page, temp_block->next, size_of_chunk);
		}
		previous_segment = temp_segment;
		temp_segment = temp_segment->next;
	}

	segment *new_segment = calloc(1, sizeof(segment));     
	previous_segment->next = new_segment;
	mem->segments->segments_number++;

	page *temp_page = init_segment_pages(new_segment);
	return add_first_block(error, temp_page, size_of_chunk);
}

void m_free(m_id ptr, m_err_code* error) {
	page *page = find_page_with_block(ptr);
	if (page == NULL) {      
		*error = M_ERR_INVALID_CHUNK;
		return;
	}

	block *current_block = page->block;
	if (strcmp((char*)current_block, (char*)ptr) == 0) {
		block *next_block = current_block->next;
		page->block = next_block;
		page->reserved_page_size -= current_block->block_size;
		free(current_block);
		*error = M_ERR_OK;
		return;
	}

	block *previous_block = current_block;
	current_block = current_block->next;
	while (strcmp((char*)current_block, (char*)ptr) != 0) {
		current_block = current_block->next;
		previous_block = previous_block->next;
	}

	previous_block->next = current_block->next;
	free(current_block->data);
	page->reserved_page_size -= current_block->block_size;
	free(current_block);
	*error = M_ERR_OK;
}

void m_read(m_id read_from_id, void *read_to_buffer, int size_to_read, m_err_code *error) {
	block* block = find_block(read_from_id);
	if (block == NULL) {    
		*error = M_ERR_INVALID_CHUNK;
		return;
	}
	if (block->block_size < size_to_read) {
		*error = M_ERR_INVALID_CHUNK;
		return;
	}
	memcpy(read_to_buffer, block->data, size_to_read); 
	*error = M_ERR_OK;
}

void m_write(m_id write_to_id, void *write_from_buffer, int size_to_write, m_err_code *error) {
	block* block = find_block(write_to_id);
	if (block == NULL) {    
		*error = M_ERR_INVALID_CHUNK;
		return;
	}
	if (block->block_size < size_to_write) {     
		*error = M_ERR_INVALID_CHUNK;
		return;
	}
	void *data = malloc(size_to_write);      
	memcpy(data, write_from_buffer, size_to_write);       
	block->data = data;
	block->data_size = size_to_write;
	*error = M_ERR_OK;
}

void m_init(int number_of_pages, int size_of_page) {
	if (_g_allocator_memory != NULL) {
		free(_g_allocator_memory);
	}

	_g_allocator_memory_size = number_of_pages * size_of_page;
	_g_allocator_memory = malloc(_g_allocator_memory_size);
	_g_bytes_allocated = 0;

	mem = calloc(1, sizeof(memory)); 
	mem->segments = calloc(1, sizeof(segments));        
	mem->segments->segment = calloc(1, sizeof(segment));     
	mem->segments->segments_number++;
	mem->page_size = size_of_page;
	mem->pages_number = number_of_pages;

	init_segment_pages(mem->segments->segment);        
}