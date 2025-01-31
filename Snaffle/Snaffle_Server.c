#include "common.h"
#include <process.h> // 멀티스레딩 관련 함수 사용
#include <time.h> // 랜덤 시드 설정을 위한 time 함수 사용

#pragma comment(lib, "ws2_32.lib") // Windows 소켓 라이브러리 링크

// 서버 설정 상수
#define PORT 8888 // 서버 포트 번호
#define MAX_CLIENTS 5 // 최대 클라이언트 수
#define BUFFER_SIZE 1024 // 메시지 버퍼 크기
#define MAX_AUCTIONS 5 // 최대 경매 아이템 수

#define MAX_HISTORY 5

// ===== 경매 아이템 구조체 =====
typedef struct {
	char name[50]; // 아이템 이름
	int current_price; // 현재 입찰가
	int instant_price; // 즉시 입찰가
	char highest_bidder[30]; // 최고 입찰자 이름
	int is_active; // 경매 활성화 상태
} AuctionItem;


// ===== 경매 결과 구조체 =====
typedef struct {
	char item_name[50]; // 아이템 이름
	char winner[30]; // 낙찰자 이름
	int final_price; // 최종 낙찰가

} AuctionResult;

// ===== 클라이언트 정보 구조체 =====
typedef struct {
	SOCKET socket; // 클라이언트 소켓
	char nickname[30]; // 클라이언트 닉네임
	int money; // 보유 금액
	int history_count;
	AuctionResult history[MAX_HISTORY];
} Client;



// ===== 전역 변수 =====
AuctionItem current_auction;
Client clients[MAX_CLIENTS];
AuctionResult auction_results[MAX_AUCTIONS];
int client_count = 0;
int auction_count = 0;
CRITICAL_SECTION cs;

// ===== 형용사와 명사 리스트 =====
const char* adjectives[] = {
"추억의", "맛있는", "신비로운", "화려한", "소중한", "희귀한", "반짝이는"
};
const char* nouns[] = {
"닌텐도스위치", "유니콘의뿔", "천년묵은지네담금주", "장수말벌을담근꿀",
"이무기의여의주", "천년묵은양말", "빗살무늬토기", "진시황제의콧수염",
"박혁거세의알껍질", "만파식적"
};

// ===== 함수 선언 =====
void generateItemName(char* item_name);
void initializeAuctionItem(AuctionItem* item);
void saveAuctionResult(AuctionItem* item);
void printAuctionSummary();
void broadcast_message(char* message, SOCKET sender_socket);
unsigned __stdcall handle_client(void* arg);
void processMoneyCommand(SOCKET client_socket, char* nickname);
void checkAuctionTimer(time_t* end_time);

// 아이템 꺼내오기
void saveClientHistory(const char* nickname, AuctionResult* result) {


	for (int i = 0; i < client_count; i++) {
		if (strcmp(clients[i].nickname, nickname) == 0) {
			if (clients[i].history_count < MAX_HISTORY) {
				clients[i].history[clients[i].history_count++] = *result;
			}
			else {
				// 오래된 기록을 밀어내고 저장
				for (int j = 1; j < MAX_HISTORY; j++) {
					clients[i].history[j - 1] = clients[i].history[j];
				}
				clients[i].history[MAX_HISTORY - 1] = *result;
			}
			break;
		}
	}
}


// ===== 랜덤 아이템 이름 생성 함수 =====
void generateItemName(char* item_name) {
	const char* adjective = adjectives[rand() % (sizeof(adjectives) / sizeof(adjectives[0]))];
	const char* noun = nouns[rand() % (sizeof(nouns) / sizeof(nouns[0]))];
	sprintf(item_name, "%s %s", adjective, noun);
}

// ===== 아이템 초기화 함수 =====
void initializeAuctionItem(AuctionItem* item) {
	generateItemName(item->name);
	item->current_price = (rand() % 91 + 10) * 1000; // 만원부터 십만원
	item->instant_price = item->current_price * 1.5;
	strcpy(item->highest_bidder, "없음");
	item->is_active = 1;
}

