#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <map>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include "allfunc.h"

using namespace std;

// MAIN
int main(int argc, char* argv[])
{	
	// USE KEY "-lag" TO SIMULATE LAGGING SERVER
	bool lagging = false;
	if (argc > 1 && strcmp(argv[1], "-lag") == 0) {
		srand(time(0));
		lagging = true;
	}

	// SERVER INFO
	sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(54000);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	// int sock_err = socket(AF_INET, SOCK_STREAM, 0); // USE TO CREATE ERROR

	if (bind(sock, (sockaddr *)&server, sizeof(server)) == SO_ERROR) {
		cout << "Can not bind socket! " << errno << endl;
		exit(1);
	}

	// CLIENT INFO
	sockaddr_in client;
	socklen_t client_len = sizeof(client);
	char clientIp[256];

	// VARIABLES TO SEND, RECEIVE AND CONTAIN PACKET
	struct packet msg_packet;
	int msg_packet_data_len;
	unsigned char *msg = (unsigned char *)malloc(sizeof(unsigned char));
	char buf[PACKETSIZE];
	int send_bytes, receive_bytes;

	// MAP TO CONTAIN DATA
	map <unsigned long long int, struct datablock> datamap;
	struct datablock db;

	unsigned long long int idfile;
	uint8_t id_file[8];
	char idfile_s[9];
	
	idfile = 0;
	ullint_to_uint8_t8(id_file, &idfile);
	uint8_t8_to_char8(id_file, idfile_s);

	// INFO IN CONSOLE
	print_console_message("SERVER STARTED");
	print_console_message("INFO");
	cout << "\t\t\tUDP packet size: " << PACKETSIZE << endl;

	while (true) {
		// SIMULATING LAGS
		if (lagging) {
			if (rand() % 50 == 0) {
				usleep((rand() % (TIMEOUT * 3)) * 1000);
			}
		}

		// RECEIVE MSG
		while (true) {
			print_console_message("RECEIVING");
			memset(&client, 0, client_len);
			memset(buf, 0, PACKETSIZE);
			receive_bytes = recvfrom(sock, buf, PACKETSIZE, 0, (sockaddr *)&client, &client_len);
			if (receive_bytes == SO_ERROR) {
				print_console_message("ERROR receiving from client");
				cout << "\t\t\tError num: " << errno << endl;
				usleep(500000);
			} else {
				print_console_message("RECEIVE OK");
				cout << "\t\t\tMessage size " << receive_bytes << endl;
				break;
			}
		}

		// CLIENT INFO
		memset(clientIp, 0, sizeof(clientIp));
		inet_ntop(AF_INET, &client.sin_addr, clientIp, 256);

		// PARSING RECIEVED PACKET
		msg_packet_data_len = receive_bytes - HEADERSIZE;
		deserialize_packet((unsigned char *)buf, &msg_packet, msg_packet_data_len);

		memset(id_file, 0, 8);
		memcpy(id_file, msg_packet.id, 8);
		uint8_t8_to_ullint(id_file, &idfile);;
		uint8_t8_to_char8(id_file, idfile_s);

		if (datamap.find(idfile) == datamap.end()) {
			db = make_new_datablocks(msg_packet.seq_total, DATASIZE, DATASIZE);
			datamap[idfile] = db;
		}

		if (msg_packet.seq_number != datamap[idfile].order[datamap[idfile].processed_num-1]) {
			// IF RECIVED MESSAGE CONTAINS NOT PREVIOUS BLOCK
			datamap[idfile].processed_num++;
			memcpy(datamap[idfile].blocks[msg_packet.seq_number-1], msg_packet.data, datamap[idfile].block_size);
			datamap[idfile].order[datamap[idfile].processed_num-1] = msg_packet.seq_number;
		}	

		if (msg_packet_data_len != DATASIZE) {
			// SERVER RECIEVED PACKET WITH LAST BLOCK
			datamap[idfile].lastblock_size = msg_packet_data_len;
		}

		// PRINT ABOUT PACKET
		cout << "\t\t\tMessage recieved from " << clientIp << endl;
		print_packet_info(&msg_packet);

		// MAKE PACKET TO SEND MSG
		msg_packet.type = 0;
		if ((unsigned int)datamap[idfile].processed_num == msg_packet.seq_total) {
			// RECIEVED LAST BLOCK
			if (msg_packet.seq_number != datamap[idfile].order[datamap[idfile].processed_num-2]) {
				// CALCULATE CRC, PRINT INFO AND SAVE FILE ONLY ONE TIME AT FIRST RECIEVING
				datamap[idfile].crc = crc32c_datablock(&datamap[idfile]);

				print_console_message("INFO");
				cout << "\t\t\tFile with id " << idfile_s << " completely recieved" << endl;
				cout << "\t\t\tCRC32C: " << datamap[idfile].crc << endl;

				char file_name[13] = "out/00000000";
				memcpy(file_name + 4, idfile_s, 8);
				save_datablocks_to_file((const char *)file_name, &datamap[idfile]);
			}

			msg_packet.seq_total = datamap[idfile].processed_num;
			memset(msg_packet.data, 0, DATASIZE);
			//datamap[idfile].crc = crc32c_datablock(&datamap[idfile]);

			unsigned int checksumserver;
			uint8_t checksum_server[4];
			checksumserver = (unsigned int)datamap[idfile].crc;
			uint_to_uint8_t4(checksum_server, &checksumserver);
			memcpy(msg_packet.data, checksum_server, 4);

			msg_packet_data_len = (int)sizeof(uint32_t);
			
		} else {
			msg_packet.seq_total = datamap[idfile].processed_num;
			memset(msg_packet.data, 0, DATASIZE);

			msg_packet_data_len = 0;
		}

		free(msg);
		msg = (unsigned char *)malloc((HEADERSIZE + msg_packet_data_len)*sizeof(char));
		serialize_packet(msg, &msg_packet, msg_packet_data_len);

		// SEND MSG
		while (true) {
			print_console_message("SENDING");
			send_bytes = sendto(sock, (char *)msg, HEADERSIZE + msg_packet_data_len, 0, (sockaddr *)&client, sizeof(client));
			if (send_bytes == SO_ERROR) {
				print_console_message("ERROR sending to client");
				cout << "\t\t\tError num: " << errno << endl;
				usleep(500000);
			} else {
				print_console_message("SEND OK");
				cout << "\t\t\tMessage sended to " << clientIp << endl;
				cout << "\t\t\tMessage size " << send_bytes << endl;
				print_packet_info(&msg_packet);
				break;
			}
		}
	}

	print_console_message("SERVER FINISHED");

	close(sock);
	
	return 0;
}