#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h> // inet_pton 사용을 위한 헤더

#pragma comment(lib, "ws2_32.lib")

#define PORT 8888

// 전역 플래그
volatile int running = 1;

// 서버로부터 실시간 업데이트 받기 위한 스레드 함수
DWORD WINAPI receiveUpdates(LPVOID clientSocket) {
	SOCKET sock = *(SOCKET*)clientSocket;
	char buffer[256];
	while (running) {
		int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
		if (bytesReceived > 0) {
			buffer[bytesReceived] = '\0';
			printf("\n<--업데이트-->\n%s\n", buffer);
		}
		else if (bytesReceived == 0 || WSAGetLastError() == WSAECONNRESET) {
			printf("서버 연결이 종료되었습니다.\n");
			running = 0;
			break;
		}
	}
	return 0;
}

int main() {
	WSADATA wsaData;
	SOCKET clientSocket;
	struct sockaddr_in serverAddress;
	char nickname[30];
	char auctionDetails[256];
	char input[256];

	// Winsock 초기화
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("Winsock 초기화 실패\n");
		return 1;
	}

	// 서버 소켓 생성
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSocket == INVALID_SOCKET) {
		printf("소켓 생성 실패\n");
		WSACleanup();
		return 1;
	}

	// 서버 주소 설정
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);

	// IP 주소 변환 및 설정
	if (inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) <= 0) {
		printf("IP 주소 변환 실패\n");
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	// 서버와 연결
	if (connect(clientSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
		printf("서버와의 연결에 실패했습니다.\n");
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	// 닉네임 등록
	printf("닉네임을 입력하세요: ");
	fgets(nickname, sizeof(nickname), stdin);
	nickname[strcspn(nickname, "\n")] = '\0'; // 개행 문자 제거
	send(clientSocket, nickname, strlen(nickname), 0);

	// 닉네임 등록에 대한 서버 응답 확인
	char nicknameResponse[256];
	int responseLength = recv(clientSocket, nicknameResponse, sizeof(nicknameResponse) - 1, 0);
	if (responseLength > 0) {
		nicknameResponse[responseLength] = '\0';
		printf("서버 응답: %s\n", nicknameResponse);
	}
	else {
		printf("닉네임 등록 실패 또는 서버 응답 없음.\n");
	}

	// 초기 경매 정보 수신
	int detailsReceived = recv(clientSocket, auctionDetails, sizeof(auctionDetails) - 1, 0);
	if (detailsReceived > 0) {
		auctionDetails[detailsReceived] = '\0';
		printf("초기 경매 정보 \n%s\n", auctionDetails);
	}
	else {
		printf("초기 경매 정보를 수신하는 데 실패했습니다.\n");
	}

	// 실시간 업데이트 받기 위한 스레드 시작
	HANDLE updateThread = CreateThread(NULL, 0, receiveUpdates, (LPVOID)&clientSocket, 0, NULL);
	if (updateThread == NULL) {
		printf("업데이트 스레드를 생성하는 데 실패했습니다.\n");
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	// 명령어 입력 및 처리 루프
	while (running) {
		printf("명령어를 입력하세요: ");
		fgets(input, sizeof(input), stdin);
		input[strcspn(input, "\n")] = '\0'; // 개행 문자 제거

		if (strncmp(input, "/bid", 4) == 0) {
			int bidAmount = atoi(input + 5);
			sprintf(input, "/bid %d", bidAmount);
			send(clientSocket, input, strlen(input), 0);
			printf("입찰 금액: %d\n", bidAmount);

		}
		else if (strcmp(input, "/status") == 0) {
			send(clientSocket, input, strlen(input), 0);

		}
		else if (strcmp(input, "/instant") == 0) {
			send(clientSocket, input, strlen(input), 0);

		}
		else if (strcmp(input, "/history") == 0)
		
		{
			send(clientSocket, input, strlen(input), 0);
		}
		else if (strcmp(input, "/time") == 0) {
			send(clientSocket, input, strlen(input), 0);
		}
		else if (strcmp(input, "/leave") == 0) {
			send(clientSocket, input, strlen(input), 0);
			running = 0;
		}
		
		else if (strcmp(input, "/help") == 0) {
			send(clientSocket, input, strlen(input), 0);
		}

		else if (strcmp(input, "/money") == 0) {
			send(clientSocket, input, strlen(input), 0);

		}
		
		else {
			printf("알 수 없는 명령어입니다. 다시 입력하세요.\n");
		}

	}

	// 스레드 종료 대기 및 자원 정리
	WaitForSingleObject(updateThread, INFINITE);
	CloseHandle(updateThread);

	printf("경매가 종료되었습니다. 연결을 종료합니다.\n");
	closesocket(clientSocket);
	WSACleanup();
	return 0;
}