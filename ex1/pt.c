
#include "os.h"
#define INTERVAL 9
#define LEVELS 5 // there are 5 levels in the multy level page table counting from 0 - 4
#define VALID 1

uint64_t getLoc(uint64_t vpn, int level) {  // level = 0 ... 4 
	// 44 - 36 ,35 - 27 ,26 - 18 ,17 - 9, 8 - 0
	uint64_t value = 0x1FF000000000 >> (level * INTERVAL);
	return (vpn & value) >> ((4 - level) * INTERVAL);
	}

int isValid(uint64_t pageAddress){
	return  pageAddress & 1;
}
void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn){
	int i;
	uint64_t index;
	uint64_t currPageAddress;
    uint64_t *pt_pointer = phys_to_virt(pt << 12); // access to 0 LEVEL of the page table 

	for(i = 0; i < LEVELS; i++){

		index = getLoc(vpn,i);
		if (i == 4) { // this is the lowest level - inject the ppn to the right PTE
			currPageAddress = (ppn == NO_MAPPING) ? currPageAddress - 1 : (ppn << 12) + 1;
		}
		
		if (i < 4) {  // not the lowes level
            currPageAddress = pt_pointer[index];
            if ((ppn == NO_MAPPING) & !isValid(currPageAddress))
				return;
            currPageAddress = !isValid(currPageAddress) ? (alloc_page_frame() << 12) + 1 : currPageAddress;
		}
		pt_pointer[index] = currPageAddress;
		pt_pointer = phys_to_virt(currPageAddress - 1);
	}
}

uint64_t page_table_query(uint64_t pt, uint64_t vpn) {
	// vpn is 45 bits 
	// ppn is 52 bits 
	// pt is 52 bits 

	int i;
	uint64_t index;
	uint64_t result;

	uint64_t *pt_pointer = phys_to_virt(pt << 12); // access to 0 LEVEL of the page table 

	for(i = 0; i < LEVELS; i++){
		index = getLoc(vpn,i);
		result = pt_pointer[index];

		if (i == 4) { // this is the lowest level - check if it's valid
			result = isValid(result) ? (result - 1) >> 12 : NO_MAPPING;
			return result; 
		}
		
		if (!isValid(result)) // not the lowest level
			return NO_MAPPING;
		
		pt_pointer = phys_to_virt(result - 1);
	}
    
    return result;
}