// ===== 경매 결과 저장 함수 =====
void saveAuctionResult(AuctionItem* item) {
	strcpy(auction_results[auction_count].item_name, item->name);
	strcpy(auction_results[auction_count].winner, item->highest_bidder);
	auction_results[auction_count].final_price = item->current_price;

	// 낙찰자의 보유 금액에서 낙찰 금액 차감
	for (int i = 0; i < client_count; i++) {
		if (strcmp(clients[i].nickname, item->highest_bidder) == 0) {
			clients[i].money -= item->current_price;

			// 낙찰 내역 저장
			saveClientHistory(clients[i].nickname, &auction_results[auction_count]);

			break;
		}
	}

	auction_count++;
}

// ===== 경매 종합 결과 출력 함수 =====
void printAuctionSummary() {
	printf("\n===== 경매 종합 결과 =====\n");
	for (int i = 0; i < auction_count; i++) {
		printf("아이템: %s\n", auction_results[i].item_name);
		printf("낙찰자: %s\n", auction_results[i].winner);
		printf("낙찰가: %d원\n\n", auction_results[i].final_price);
	}
}

// ===== 브로드캐스트 메시지 함수 =====
void broadcast_message(char* message, SOCKET sender_socket) {
	EnterCriticalSection(&cs);
	for (int i = 0; i < client_count; i++) {
		if (clients[i].socket != sender_socket) {
			send(clients[i].socket, message, strlen(message), 0);
		}
	}
	LeaveCriticalSection(&cs);
}



// 경매 타이머 함수
void checkAuctionTimer(time_t* end_time) {
	EnterCriticalSection(&cs);

	// 타이머 중단 여부 확인
	if (!current_auction.is_active) {
		LeaveCriticalSection(&cs);
		return; // 즉시 낙찰 또는 종료 처리 중
	}

	time_t now = time(NULL); // 현재 시간



	if (now >= *end_time) {
		// 경매 종료 처리
		current_auction.is_active = 0;
		saveAuctionResult(&current_auction);

		char message[BUFFER_SIZE];
		snprintf(message, sizeof(message), "경매 종료! 낙찰자: %s, 낙찰가: %d원",
			current_auction.highest_bidder, current_auction.current_price);
		broadcast_message(message, INVALID_SOCKET);

		// 다음 경매 준비
		snprintf(message, sizeof(message), "다음 경매 준비 중입니다. 잠시만 기다려주세요...");
		broadcast_message(message, INVALID_SOCKET);

		Sleep(5000); // 5초 대기
		initializeAuctionItem(&current_auction);

		snprintf(message, sizeof(message),
			"새로운 경매가 시작되었습니다! 아이템: %s, 시작가: %d원, 즉시 낙찰가: %d원",
			current_auction.name, current_auction.current_price, current_auction.instant_price);
		broadcast_message(message, INVALID_SOCKET);

		// 새로운 경매 타이머 설정
		*end_time = time(NULL) + 40; // 40초로 초기화
	}

	LeaveCriticalSection(&cs);
}

// 남은 시간 계산 함수
void sendRemainingTimeToClients(time_t end_time) {
	EnterCriticalSection(&cs);

	if (!current_auction.is_active) {
		LeaveCriticalSection(&cs);
		return; // 경매가 활성 상태가 아니면 반환
	}

	time_t now = time(NULL);
	int remaining_time = (int)(end_time - now); // 남은 시간 계산

	if (remaining_time < 0) remaining_time = 0;

	char message[BUFFER_SIZE];
	snprintf(message, sizeof(message), "남은 경매 시간: %d초", remaining_time);

	broadcast_message(message, INVALID_SOCKET); // 모든 클라이언트에게 브로드캐스트

	LeaveCriticalSection(&cs);
}


