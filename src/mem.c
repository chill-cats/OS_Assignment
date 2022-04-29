
#include "mem.h"
#include "common.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static BYTE _ram[RAM_SIZE];

static struct {
    uint32_t proc; // ID of process currently uses this page
    int index;     // Index of the page in the list of pages allocated
                   // to the process.
    int next;      // The next page in the list. -1 if it is the last
                   // page.
} _mem_stat[NUM_PAGES];

static pthread_mutex_t mem_lock;

void init_mem(void) {
    memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
    memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
    pthread_mutex_init(&mem_lock, NULL);
    INFO_PRINT("Memory initialized\n");
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) {
    return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
    return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
    return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
static struct page_table_t *get_page_table(
    addr_t index,                    // Segment level index
    struct seg_table_t *seg_table) { // first level table

    for (uint32_t i = 0; i < seg_table->segment_count; i++) {

        if (seg_table->segments[i].v_index == index) {
            return seg_table->segments[i].pages_table;
        }
    }
    return NULL;
}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(
    addr_t virtual_addr,   // Given virtual address
    addr_t *physical_addr, // Physical address to be returned
    struct pcb_t *proc) {  // Process uses given virtual address

    /* Offset of the virtual address */
    addr_t offset = get_offset(virtual_addr);
    /* The first layer index */
    addr_t segment_index = get_first_lv(virtual_addr);
    /* The second layer index */
    addr_t page_index = get_second_lv(virtual_addr);

    /* Search in the first level */
    struct page_table_t *page_table = NULL;
    page_table = get_page_table(segment_index, proc->seg_table);
    if (page_table == NULL) {
        return 0;
    }

    for (uint32_t i = 0; i < page_table->page_count; i++) {

        if (page_table->pages[i].v_index == page_index) {

            uint32_t physical_index = page_table->pages[i].p_index;
            *physical_addr = ((physical_index << OFFSET_LEN) | (offset));
            INFO_PRINT("PID %d: translate 0x%02x -> 0x%02x\n", proc->pid,
                       virtual_addr, *physical_addr);
            return 1;
        }
    }
    return 0;
}

static void set_mem_stat(uint32_t index, uint32_t pid, int next) {
    _mem_stat[index].proc = pid;
    _mem_stat[index].index = index;
    _mem_stat[index].next = next;
}

static void unset_mem_stat(uint32_t index) {
    _mem_stat[index].proc = 0;
    _mem_stat[index].index = 0;
    _mem_stat[index].next = -1;
}

static void initialize_page_table(struct page_table_t *page_table) {
    for (uint32_t i = 0; i < MAX_PAGE_PER_SEGMENT; i++) {
        page_table->pages[i].p_index = MAX_PAGE_PER_SEGMENT;
        page_table->pages[i].v_index = MAX_PAGE_PER_SEGMENT;
    }
    page_table->page_count = 0;
}

addr_t alloc_mem(uint32_t size, struct pcb_t *proc) {
    INFO_PRINT("PID %d: Allocating %d bytes\n", proc->pid, size);
    pthread_mutex_lock(&mem_lock);
    addr_t ret_mem = 0;

    uint32_t required_page_count = (size % PAGE_SIZE) ? size / PAGE_SIZE + 1 : size / PAGE_SIZE; // Number of pages we will use
    INFO_PRINT("PID %d: Required page count: %d\n", proc->pid, required_page_count);
    uint32_t *free_frame_physical_indexes = calloc(required_page_count, sizeof(uint32_t));
    uint32_t free_frame_available = 0;

    for (uint32_t free_frame_physical_index = 0; free_frame_physical_index < NUM_PAGES; free_frame_physical_index++) {
        if (_mem_stat[free_frame_physical_index].proc == 0) {
            free_frame_physical_indexes[free_frame_available] = free_frame_physical_index;
            free_frame_available++;
        }

        if (free_frame_available == required_page_count) {
            break;
        }
    }
    const uint32_t start_of_chunk = proc->bp;
    const uint32_t end_of_chunk = proc->bp + PAGE_SIZE * required_page_count;
    if (free_frame_available < required_page_count && end_of_chunk > RAM_SIZE) {
        INFO_PRINT("PID %d: Not enough memory\n", proc->pid);
        pthread_mutex_unlock(&mem_lock);
        return 1;
    }

    for (uint32_t i = 0; i < free_frame_available; i++) {
        const uint32_t free_frame_physical_index = free_frame_physical_indexes[i];
        // _mem_stat handling
        set_mem_stat(free_frame_physical_index, proc->pid, i == free_frame_available - 1 ? -1 : free_frame_physical_indexes[i + 1]);
        INFO_PRINT("PID %d: Free frame physical index: %d\n", proc->pid, free_frame_physical_index);
        INFO_PRINT("PID %d: Free frame info: proc: %d, index: %d, next: %d\n", proc->pid, _mem_stat[free_frame_physical_index].proc, _mem_stat[free_frame_physical_index].index, _mem_stat[free_frame_physical_index].next);

        uint32_t current_address = start_of_chunk + i * PAGE_SIZE;
        uint32_t current_segment_v_index = get_first_lv(current_address);
        uint32_t current_page_v_index = get_second_lv(current_address);

        struct page_table_t *page_table = NULL;

        for (uint32_t segment_index = 0; segment_index < proc->seg_table->segment_count; segment_index++) {
            if (proc->seg_table->segments[segment_index].v_index == current_segment_v_index) {
                page_table = proc->seg_table->segments[segment_index].pages_table;
                break;
            }
        }

        if (page_table == NULL) {
            page_table = calloc(1, sizeof(struct page_table_t));
            proc->seg_table->segments[proc->seg_table->segment_count].pages_table = page_table;
            proc->seg_table->segments[proc->seg_table->segment_count].v_index = current_segment_v_index;
            proc->seg_table->segment_count++;

            initialize_page_table(page_table);
        }

        page_table->pages[page_table->page_count].p_index = free_frame_physical_index;
        page_table->pages[page_table->page_count].v_index = current_page_v_index;
        page_table->page_count++;
    }

    /* We could allocate new memory region to the process */
    ret_mem = proc->bp;
    proc->bp = end_of_chunk;
    /* Update status of physical pages which will be allocated
     * to [proc] in _mem_stat. Tasks to do:
     * 	- Update [proc], [index], and [next] field
     * 	- Add entries to segment table page tables of [proc]
     * 	  to ensure accesses to allocated memory slot is
     * 	  valid. */

#ifdef DEBUG
    // dump();
#endif
    pthread_mutex_unlock(&mem_lock);
    return ret_mem;
}

void adjust_bp(struct pcb_t *proc) {
    if (proc->seg_table->segment_count == 0) {
        proc->bp = 1024;
        return;
    }
    uint32_t max_segment_v_index = 0;
    uint32_t max_v_index_segment_index = 0;
    for (uint32_t i = 0; i < proc->seg_table->segment_count; i++) {
        if (proc->seg_table->segments[i].v_index > max_segment_v_index) {
            max_segment_v_index = proc->seg_table->segments[i].v_index;
            max_v_index_segment_index = i;
        }
    }
    uint32_t max_page_v_index = 0;
    for (uint32_t i = 0; i < proc->seg_table->segments[max_v_index_segment_index].pages_table->page_count; i++) {
        if (proc->seg_table->segments[max_v_index_segment_index].pages_table->pages[i].v_index > max_page_v_index) {
            max_page_v_index = proc->seg_table->segments[max_v_index_segment_index].pages_table->pages[i].v_index;
        }
    }

    proc->bp = ((max_v_index_segment_index << (OFFSET_LEN + PAGE_LEN)) | (max_page_v_index << OFFSET_LEN)) + PAGE_SIZE;
}

int free_mem(addr_t address, struct pcb_t *proc) {
    pthread_mutex_lock(&mem_lock);

    uint32_t current_address = address;
    bool hasNext = true;
    while (hasNext) {
        uint32_t current_segment_v_index = get_first_lv(current_address);
        uint32_t current_page_v_index = get_second_lv(current_address);

        struct page_table_t *page_table = get_page_table(current_segment_v_index, proc->seg_table);
        if (page_table == NULL) {
            pthread_mutex_unlock(&mem_lock);
            return 1;
        }

        uint32_t current_page_index = 32;
        for (uint32_t i = 0; i < page_table->page_count; i++) {
            if (page_table->pages[i].v_index == current_page_v_index) {
                current_page_index = i;
                break;
            }
        }
        if (current_page_index == 32) {
            pthread_mutex_unlock(&mem_lock);
            return 1;
        }

        uint32_t frame_index = page_table->pages[current_page_index].p_index;
        hasNext = _mem_stat[frame_index].next != -1;
        unset_mem_stat(frame_index);

        page_table->pages[current_page_index].p_index = 32;
        page_table->pages[current_page_index].v_index = 32;
        for (int i = current_page_index; i < page_table->page_count - 1; i++) {
            page_table->pages[i] = page_table->pages[i + 1];
        }

        page_table->pages[page_table->page_count].p_index = 32;
        page_table->pages[page_table->page_count].v_index = 32;
        page_table->page_count--;

        if (page_table->page_count == 0) {
            for (int i = 0; i < proc->seg_table->segment_count; i++) {
                if (proc->seg_table->segments[i].v_index == current_segment_v_index) {
                    free(proc->seg_table->segments[i].pages_table);
                    for (int j = i; j < proc->seg_table->segment_count - 1; j++) {
                        proc->seg_table->segments[j] = proc->seg_table->segments[j + 1];
                    }
                    proc->seg_table->segment_count--;
                    break;
                }
            }
        }
        adjust_bp(proc);
        current_address += PAGE_SIZE;
    }
#ifdef DEBUG
    // dump();
#endif
    pthread_mutex_unlock(&mem_lock);

    return 0;
}

int read_mem(addr_t address, struct pcb_t *proc, BYTE *data) {
    addr_t physical_addr;
    if (translate(address, &physical_addr, proc)) {
        *data = _ram[physical_addr];
        INFO_PRINT("PID: %d read at address 0x%x, got data 0x%02x\n", proc->pid, address, *data);
        return 0;
    } else {
        INFO_PRINT("PID: %d failed to read at address 0x%02x\n", proc->pid, address);
        return 1;
    }
}

int write_mem(addr_t address, struct pcb_t *proc, BYTE data) {
    addr_t physical_addr;
    if (translate(address, &physical_addr, proc)) {
        _ram[physical_addr] = data;
        INFO_PRINT("PID: %d wrote at address 0x%x, with data 0x%02x\n", proc->pid, address, data);
        return 0;
    } else {
        INFO_PRINT("PID: %d failed to write at address 0x%x, with data 0x%02x\n", proc->pid, address, data);
        return 1;
    }
}

void dump(void) {
    int i;
    for (i = 0; i < NUM_PAGES; i++) {
        if (_mem_stat[i].proc != 0) {
            printf("%03d: ", i);
            printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
                   i << OFFSET_LEN,
                   ((i + 1) << OFFSET_LEN) - 1,
                   _mem_stat[i].proc,
                   _mem_stat[i].index,
                   _mem_stat[i].next);
            int j;
            for (j = i << OFFSET_LEN;
                 j < ((i + 1) << OFFSET_LEN) - 1;
                 j++) {

                if (_ram[j] != 0) {
                    printf("\t%05x: %02x\n", j, _ram[j]);
                }
            }
        }
    }
}
