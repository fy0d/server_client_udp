#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <map>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include "allfunc.h"

using namespace std;

// MAIN
int main(int argc, char* argv[])
{
	// SERVER INFO
	sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(54000);
	inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);
	socklen_t server_len = sizeof(server);
	
	// SOCKET
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	// int sock_err = socket(AF_INET, SOCK_STREAM, 0); // USE TO CREATE ERROR

	struct timeval tv;
	tv.tv_sec = TIMEOUT / 1000;
	tv.tv_usec = TIMEOUT * 1000 % 1000000;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		cout << "Can't set timeout for socket" << endl;
		exit(1);
	}

	// VARIABLES TO SEND, RECEIVE AND CONTAIN PACKET
	struct packet msg_packet, msg_packet_from;
	int msg_packet_data_len, msg_packet_from_data_len;
	unsigned char *msg;
	msg = (unsigned char *)malloc(PACKETSIZE*sizeof(unsigned char));
	char buf[PACKETSIZE];
	int send_bytes, receive_bytes;

	// MAP TO CONTAIN DATA
	map <unsigned long long int, struct datablock> datamap;
	map <unsigned long long int, struct datablock> :: iterator it = datamap.begin();
	struct datablock db;

	unsigned long long int idfile;
	uint8_t id_file[8];
	char idfile_s[9];
	
	srand(time(0));
	idfile = (rand() % 32768 + rand() % 32768 + rand() % 32768) * 100; // random id file in 1 client session
	ullint_to_uint8_t8(id_file, &idfile);
	uint8_t8_to_char8(id_file, idfile_s);

	// FILES TO SEND
	int files_num = argc - 1;
	int blocks_num_all = 0;
	for (int i=0; i < files_num; i++) {
		idfile++;
		ullint_to_uint8_t8(id_file, &idfile);
		uint8_t8_to_char8(id_file, idfile_s);
		db = make_datablocks_from_file(argv[i+1]);
		datamap[idfile] = db;

		blocks_num_all += datamap[idfile].blocks_num;
	}
	int block_i = 0;

	// CHECKSUMS
	unsigned int checksumserver;
	uint8_t checksum_server[4];
	unsigned int *checksums;
	map <unsigned long long int, unsigned int *> checksumsmap;

	// INFO IN CONSOLE
	print_console_message("CLIENT STARTED");
	print_console_message("INFO");
	cout << "\t\t\tUDP packet size: " << PACKETSIZE << endl;
	
	print_console_message("INFO");
	cout << "\t\t\tFiles to send: " << files_num << endl;
	cout << "\t\t\tPackets number: " << blocks_num_all << endl;

	while (block_i < blocks_num_all) {
		// DATA FROM WHICH FILE SENDING NOW
		it = datamap.begin();
		advance(it, block_i % datamap.size());
		idfile = it->first;

		datamap[idfile].processed_num++;

		// MAKE PACKET TO SEND MSG
		msg_packet.seq_number = datamap[idfile].order[datamap[idfile].processed_num - 1] + 1; // ORDER IS ARRAY OF RANDOM BLOCKS SEQUENCE
		msg_packet.seq_total = datamap[idfile].blocks_num;
		msg_packet.type = 1; // type PUT
		
		memset(msg_packet.id, 0, 8);
		ullint_to_uint8_t8(id_file, &idfile);
		uint8_t8_to_char8(id_file, idfile_s);
		memcpy(msg_packet.id, id_file, 8);

		memset(msg_packet.data, 0, DATASIZE);
		memcpy(msg_packet.data, datamap[idfile].blocks[msg_packet.seq_number-1], datamap[idfile].block_size);
		
		if (msg_packet.seq_number == (unsigned int)datamap[idfile].blocks_num) { // CLIENT IS SENDING PACKET WITH LAST BLOCK
			msg_packet_data_len = datamap[idfile].lastblock_size;
		} else {
			msg_packet_data_len = datamap[idfile].block_size;
		}

		free(msg);
		msg = (unsigned char *)malloc((HEADERSIZE + msg_packet_data_len)*sizeof(char));
		serialize_packet(msg, &msg_packet, msg_packet_data_len);

		// SEND AND RECIEVE. IF RECIEVE TIMED OUT DO AGAIN
		bool recieved = false;
		while (!recieved) {
			// SEND MSG
			while (true) {
				print_console_message("SENDING");
				send_bytes = sendto(sock, (char *)msg, HEADERSIZE + msg_packet_data_len, 0, (sockaddr *)&server, sizeof(server));
				if (send_bytes == SO_ERROR) {
					print_console_message("ERROR sending to server");
					cout << "\t\t\tError num: " << errno << endl;
					usleep(500000);
				} else {
					print_console_message("SEND OK");
					cout << "\t\t\tMessage size " << send_bytes << endl;
					print_packet_info(&msg_packet);
					break;
				}
			}

			// RECEIVE MSG
			while (true) {
				print_console_message("RECEIVING");
				memset(buf, 0, PACKETSIZE);
				receive_bytes = recvfrom(sock, buf, PACKETSIZE, 0, (sockaddr *)&server, &server_len);
				if (receive_bytes == -1) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						print_console_message("RECEIVE TIMEOUT");
						break;
					} else {
						print_console_message("ERROR receiving from server");
						cout << "\t\t\tError num: " << errno << endl;
						usleep(500000);
					}
				} else {
					print_console_message("RECEIVE OK");
					cout << "\t\t\tMessage size " << receive_bytes << endl;

					// PARSING RECIEVED PACKET
					msg_packet_from_data_len = receive_bytes - HEADERSIZE;
					deserialize_packet((unsigned char *)buf, &msg_packet_from, msg_packet_from_data_len);
					memset(id_file, 0, 8);
					memcpy(id_file, msg_packet_from.id, 8);
					uint8_t8_to_ullint(id_file, &idfile);
					uint8_t8_to_char8(id_file, idfile_s);

					// PRINT ABOUT PACKET
					print_packet_info(&msg_packet_from);

					// CLIENT RECIEVED ACK PACKET FOR CURRENT BLOCK
					if (msg_packet_from.seq_number == msg_packet.seq_number) {
						block_i++;
						recieved = true;
						break;
					}
				}
			}
		}

		// LAST PACKET OF FILE
		if (msg_packet_from.seq_total == (unsigned int)datamap[idfile].blocks_num) {
			memcpy(checksum_server, msg_packet_from.data, 4);
			uint8_t4_to_uint(checksum_server, &checksumserver);

			checksums = (unsigned int *)malloc(3*sizeof(unsigned int));
			checksums[0] = datamap[idfile].crc;
			checksums[1] = checksumserver;
			checksums[2] = (checksumserver == datamap[idfile].crc);
			checksumsmap[idfile] = checksums;

			print_console_message("INFO");
			cout << "\t\t\tFile with id " << idfile_s << " completely sended" << endl;
			cout << "\t\t\tCRC32C on client: " << datamap[idfile].crc << endl;
			cout << "\t\t\tCRC32C on server: " << checksumserver << endl;
			cout << "\t\t\tCRC32C match: " << (checksumserver == datamap[idfile].crc) << endl;

			datablock_free(&datamap[idfile]);
			datamap.erase(idfile);
		}

	}

	map <unsigned long long int, unsigned int *> :: iterator it_csm = checksumsmap.begin();
	for (; it_csm != checksumsmap.end(); it_csm++) {
		idfile = it_csm->first;
		ullint_to_uint8_t8(id_file, &idfile);
		uint8_t8_to_char8(id_file, idfile_s);
		print_console_message("INFO");
		cout << "\t\t\tFile id " << idfile_s << " checksums" << endl;
		cout << "\t\t\tCRC32C on client: " << checksumsmap[idfile][0] << endl;
		cout << "\t\t\tCRC32C on server: " << checksumsmap[idfile][1] << endl;
		cout << "\t\t\tCRC32C match: " << checksumsmap[idfile][2] << endl;
	}

	print_console_message("CLIENT FINISHED");

	close(sock);

	return 0;
}