// 즉시 낙찰 처리 함수
void ProcessBid2(SOCKET client_socket, char* nickname, int bidAmount) {
	EnterCriticalSection(&cs);

	// 경매 아이템 유효성 확인
	if (!current_auction.is_active) {
		char errorMessage[BUFFER_SIZE];
		snprintf(errorMessage, sizeof(errorMessage), "즉시 낙찰 실패! 현재 진행 중인 경매가 없습니다.\n");
		send(client_socket, errorMessage, strlen(errorMessage), 0);
		LeaveCriticalSection(&cs);
		return;
	}

	// 즉시 낙찰 처리
	if (bidAmount >= current_auction.instant_price) {
		current_auction.current_price = bidAmount;
		snprintf(current_auction.highest_bidder, sizeof(current_auction.highest_bidder), "%s", nickname);



		char successMessage[BUFFER_SIZE];
		snprintf(successMessage, sizeof(successMessage),
			"[즉시 낙찰] %s님이 '%s'에 %d원으로 즉시 낙찰되었습니다.",
			nickname, current_auction.name, bidAmount);
		broadcast_message(successMessage, INVALID_SOCKET);

		// 경매 종료
		current_auction.is_active = 0;
		saveAuctionResult(&current_auction);

		// 다음 경매 준비 메시지
		char nextMessage[BUFFER_SIZE];
		snprintf(nextMessage, sizeof(nextMessage), "다음 경매 준비 중입니다. 잠시만 기다려주세요...");
		broadcast_message(nextMessage, INVALID_SOCKET);

		LeaveCriticalSection(&cs);

		// 5초 대기 후 다음 경매 시작
		Sleep(5000);
		EnterCriticalSection(&cs);
		initializeAuctionItem(&current_auction);

		char newAuctionMessage[BUFFER_SIZE];
		snprintf(newAuctionMessage, sizeof(newAuctionMessage),
			"새로운 경매가 시작되었습니다! 아이템: %s, 시작가: %d원, 즉시 낙찰가: %d원",
			current_auction.name, current_auction.current_price, current_auction.instant_price);
		broadcast_message(newAuctionMessage, INVALID_SOCKET);
	}
	else {
		char errorMessage[BUFFER_SIZE];
		snprintf(errorMessage, sizeof(errorMessage),
			"[즉시 낙찰 실패] 즉시 낙찰가는 %d원 이상이어야 합니다.",
			current_auction.instant_price);
		send(client_socket, errorMessage, strlen(errorMessage), 0);
	}

	LeaveCriticalSection(&cs);
}

//  /money 명령어 처리 함수 
void processMoneyCommand(SOCKET client_socket, char* nickname) {
	EnterCriticalSection(&cs);

	// 클라이언트의 자금을 검색
	for (int i = 0; i < client_count; i++) {
		if (clients[i].socket == client_socket) {
			char moneyMessage[BUFFER_SIZE];
			snprintf(moneyMessage, sizeof(moneyMessage), "%s님의 현재 자금: %d원", nickname, clients[i].money);
			send(client_socket, moneyMessage, strlen(moneyMessage), 0);
			LeaveCriticalSection(&cs);
			return;
		}
	}

	LeaveCriticalSection(&cs);
	char errorMessage[BUFFER_SIZE];
	snprintf(errorMessage, sizeof(errorMessage), "자금 정보를 가져올 수 없습니다.");
	send(client_socket, errorMessage, strlen(errorMessage), 0);
}




