#ifndef allfunc_H
#define allfunc_H


// MACROS TO DEFINE SIZE OF UDP PACKET
#define TIMEOUT 500 // IN MS, YOU CAN CHANGE IT
#define PACKETSIZE 1472 // YOU CAN CHANGE IT, BUT IT MUST BE > HEADERSIZE
#define HEADERSIZE 17 // YOU CAN NOT CHANGE IT
#define DATASIZE PACKETSIZE - HEADERSIZE


// UDP METADATA + DATA PACKET
struct packet
{
	uint32_t seq_number;
	uint32_t seq_total;
	uint8_t type;
	uint8_t id[8];
	uint8_t data[DATASIZE];
};


// STRUCT TO CONTAIN FILE'S DATA
struct datablock
{
	int blocks_num;
	int block_size;
	int lastblock_size;
	char **blocks;
	int processed_num;
	unsigned int *order;
	uint32_t crc;
};


// CONSOLE PRINTS
void print_packet_info(struct packet *pack);

void print_console_message(const char *message);


// SERIALIZE PACKET
unsigned char * serialize_uint32_t(unsigned char *buffer, uint32_t value);

unsigned char * serialize_uint8_t(unsigned char *buffer, uint8_t value);

unsigned char * serialize_uint8_t_array(unsigned char *buffer, uint8_t *array, int array_len);

unsigned char * serialize_packet(unsigned char *buffer, struct packet *pack, int packet_data_len);


// DESERIALIZE PACKET
unsigned char * deserialize_uint32_t(unsigned char *buffer, uint32_t *value);

unsigned char * deserialize_uint8_t(unsigned char *buffer, uint8_t *value);

unsigned char * deserialize_uint8_t_array(unsigned char *buffer, uint8_t *array, int array_len);

unsigned char * deserialize_packet(unsigned char *buffer, struct packet *pack, int packet_data_len);

// CHECKSUM
uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len);


// FUNCTION TO MAKE RANDOM ORDER OF PACKETS
unsigned int * random_array(unsigned int start, unsigned int end);


// READ/PUT BYTES FROM/TO FILE
int file_getbytes(const char *filename, char **data, int *data_len);

int file_putbytes(const char *filename, char **data, int *data_len);


// FUNCTIONS TO WORK WITH STRUCT DATABLOCK
void datablock_free(struct datablock *db);

uint32_t crc32c_datablock(struct datablock *db);

struct datablock make_datablocks_from_file(const char *filename);

struct datablock make_new_datablocks(int blocks_num, int block_size, int lastblock_size);

int save_datablocks_to_file(const char *filename, struct datablock *db);


// FUNCTIONS TO WORK WITH UINT8_T ARRAYS
void uint8_t8_to_ullint(uint8_t x[8], unsigned long long int *y);

void ullint_to_uint8_t8(uint8_t x[8], unsigned long long int *y);

void uint8_t8_to_char8(uint8_t x[8], char s[9]);

void uint8_t4_to_uint(uint8_t x[4], unsigned int *y);

void uint_to_uint8_t4(uint8_t x[4], unsigned int *y);


#endif