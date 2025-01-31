#include "common.h"
#include <process.h> // ��Ƽ������ ���� �Լ� ���
#include <time.h> // ���� �õ� ������ ���� time �Լ� ���

#pragma comment(lib, "ws2_32.lib") // Windows ���� ���̺귯�� ��ũ

// ���� ���� ���
#define PORT 8888 // ���� ��Ʈ ��ȣ
#define MAX_CLIENTS 5 // �ִ� Ŭ���̾�Ʈ ��
#define BUFFER_SIZE 1024 // �޽��� ���� ũ��
#define MAX_AUCTIONS 5 // �ִ� ��� ������ ��

#define MAX_HISTORY 5

// ===== ��� ������ ����ü =====
typedef struct {
	char name[50]; // ������ �̸�
	int current_price; // ���� ������
	int instant_price; // ��� ������
	char highest_bidder[30]; // �ְ� ������ �̸�
	int is_active; // ��� Ȱ��ȭ ����
} AuctionItem;


// ===== ��� ��� ����ü =====
typedef struct {
	char item_name[50]; // ������ �̸�
	char winner[30]; // ������ �̸�
	int final_price; // ���� ������

} AuctionResult;

// ===== Ŭ���̾�Ʈ ���� ����ü =====
typedef struct {
	SOCKET socket; // Ŭ���̾�Ʈ ����
	char nickname[30]; // Ŭ���̾�Ʈ �г���
	int money; // ���� �ݾ�
	int history_count;
	AuctionResult history[MAX_HISTORY];
} Client;



// ===== ���� ���� =====
AuctionItem current_auction;
Client clients[MAX_CLIENTS];
AuctionResult auction_results[MAX_AUCTIONS];
int client_count = 0;
int auction_count = 0;
CRITICAL_SECTION cs;

// ===== ������ ��� ����Ʈ =====
const char* adjectives[] = {
"�߾���", "���ִ�", "�ź�ο�", "ȭ����", "������", "�����", "��¦�̴�"
};
const char* nouns[] = {
"���ٵ�����ġ", "�������ǻ�", "õ�⹬�����״����", "�����������ٲ�",
"�̹����ǿ�����", "õ�⹬���縻", "���칫�����", "����Ȳ���������",
"�����ż��Ǿ˲���", "���Ľ���"
};

// ===== �Լ� ���� =====
void generateItemName(char* item_name);
void initializeAuctionItem(AuctionItem* item);
void saveAuctionResult(AuctionItem* item);
void printAuctionSummary();
void broadcast_message(char* message, SOCKET sender_socket);
unsigned __stdcall handle_client(void* arg);
void processMoneyCommand(SOCKET client_socket, char* nickname);
void checkAuctionTimer(time_t* end_time);

// ������ ��������
void saveClientHistory(const char* nickname, AuctionResult* result) {


	for (int i = 0; i < client_count; i++) {
		if (strcmp(clients[i].nickname, nickname) == 0) {
			if (clients[i].history_count < MAX_HISTORY) {
				clients[i].history[clients[i].history_count++] = *result;
			}
			else {
				// ������ ����� �о�� ����
				for (int j = 1; j < MAX_HISTORY; j++) {
					clients[i].history[j - 1] = clients[i].history[j];
				}
				clients[i].history[MAX_HISTORY - 1] = *result;
			}
			break;
		}
	}
}


// ===== ���� ������ �̸� ���� �Լ� =====
void generateItemName(char* item_name) {
	const char* adjective = adjectives[rand() % (sizeof(adjectives) / sizeof(adjectives[0]))];
	const char* noun = nouns[rand() % (sizeof(nouns) / sizeof(nouns[0]))];
	sprintf(item_name, "%s %s", adjective, noun);
}

// ===== ������ �ʱ�ȭ �Լ� =====
void initializeAuctionItem(AuctionItem* item) {
	generateItemName(item->name);
	item->current_price = (rand() % 91 + 10) * 1000; // �������� �ʸ���
	item->instant_price = item->current_price * 1.5;
	strcpy(item->highest_bidder, "����");
	item->is_active = 1;
}

// ===== ��� ��� ���� �Լ� =====
void saveAuctionResult(AuctionItem* item) {
	strcpy(auction_results[auction_count].item_name, item->name);
	strcpy(auction_results[auction_count].winner, item->highest_bidder);
	auction_results[auction_count].final_price = item->current_price;

	// �������� ���� �ݾ׿��� ���� �ݾ� ����
	for (int i = 0; i < client_count; i++) {
		if (strcmp(clients[i].nickname, item->highest_bidder) == 0) {
			clients[i].money -= item->current_price;

			// ���� ���� ����
			saveClientHistory(clients[i].nickname, &auction_results[auction_count]);

			break;
		}
	}

	auction_count++;
}

