#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <cstring>
#include "allfunc.h"


// CONSOLE PRINTS
void print_packet_info(struct packet *pack)
{
	uint8_t id_file[8];
	char idfile_s[8];
	memcpy(id_file, pack->id, 8);
	uint8_t8_to_char8(id_file, idfile_s);
	std::cout << "\t\t\tPacket seq_number: " << pack->seq_number << std::endl;
	std::cout << "\t\t\tPacket seq_total: " << pack->seq_total << std::endl;
	std::cout << "\t\t\tPacket type: " << (int)pack->type << std::endl;
	std::cout << "\t\t\tPacket id: " << idfile_s << std::endl;
}

void print_console_message(const char *message)
{
	std::time_t tm = std::time(0);
	std::cout << "\033[32m[ " << std::put_time(std::gmtime(&tm), "%Y/%m/%d %T") << " ]\033[0m " << message << std::endl;
}


// SERIALIZE PACKET
unsigned char * serialize_uint32_t(unsigned char *buffer, uint32_t value)
{
	buffer[0] = value >> 24;
	buffer[1] = value >> 16;
	buffer[2] = value >> 8;
	buffer[3] = value;
	return buffer + 4;
}

unsigned char * serialize_uint8_t(unsigned char *buffer, uint8_t value)
{
	buffer[0] = value;
	return buffer + 1;
}

unsigned char * serialize_uint8_t_array(unsigned char *buffer, uint8_t *array, int array_len)
{
	for (int i=0; i < array_len; i++)
	{
		buffer[i] = array[i];
	}
	return buffer + array_len;
}

unsigned char * serialize_packet(unsigned char *buffer, struct packet *pack, int packet_data_len)
{
	buffer = serialize_uint32_t(buffer, pack->seq_number);
	buffer = serialize_uint32_t(buffer, pack->seq_total);
	buffer = serialize_uint8_t(buffer, pack->type);
	buffer = serialize_uint8_t_array(buffer, pack->id, 8);
	buffer = serialize_uint8_t_array(buffer, pack->data, packet_data_len);
	return buffer;
}


// DESERIALIZE PACKET
unsigned char * deserialize_uint32_t(unsigned char *buffer, uint32_t *value)
{
	*value = ((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16) | ((uint32_t)buffer[2] << 8) | (uint32_t)buffer[3];
	return buffer + 4;
}


unsigned char * deserialize_uint8_t(unsigned char *buffer, uint8_t *value)
{
	*value = (uint8_t)buffer[0];
	return buffer + 1;
}

unsigned char * deserialize_uint8_t_array(unsigned char *buffer, uint8_t *array, int array_len)
{
	for (int i=0; i < array_len; i++)
	{
		array[i] = (uint8_t)buffer[i];
	}
	return buffer + array_len;
}

unsigned char * deserialize_packet(unsigned char *buffer, struct packet *pack, int packet_data_len)
{
	buffer = deserialize_uint32_t(buffer, &pack->seq_number);
	buffer = deserialize_uint32_t(buffer, &pack->seq_total);
	buffer = deserialize_uint8_t(buffer, &pack->type);
	buffer = deserialize_uint8_t_array(buffer, pack->id, 8);
	buffer = deserialize_uint8_t_array(buffer, pack->data, packet_data_len);
	return buffer;
}


// CHECKSUM
uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len)
{
	int k;
	crc = ~crc;
	while (len--) {
		crc ^= *buf++;
		for (k = 0; k < 8; k++)
			crc = crc & 1 ? (crc >> 1) ^ 0x82f63b78 : crc >> 1; // crc32c 0x82f63b78 crc32 0xedb88320
	}
	return ~crc;
}


// FUNCTION TO MAKE RANDOM ORDER OF PACKETS
unsigned int * random_array(unsigned int start, unsigned int end)
{
	int len = end - start;
	unsigned int *arr = (unsigned int *)malloc(len*sizeof(unsigned int));
	for (int i=0; i < len; i++) arr[i] = start + i;
	srand(time(0));
	for (int i=0; i < len; i++) std::swap(arr[i], arr[std::rand() % len]);
	return arr;
}


// READ/PUT BYTES FROM/TO FILE
int file_getbytes(const char *filename, char **data, int *data_len)
{
	std::streampos size;
	char *memblock;
	std::ifstream file;
	file.open(filename, std::ios::in | std::ios::binary | std::ios::ate);
	if (file.is_open()) {
		size = file.tellg();
		memblock = new char [size];
		file.seekg(0, std::ios::beg);
		file.read(memblock, size);
		file.close();
		*data = memblock;
		*data_len = (int)size;
		return 0;
	} else {
		std::cout << "Unable to open file" << std::endl;
		exit(1);
	}
}

int file_putbytes(const char *filename, char **data, int *data_len)
{
	std::ofstream file;
	file.open(filename, std::ios::out | std::ios::binary | std::ios::trunc);
	if (file.is_open()) {
		file.seekp(0, std::ios::beg);
		file.write(*data, *data_len);
		file.close();
		return 0;
	} else {
		std::cout << "Unable to open file" << std::endl;
		exit(1);
	}
}