// ===== 클라이언트 처리 함수 =====
unsigned __stdcall handle_client(void* arg) {
	SOCKET client_socket = *(SOCKET*)arg;
	char buffer[BUFFER_SIZE];
	int recv_len;
	char nickname[30] = "";

	// 클라이언트 닉네임 수신
	recv_len = recv(client_socket, nickname, sizeof(nickname) - 1, 0);
	if (recv_len > 0) {
		nickname[recv_len] = '\0';
		EnterCriticalSection(&cs);
		strncpy(clients[client_count].nickname, nickname, 30);
		clients[client_count].socket = client_socket;
		clients[client_count].money = 300000; // 30만원 초기자본
		client_count++;
		LeaveCriticalSection(&cs);

		char welcome_msg[BUFFER_SIZE];
		sprintf(welcome_msg, "환영합니다, %s님! 경매 시간은 50초입니다!! \n/help 명령어를 통해 명령어를 볼 수 있습니다.", nickname);
		send(client_socket, welcome_msg, strlen(welcome_msg), 0);

		// 현재 경매 정보 전송
		char auction_info[BUFFER_SIZE];
		sprintf(auction_info, "현재 경매 아이템: %s, 현재 가격: %d원, 즉시 입찰가: %d원",
			current_auction.name, current_auction.current_price, current_auction.instant_price);
		send(client_socket, auction_info, strlen(auction_info), 0);
	}

	// 경매 타이머 초기화
	time_t end_time = time(NULL) + 50; // 60초 타이머
	sendRemainingTimeToClients(end_time);

	// 클라이언트 요청 처리 루프
	while ((recv_len = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
		buffer[recv_len] = '\0';
		printf("%s로부터 수신: %s\n", nickname, buffer);

		// 입찰 명령어 처리
		if (strncmp(buffer, "/bid", 4) == 0) {
			int bid_amount = atoi(buffer + 5);
			EnterCriticalSection(&cs);
			if (current_auction.is_active) {
				// 클라이언트 검색
				for (int i = 0; i < client_count; i++) {
					if (clients[i].socket == client_socket) {
						// 입찰 금액이 클라이언트 보유 금액보다 많은 경우
						if (bid_amount > clients[i].money) {
							char errorMessage[BUFFER_SIZE];
							snprintf(errorMessage, sizeof(errorMessage),
								"[입찰 실패] 보유 금액(%d원)이 입찰 금액(%d원)보다 부족합니다.",
								clients[i].money, bid_amount);
							send(client_socket, errorMessage, strlen(errorMessage), 0);
							LeaveCriticalSection(&cs);
							return;
						}
						// 입찰 금액이 현재 가격보다 높으면 입찰 처리
						if (bid_amount > current_auction.current_price) {
							current_auction.current_price = bid_amount;
							strncpy(current_auction.highest_bidder, clients[i].nickname, sizeof(current_auction.highest_bidder));

							// 브로드캐스트 메시지
							char message[BUFFER_SIZE];
							snprintf(message, sizeof(message),
								"[입찰 성공] %s님이 %d원으로 입찰하였습니다.",
								clients[i].nickname, bid_amount);
							broadcast_message(message, INVALID_SOCKET);
						}
						else {
							// 입찰 금액이 현재 가격보다 낮거나 같을 때
							char errorMessage[BUFFER_SIZE];
							snprintf(errorMessage, sizeof(errorMessage),
								"[입찰 실패] 입찰 금액은 현재 가격(%d원)보다 높아야 합니다.",
								current_auction.current_price);
							send(client_socket, errorMessage, strlen(errorMessage), 0);
						}
						break;
					}
				}
			}
			else {
				char errorMessage[BUFFER_SIZE];
				snprintf(errorMessage, sizeof(errorMessage), "현재 진행 중인 경매가 없습니다.");
				send(client_socket, errorMessage, strlen(errorMessage), 0);
			}

			LeaveCriticalSection(&cs);
		}
		// 상태 명렁어
		else if (strcmp(buffer, "/status") == 0) {
			char status[BUFFER_SIZE];
			sprintf(status, "현재 경매 상태: 아이템 - %s, 가격 - %d원, 최고 입찰자 - %s",
				current_auction.name, current_auction.current_price, current_auction.highest_bidder);
			send(client_socket, status, strlen(status), 0);
		}
		// 즉시 입찰
		else if (strcmp(buffer, "/instant") == 0) {
			ProcessBid2(client_socket, nickname, current_auction.instant_price); // 즉시 낙찰 함수 호출
		}
		// 보유 돈
		else if (strcmp(buffer, "/money") == 0) {
			processMoneyCommand(client_socket, nickname); // /money 명령어 처리
		}
		// 남는 시간 전송
		else if (strcmp(buffer, "/time") == 0) { // 클라이언트가 /time 입력
			time_t now = time(NULL);
			int remaining_time = (int)(end_time - now);

			if (remaining_time < 0) remaining_time = 0;

			char time_message[BUFFER_SIZE];
			snprintf(time_message, sizeof(time_message), "남은 경매 시간: %d초", remaining_time);
			send(client_socket, time_message, strlen(time_message), 0);
		}
		// 아이템 명령어
		else if (strcmp(buffer, "/history") == 0) {
			char history_message[BUFFER_SIZE];
			int client_index = -1;

			// 클라이언트 검색
			for (int i = 0; i < client_count; i++) {
				if (strcmp(clients[i].nickname, nickname) == 0) {
					client_index = i;
					break;
				}
			}
			if (client_index != -1) {
				Client* client = &clients[client_index];
				snprintf(history_message, sizeof(history_message), "=== %s님의 낙찰 내역 ===\n", nickname);
				if (client->history_count == 0) {
					strcat(history_message, "낙찰된 물품이 없습니다.\n");
				}
				else {
					for (int i = 0; i < client->history_count; i++) {
						char item_info[BUFFER_SIZE];
						snprintf(item_info, sizeof(item_info), "%d. 아이템: %s, 낙찰가: %d원\n",
							i + 1, client->history[i].item_name, client->history[i].final_price);
						strcat(history_message, item_info);
					}
				}
			}
			else {
				snprintf(history_message, sizeof(history_message), "사용자 정보를 찾을 수 없습니다.\n");
			}

			send(client_socket, history_message, strlen(history_message), 0);
		}

		// 도움말
		else if (strcmp(buffer, "/help") == 0) {
			char help[BUFFER_SIZE];
			sprintf(help, "/bid: 입찰가를 입력합니다.\n/status: 현재 경매 정보를 호출합니다.\n/instant: 즉시입찰 명령을 수행합니다.\n/history: 입찰한 물품 목록을 보여줍니다.\n/money: 현재 자금을 확인합니다.\n/time: 남은 시간을 확인합니다\n/leave: 경매를 종료합니다.");
			send(client_socket, help, strlen(help), 0);
		}
		// 떠나기
		else if (strcmp(buffer, "/leave") == 0) {
			closesocket(client_socket);
		}

		// 경매 타이머 확인
		Sleep(1000); // 1초 대기
		checkAuctionTimer(&end_time);

		// 경매 상태 변경 확인 및 브로드캐스트
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
			sprintf(message, "경매가 종료되었습니다. 낙찰자: %s, 낙찰가: %d원, 남은 금액: %d원",
				current_auction.highest_bidder, current_auction.current_price, remaining_money);
			broadcast_message(message, INVALID_SOCKET);
		}
		LeaveCriticalSection(&cs);
	}

	// 클라이언트 연결 종료 처리
	EnterCriticalSection(&cs);
	for (int i = 0; i < client_count; i++) {
		if (clients[i].socket == client_socket) {
			char leave_message[BUFFER_SIZE];
			sprintf(leave_message, "%s님이 경매장을 떠났습니다.", clients[i].nickname);
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


// ===== 메인 함수 =====
int main() {
	WSADATA wsa;
	SOCKET server_socket, client_socket;
	struct sockaddr_in server, client;
	int c;

	srand((unsigned int)time(NULL)); // 랜덤 시드 설정

	// Winsock 초기화
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		printf("Winsock 초기화 실패\n");
		return 1;
	}

	InitializeCriticalSection(&cs);

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == INVALID_SOCKET) {
		printf("소켓 생성 실패\n");
		return 1;
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(PORT);

	if (bind(server_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
		printf("바인딩 실패\n");
		return 1;
	}

	listen(server_socket, MAX_CLIENTS);

	printf("경매 서버가 시작되었습니다. 포트: %d\n", PORT);

	for (int i = 0; i < MAX_AUCTIONS; i++) {
		initializeAuctionItem(&current_auction);

		printf("\n새로운 아이템이 등장했습니다!\n");
		printf("아이템: %s\n", current_auction.name);
		printf("시작가: %d원\n", current_auction.current_price);
		printf("즉시 입찰가: %d원\n\n", current_auction.instant_price);

		c = sizeof(struct sockaddr_in);

		while ((client_socket = accept(server_socket, (struct sockaddr*)&client, &c)) != INVALID_SOCKET) {
			printf("새로운 연결: %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

			HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, handle_client, (void*)&client_socket, 0, NULL);
			if (thread == NULL) {
				printf("스레드 생성 실패\n");
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