// ===== ��� ���� ��� ��� �Լ� =====
void printAuctionSummary() {
	printf("\n===== ��� ���� ��� =====\n");
	for (int i = 0; i < auction_count; i++) {
		printf("������: %s\n", auction_results[i].item_name);
		printf("������: %s\n", auction_results[i].winner);
		printf("������: %d��\n\n", auction_results[i].final_price);
	}
}

// ===== ��ε�ĳ��Ʈ �޽��� �Լ� =====
void broadcast_message(char* message, SOCKET sender_socket) {
	EnterCriticalSection(&cs);
	for (int i = 0; i < client_count; i++) {
		if (clients[i].socket != sender_socket) {
			send(clients[i].socket, message, strlen(message), 0);
		}
	}
	LeaveCriticalSection(&cs);
}



// ��� Ÿ�̸� �Լ�
void checkAuctionTimer(time_t* end_time) {
	EnterCriticalSection(&cs);

	// Ÿ�̸� �ߴ� ���� Ȯ��
	if (!current_auction.is_active) {
		LeaveCriticalSection(&cs);
		return; // ��� ���� �Ǵ� ���� ó�� ��
	}

	time_t now = time(NULL); // ���� �ð�



	if (now >= *end_time) {
		// ��� ���� ó��
		current_auction.is_active = 0;
		saveAuctionResult(&current_auction);

		char message[BUFFER_SIZE];
		snprintf(message, sizeof(message), "��� ����! ������: %s, ������: %d��",
			current_auction.highest_bidder, current_auction.current_price);
		broadcast_message(message, INVALID_SOCKET);

		// ���� ��� �غ�
		snprintf(message, sizeof(message), "���� ��� �غ� ���Դϴ�. ��ø� ��ٷ��ּ���...");
		broadcast_message(message, INVALID_SOCKET);

		Sleep(5000); // 5�� ���
		initializeAuctionItem(&current_auction);

		snprintf(message, sizeof(message),
			"���ο� ��Ű� ���۵Ǿ����ϴ�! ������: %s, ���۰�: %d��, ��� ������: %d��",
			current_auction.name, current_auction.current_price, current_auction.instant_price);
		broadcast_message(message, INVALID_SOCKET);

		// ���ο� ��� Ÿ�̸� ����
		*end_time = time(NULL) + 40; // 40�ʷ� �ʱ�ȭ
	}

	LeaveCriticalSection(&cs);
}

// ���� �ð� ��� �Լ�
void sendRemainingTimeToClients(time_t end_time) {
	EnterCriticalSection(&cs);

	if (!current_auction.is_active) {
		LeaveCriticalSection(&cs);
		return; // ��Ű� Ȱ�� ���°� �ƴϸ� ��ȯ
	}

	time_t now = time(NULL);
	int remaining_time = (int)(end_time - now); // ���� �ð� ���

	if (remaining_time < 0) remaining_time = 0;

	char message[BUFFER_SIZE];
	snprintf(message, sizeof(message), "���� ��� �ð�: %d��", remaining_time);

	broadcast_message(message, INVALID_SOCKET); // ��� Ŭ���̾�Ʈ���� ��ε�ĳ��Ʈ

	LeaveCriticalSection(&cs);
}