// FUNCTIONS TO WORK WITH STRUCT DATABLOCK
void datablock_free(struct datablock *db)
{
	for (int i=0; i < db->blocks_num; i++) {
		free(db->blocks[i]);
		db->blocks[i] = NULL;
	}
	free(db->blocks);
	db->blocks = NULL;
	free(db->order);
	db->order = NULL;
}

uint32_t crc32c_datablock(struct datablock *db)
{
	uint32_t crc = 0;
	int block_size = db->block_size;
	int blocks_num = db->blocks_num;
	int lastblock_size = db->lastblock_size;
	for (int i=0; i < blocks_num-1; i++) {
		crc = crc32c(crc, (const unsigned char *)db->blocks[i], (size_t)block_size);
	}
	crc = crc32c(crc, (const unsigned char *)db->blocks[blocks_num-1], (size_t)lastblock_size);
	return crc;
}

struct datablock make_datablocks_from_file(const char *filename)
{
	char *bytedata;
	int bytedata_len;

	file_getbytes(filename, &bytedata, &bytedata_len);

	int block_size = DATASIZE;
	int blocks_num = bytedata_len / block_size;
	int lastblock_size = block_size;
	if (bytedata_len % block_size != 0) {
		lastblock_size = bytedata_len % block_size;
		blocks_num++;
	}
	
	char **blocks;
	blocks = (char **)malloc(blocks_num*sizeof(char *));
	for (int i=0; i < blocks_num; i++) {
		blocks[i] = (char *)malloc(block_size*sizeof(char));
	}
	for (int i=0; i < blocks_num-1; i++) {
		memcpy(blocks[i], bytedata + i*block_size, block_size);
	}
	memcpy(blocks[blocks_num-1], bytedata + (blocks_num-1)*block_size, lastblock_size);

	free(bytedata);

	// PRINT BLOCKS
	/*
	for (int i=0; i < blocks_num-1; i++) {
		for (int j=0; j < block_size; j++) {
			std::cout << blocks[i][j];
		}
		std::cout << std::endl;
	}
	for (int j=0; j < lastblock_size; j++) {
		std::cout << blocks[blocks_num-1][j];
	}
	std::cout << std::endl;
	*/
	// END PRINT

	struct datablock db;
	db.blocks_num = blocks_num;
	db.block_size = block_size;
	db.lastblock_size = lastblock_size;
	db.blocks = blocks;
	db.processed_num = 0;
	db.order = random_array(0, (unsigned int)blocks_num); // RANDOM ORDER OF PACKETS
	db.crc = crc32c_datablock(&db);

	return db;
}

struct datablock make_new_datablocks(int blocks_num, int block_size, int lastblock_size)
{
	struct datablock db;
	db.blocks_num = blocks_num;
	db.block_size = block_size;
	db.lastblock_size = lastblock_size;
	char **blocks;
	blocks = (char **)malloc(blocks_num*sizeof(char *));
	for (int i=0; i < blocks_num; i++) {
		blocks[i] = (char *)malloc(block_size*sizeof(char));
	}
	db.blocks = blocks;
	db.processed_num = 0;
	db.order = (unsigned int *)malloc(blocks_num*sizeof(unsigned int));
	db.crc = 0;
	return db;
}

int save_datablocks_to_file(const char *filename, struct datablock *db)
{
	char *bytedata;
	int bytedata_len;

	int blocks_num = db->blocks_num;
	int block_size = db->block_size;
	int lastblock_size = db->lastblock_size;

	bytedata_len = (blocks_num - 1)*block_size + lastblock_size;
	bytedata = (char *)malloc(bytedata_len*sizeof(char));

	for (int i=0; i < blocks_num-1; i++) {
		memcpy(bytedata + i*block_size, db->blocks[i], block_size);
	}
	memcpy(bytedata + (blocks_num-1)*block_size, db->blocks[blocks_num-1], lastblock_size);

	file_putbytes(filename, &bytedata, &bytedata_len);
	free(bytedata);
	return 0;
}


// FUNCTIONS TO WORK WITH UINT8_T ARRAYS
void uint8_t8_to_ullint(uint8_t x[8], unsigned long long int *y)
{
	*y = 0;
	for (int i=0; i < 8; i++) {
		*y = *y * 10 + (unsigned long long int)x[i];
	}
}

void ullint_to_uint8_t8(uint8_t x[8], unsigned long long int *y)
{
	unsigned long long int z = *y;
	for (int i=7; i >= 0; i--) {
		x[i] = (uint8_t)(z % 10);
		z = z / 10;
	}
}

void uint8_t8_to_char8(uint8_t x[8], char s[9])
{
	for (int i=0; i < 8; i++) {
		s[i] = '0' + (char)x[i];
	}
	s[8] = '\0';
}

void uint8_t4_to_uint(uint8_t x[4], unsigned int *y)
{
	for (int i=0; i < 4; i++) {
		*y = *y << 8 | (unsigned int)x[i];
	}
}

void uint_to_uint8_t4(uint8_t x[4], unsigned int *y)
{
	unsigned int z = *y;
	for (int i=3; i >= 0; i--) {
		x[i] = (uint8_t)(z & 255);
		z = z >> 8;
	}
}