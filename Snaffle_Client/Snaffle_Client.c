#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h> // inet_pton ����� ���� ���

#pragma comment(lib, "ws2_32.lib")

#define PORT 8888

// ���� �÷���
volatile int running = 1;

// �����κ��� �ǽð� ������Ʈ �ޱ� ���� ������ �Լ�
DWORD WINAPI receiveUpdates(LPVOID clientSocket) {
	SOCKET sock = *(SOCKET*)clientSocket;
	char buffer[256];
	while (running) {
		int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
		if (bytesReceived > 0) {
			buffer[bytesReceived] = '\0';
			printf("\n<--������Ʈ-->\n%s\n", buffer);
		}
		else if (bytesReceived == 0 || WSAGetLastError() == WSAECONNRESET) {
			printf("���� ������ ����Ǿ����ϴ�.\n");
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

	// Winsock �ʱ�ȭ
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("Winsock �ʱ�ȭ ����\n");
		return 1;
	}

	// ���� ���� ����
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSocket == INVALID_SOCKET) {
		printf("���� ���� ����\n");
		WSACleanup();
		return 1;
	}

	// ���� �ּ� ����
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);

	// IP �ּ� ��ȯ �� ����
	if (inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) <= 0) {
		printf("IP �ּ� ��ȯ ����\n");
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	// ������ ����
	if (connect(clientSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
		printf("�������� ���ῡ �����߽��ϴ�.\n");
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	// �г��� ���
	printf("�г����� �Է��ϼ���: ");
	fgets(nickname, sizeof(nickname), stdin);
	nickname[strcspn(nickname, "\n")] = '\0'; // ���� ���� ����
	send(clientSocket, nickname, strlen(nickname), 0);

	// �г��� ��Ͽ� ���� ���� ���� Ȯ��
	char nicknameResponse[256];
	int responseLength = recv(clientSocket, nicknameResponse, sizeof(nicknameResponse) - 1, 0);
	if (responseLength > 0) {
		nicknameResponse[responseLength] = '\0';
		printf("���� ����: %s\n", nicknameResponse);
	}
	else {
		printf("�г��� ��� ���� �Ǵ� ���� ���� ����.\n");
	}

	// �ʱ� ��� ���� ����
	int detailsReceived = recv(clientSocket, auctionDetails, sizeof(auctionDetails) - 1, 0);
	if (detailsReceived > 0) {
		auctionDetails[detailsReceived] = '\0';
		printf("�ʱ� ��� ���� \n%s\n", auctionDetails);
	}
	else {
		printf("�ʱ� ��� ������ �����ϴ� �� �����߽��ϴ�.\n");
	}

	// �ǽð� ������Ʈ �ޱ� ���� ������ ����
	HANDLE updateThread = CreateThread(NULL, 0, receiveUpdates, (LPVOID)&clientSocket, 0, NULL);
	if (updateThread == NULL) {
		printf("������Ʈ �����带 �����ϴ� �� �����߽��ϴ�.\n");
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	// ��ɾ� �Է� �� ó�� ����
	while (running) {
		printf("��ɾ �Է��ϼ���: ");
		fgets(input, sizeof(input), stdin);
		input[strcspn(input, "\n")] = '\0'; // ���� ���� ����

		if (strncmp(input, "/bid", 4) == 0) {
			int bidAmount = atoi(input + 5);
			sprintf(input, "/bid %d", bidAmount);
			send(clientSocket, input, strlen(input), 0);
			printf("���� �ݾ�: %d\n", bidAmount);

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
			printf("�� �� ���� ��ɾ��Դϴ�. �ٽ� �Է��ϼ���.\n");
		}

	}

	// ������ ���� ��� �� �ڿ� ����
	WaitForSingleObject(updateThread, INFINITE);
	CloseHandle(updateThread);

	printf("��Ű� ����Ǿ����ϴ�. ������ �����մϴ�.\n");
	closesocket(clientSocket);
	WSACleanup();
	return 0;
}