// ��� ���� ó�� �Լ�
void ProcessBid2(SOCKET client_socket, char* nickname, int bidAmount) {
	EnterCriticalSection(&cs);

	// ��� ������ ��ȿ�� Ȯ��
	if (!current_auction.is_active) {
		char errorMessage[BUFFER_SIZE];
		snprintf(errorMessage, sizeof(errorMessage), "��� ���� ����! ���� ���� ���� ��Ű� �����ϴ�.\n");
		send(client_socket, errorMessage, strlen(errorMessage), 0);
		LeaveCriticalSection(&cs);
		return;
	}

	// ��� ���� ó��
	if (bidAmount >= current_auction.instant_price) {
		current_auction.current_price = bidAmount;
		snprintf(current_auction.highest_bidder, sizeof(current_auction.highest_bidder), "%s", nickname);



		char successMessage[BUFFER_SIZE];
		snprintf(successMessage, sizeof(successMessage),
			"[��� ����] %s���� '%s'�� %d������ ��� �����Ǿ����ϴ�.",
			nickname, current_auction.name, bidAmount);
		broadcast_message(successMessage, INVALID_SOCKET);

		// ��� ����
		current_auction.is_active = 0;
		saveAuctionResult(&current_auction);

		// ���� ��� �غ� �޽���
		char nextMessage[BUFFER_SIZE];
		snprintf(nextMessage, sizeof(nextMessage), "���� ��� �غ� ���Դϴ�. ��ø� ��ٷ��ּ���...");
		broadcast_message(nextMessage, INVALID_SOCKET);

		LeaveCriticalSection(&cs);

		// 5�� ��� �� ���� ��� ����
		Sleep(5000);
		EnterCriticalSection(&cs);
		initializeAuctionItem(&current_auction);

		char newAuctionMessage[BUFFER_SIZE];
		snprintf(newAuctionMessage, sizeof(newAuctionMessage),
			"���ο� ��Ű� ���۵Ǿ����ϴ�! ������: %s, ���۰�: %d��, ��� ������: %d��",
			current_auction.name, current_auction.current_price, current_auction.instant_price);
		broadcast_message(newAuctionMessage, INVALID_SOCKET);
	}
	else {
		char errorMessage[BUFFER_SIZE];
		snprintf(errorMessage, sizeof(errorMessage),
			"[��� ���� ����] ��� �������� %d�� �̻��̾�� �մϴ�.",
			current_auction.instant_price);
		send(client_socket, errorMessage, strlen(errorMessage), 0);
	}

	LeaveCriticalSection(&cs);
}

//  /money ��ɾ� ó�� �Լ� 
void processMoneyCommand(SOCKET client_socket, char* nickname) {
	EnterCriticalSection(&cs);

	// Ŭ���̾�Ʈ�� �ڱ��� �˻�
	for (int i = 0; i < client_count; i++) {
		if (clients[i].socket == client_socket) {
			char moneyMessage[BUFFER_SIZE];
			snprintf(moneyMessage, sizeof(moneyMessage), "%s���� ���� �ڱ�: %d��", nickname, clients[i].money);
			send(client_socket, moneyMessage, strlen(moneyMessage), 0);
			LeaveCriticalSection(&cs);
			return;
		}
	}

	LeaveCriticalSection(&cs);
	char errorMessage[BUFFER_SIZE];
	snprintf(errorMessage, sizeof(errorMessage), "�ڱ� ������ ������ �� �����ϴ�.");
	send(client_socket, errorMessage, strlen(errorMessage), 0);
}




