#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

//"Compresses" the image by detecting large runs of zeroes and indicating them with just a few bytes

void test_decompress_algo(void) {
	FILE * infile = fopen("./rv32_c.bin", "rb");
	FILE * outfile = fopen("./rv32_d.bin", "wb");
	uint8_t rbuffer[512];
	uint16_t rptr = 0xFFFF;
	uint8_t wbuffer[512];
	uint32_t ram_ptr = 0;
	uint16_t wptr = 0;
	uint16_t rem;
	uint8_t flag1 = 0;
	uint32_t skip_len;
	uint8_t addr_progress = 0;
	while(1) {
		if(rptr >= 512) {
			rem = fread(rbuffer, 1, 512, infile);
			rptr = 0;
		}
		if(rem == 0) break;
		uint8_t nb = rbuffer[rptr++];
		rem--;
		if(addr_progress) {
			skip_len <<= 8;
			skip_len |= nb;
			addr_progress++;
			if(addr_progress != 3) continue;
			addr_progress = 0;
			if(wptr != 0) {
				fwrite(wbuffer, 1, wptr, outfile);
				ram_ptr += wptr;
				wptr = 0;
			}
			//if(skip_len > 1600) printf("%u\r\n", skip_len);
			//Write out zeroes
			memset(wbuffer, 0, 512);
			while(skip_len) {
				uint32_t wlen = skip_len > 512 ? 512 : skip_len;
				fwrite(wbuffer, 1, wlen, outfile);
				skip_len -= wlen;
				ram_ptr += wlen;
			}
			continue;
		}
		if(flag1) {
			flag1 = 0;
			if(nb != 0xFF) {
				skip_len = nb;
				addr_progress = 1;
				continue;
			}
		}else if(nb == 0xFF) {
			flag1 = 1;
			continue;
		}
		wbuffer[wptr++] = nb;
		if(wptr >= 512) {
			fwrite(wbuffer, 1, wptr, outfile);
			ram_ptr += wptr;
			wptr = 0;
		}
	}
	if(wptr != 0) fwrite(wbuffer, 1, wptr, outfile);
	putchar('\r');
	putchar('\n');
	fclose(infile);
	fclose(outfile);
}

void main(void) {
	FILE * infile = fopen("./rv32.bin", "rb");
	FILE * outfile = fopen("./rv32_c.bin", "wb");
	uint8_t buffer[1024];
	uint8_t buffer_out[1024+8];
	uint16_t ptr_out = 0;
	uint16_t ptr = 0xFFFF;
	uint16_t rem;
	uint8_t last_byte = 0xFF;
	uint8_t in_run = 0;
	uint32_t run_length;
	while(1) {
		if(ptr >= 1024) {
			ptr = 0;
			rem = fread(buffer, 1, 1024, infile);
		}
		if(rem == 0) break;
		uint8_t nb = buffer[ptr++];
		rem--;
		if(in_run) {
			if(nb != 0) {
				last_byte = 0xFF;
				buffer_out[ptr_out++] = 0xFF;
				buffer_out[ptr_out++] = run_length >> 16;
				buffer_out[ptr_out++] = run_length >> 8;
				buffer_out[ptr_out++] = run_length;
				buffer_out[ptr_out++] = nb;
				if(nb == 0xFF) buffer_out[ptr_out++] = nb;
				in_run = 0;
			}else run_length++;
		}else {
			if(nb == 0xFF) {
				buffer_out[ptr_out++] = 0xFF;
				buffer_out[ptr_out++] = 0xFF;
			}else if(nb == 0 && last_byte == 0) {
				ptr_out--; //Backtrack, to erase previous 0
				in_run = 1;
				run_length = 2;
				continue;
			}else {
				buffer_out[ptr_out++] = nb;
			}
			last_byte = nb;
		}
		
		if(ptr_out >= 1024 && !in_run) {
			fwrite(buffer_out, 1, ptr_out, outfile);
			ptr_out = 0;
			last_byte = 0xFF;
		}
	}
	if(ptr_out) fwrite(buffer_out, 1, ptr_out, outfile);
	if(in_run) {
		//Actually write out the zeroes here
		memset(buffer_out, 0, 1024);
		while(run_length) {
			uint32_t wlen = run_length > 1024 ? 1024 : run_length;
			fwrite(buffer_out, 1, wlen, outfile);
			run_length -= wlen;
		}
	}
	fclose(infile);
	fclose(outfile);
	//test_decompress_algo();
}