// ===== Ŭ���̾�Ʈ ó�� �Լ� =====
unsigned __stdcall handle_client(void* arg) {
	SOCKET client_socket = *(SOCKET*)arg;
	char buffer[BUFFER_SIZE];
	int recv_len;
	char nickname[30] = "";

	// Ŭ���̾�Ʈ �г��� ����
	recv_len = recv(client_socket, nickname, sizeof(nickname) - 1, 0);
	if (recv_len > 0) {
		nickname[recv_len] = '\0';
		EnterCriticalSection(&cs);
		strncpy(clients[client_count].nickname, nickname, 30);
		clients[client_count].socket = client_socket;
		clients[client_count].money = 300000; // 30���� �ʱ��ں�
		client_count++;
		LeaveCriticalSection(&cs);

		char welcome_msg[BUFFER_SIZE];
		sprintf(welcome_msg, "ȯ���մϴ�, %s��! ��� �ð��� 50���Դϴ�!! \n/help ��ɾ ���� ��ɾ �� �� �ֽ��ϴ�.", nickname);
		send(client_socket, welcome_msg, strlen(welcome_msg), 0);

		// ���� ��� ���� ����
		char auction_info[BUFFER_SIZE];
		sprintf(auction_info, "���� ��� ������: %s, ���� ����: %d��, ��� ������: %d��",
			current_auction.name, current_auction.current_price, current_auction.instant_price);
		send(client_socket, auction_info, strlen(auction_info), 0);
	}

	// ��� Ÿ�̸� �ʱ�ȭ
	time_t end_time = time(NULL) + 50; // 60�� Ÿ�̸�
	sendRemainingTimeToClients(end_time);

	// Ŭ���̾�Ʈ ��û ó�� ����
	while ((recv_len = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
		buffer[recv_len] = '\0';
		printf("%s�κ��� ����: %s\n", nickname, buffer);

		// ���� ��ɾ� ó��
		if (strncmp(buffer, "/bid", 4) == 0) {
			int bid_amount = atoi(buffer + 5);
			EnterCriticalSection(&cs);
			if (current_auction.is_active) {
				// Ŭ���̾�Ʈ �˻�
				for (int i = 0; i < client_count; i++) {
					if (clients[i].socket == client_socket) {
						// ���� �ݾ��� Ŭ���̾�Ʈ ���� �ݾ׺��� ���� ���
						if (bid_amount > clients[i].money) {
							char errorMessage[BUFFER_SIZE];
							snprintf(errorMessage, sizeof(errorMessage),
								"[���� ����] ���� �ݾ�(%d��)�� ���� �ݾ�(%d��)���� �����մϴ�.",
								clients[i].money, bid_amount);
							send(client_socket, errorMessage, strlen(errorMessage), 0);
							LeaveCriticalSection(&cs);
							return;
						}
						// ���� �ݾ��� ���� ���ݺ��� ������ ���� ó��
						if (bid_amount > current_auction.current_price) {
							current_auction.current_price = bid_amount;
							strncpy(current_auction.highest_bidder, clients[i].nickname, sizeof(current_auction.highest_bidder));

							// ��ε�ĳ��Ʈ �޽���
							char message[BUFFER_SIZE];
							snprintf(message, sizeof(message),
								"[���� ����] %s���� %d������ �����Ͽ����ϴ�.",
								clients[i].nickname, bid_amount);
							broadcast_message(message, INVALID_SOCKET);
						}
						else {
							// ���� �ݾ��� ���� ���ݺ��� ���ų� ���� ��
							char errorMessage[BUFFER_SIZE];
							snprintf(errorMessage, sizeof(errorMessage),
								"[���� ����] ���� �ݾ��� ���� ����(%d��)���� ���ƾ� �մϴ�.",
								current_auction.current_price);
							send(client_socket, errorMessage, strlen(errorMessage), 0);
						}
						break;
					}
				}
			}
			else {
				char errorMessage[BUFFER_SIZE];
				snprintf(errorMessage, sizeof(errorMessage), "���� ���� ���� ��Ű� �����ϴ�.");
				send(client_socket, errorMessage, strlen(errorMessage), 0);
			}

			LeaveCriticalSection(&cs);
		}
		// ���� ����
		else if (strcmp(buffer, "/status") == 0) {
			char status[BUFFER_SIZE];
			sprintf(status, "���� ��� ����: ������ - %s, ���� - %d��, �ְ� ������ - %s",
				current_auction.name, current_auction.current_price, current_auction.highest_bidder);
			send(client_socket, status, strlen(status), 0);
		}
		// ��� ����
		else if (strcmp(buffer, "/instant") == 0) {
			ProcessBid2(client_socket, nickname, current_auction.instant_price); // ��� ���� �Լ� ȣ��
		}
		// ���� ��
		else if (strcmp(buffer, "/money") == 0) {
			processMoneyCommand(client_socket, nickname); // /money ��ɾ� ó��
		}
		// ���� �ð� ����
		else if (strcmp(buffer, "/time") == 0) { // Ŭ���̾�Ʈ�� /time �Է�
			time_t now = time(NULL);
			int remaining_time = (int)(end_time - now);

			if (remaining_time < 0) remaining_time = 0;

			char time_message[BUFFER_SIZE];
			snprintf(time_message, sizeof(time_message), "���� ��� �ð�: %d��", remaining_time);
			send(client_socket, time_message, strlen(time_message), 0);
		}
		// ������ ��ɾ�
		else if (strcmp(buffer, "/history") == 0) {
			char history_message[BUFFER_SIZE];
			int client_index = -1;

			// Ŭ���̾�Ʈ �˻�
			for (int i = 0; i < client_count; i++) {
				if (strcmp(clients[i].nickname, nickname) == 0) {
					client_index = i;
					break;
				}
			}
			if (client_index != -1) {
				Client* client = &clients[client_index];
				snprintf(history_message, sizeof(history_message), "=== %s���� ���� ���� ===\n", nickname);
				if (client->history_count == 0) {
					strcat(history_message, "������ ��ǰ�� �����ϴ�.\n");
				}
				else {
					for (int i = 0; i < client->history_count; i++) {
						char item_info[BUFFER_SIZE];
						snprintf(item_info, sizeof(item_info), "%d. ������: %s, ������: %d��\n",
							i + 1, client->history[i].item_name, client->history[i].final_price);
						strcat(history_message, item_info);
					}
				}
			}
			else {
				snprintf(history_message, sizeof(history_message), "����� ������ ã�� �� �����ϴ�.\n");
			}

			send(client_socket, history_message, strlen(history_message), 0);
		}

		// ����
		else if (strcmp(buffer, "/help") == 0) {
			char help[BUFFER_SIZE];
			sprintf(help, "/bid: �������� �Է��մϴ�.\n/status: ���� ��� ������ ȣ���մϴ�.\n/instant: ������� ����� �����մϴ�.\n/history: ������ ��ǰ ����� �����ݴϴ�.\n/money: ���� �ڱ��� Ȯ���մϴ�.\n/time: ���� �ð��� Ȯ���մϴ�\n/leave: ��Ÿ� �����մϴ�.");
			send(client_socket, help, strlen(help), 0);
		}
		// ������
		else if (strcmp(buffer, "/leave") == 0) {
			closesocket(client_socket);
		}

		// ��� Ÿ�̸� Ȯ��
		Sleep(1000); // 1�� ���
		checkAuctionTimer(&end_time);

		// ��� ���� ���� Ȯ�� �� ��ε�ĳ��Ʈ
		EnterCriticalSection(&cs);
		if (!current_auction.is_active) {
			char message[BUFFER_SIZE];
			int remaining_money = 0;
			for (int i = 0; i < client_count; i++) {
				if (strcmp(clients[i].nickname, current_auction.highest_bidder) == 0) {
					remaining_money = clients[i].money;
					break;
				}
			}
			sprintf(message, "��Ű� ����Ǿ����ϴ�. ������: %s, ������: %d��, ���� �ݾ�: %d��",
				current_auction.highest_bidder, current_auction.current_price, remaining_money);
			broadcast_message(message, INVALID_SOCKET);
		}
		LeaveCriticalSection(&cs);
	}

	// Ŭ���̾�Ʈ ���� ���� ó��
	EnterCriticalSection(&cs);
	for (int i = 0; i < client_count; i++) {
		if (clients[i].socket == client_socket) {
			char leave_message[BUFFER_SIZE];
			sprintf(leave_message, "%s���� ������� �������ϴ�.", clients[i].nickname);
			broadcast_message(leave_message, client_socket);

			for (int j = i; j < client_count - 1; j++) {
				clients[j] = clients[j + 1];
			}
			client_count--;
			break;
		}
	}
	LeaveCriticalSection(&cs);

	closesocket(client_socket);
	return 0;
	//*

}


// ===== ���� �Լ� =====
int main() {
	WSADATA wsa;
	SOCKET server_socket, client_socket;
	struct sockaddr_in server, client;
	int c;

	srand((unsigned int)time(NULL)); // ���� �õ� ����

	// Winsock �ʱ�ȭ
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		printf("Winsock �ʱ�ȭ ����\n");
		return 1;
	}

	InitializeCriticalSection(&cs);

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == INVALID_SOCKET) {
		printf("���� ���� ����\n");
		return 1;
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PORT);

	if (bind(server_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
		printf("���ε� ����\n");
		return 1;
	}

	listen(server_socket, MAX_CLIENTS);

	printf("��� ������ ���۵Ǿ����ϴ�. ��Ʈ: %d\n", PORT);

	for (int i = 0; i < MAX_AUCTIONS; i++) {
		initializeAuctionItem(&current_auction);

		printf("\n���ο� �������� �����߽��ϴ�!\n");
		printf("������: %s\n", current_auction.name);
		printf("���۰�: %d��\n", current_auction.current_price);
		printf("��� ������: %d��\n\n", current_auction.instant_price);

		c = sizeof(struct sockaddr_in);

		while ((client_socket = accept(server_socket, (struct sockaddr*)&client, &c)) != INVALID_SOCKET) {
			printf("���ο� ����: %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

			HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, handle_client, (void*)&client_socket, 0, NULL);
			if (thread == NULL) {
				printf("������ ���� ����\n");
				closesocket(client_socket);
			}
			else {
				CloseHandle(thread);
			}
		}

		if (!current_auction.is_active) {
			saveAuctionResult(&current_auction);
		}
	}

	printAuctionSummary();

	closesocket(server_socket);
	WSACleanup();
	DeleteCriticalSection(&cs);

	return 0;
}