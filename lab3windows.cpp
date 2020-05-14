#include<stdio.h>
#include<windows.h>
#include<conio.h>
#include<time.h>
#include<iostream>
#include<string>

using namespace std;

void Server(char* path)
{
	//Структура STARTUPINFO используется для работы с функциями CreateProcess,
			//CreateProcessAsUser, CreateProcessWithLogonW, чтобы определить оконный
			//терминал, рабочий стол и тд
			//для нового процесса.
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	// Структура PROCESS_INFORMATION заполняется функцией CreateProcess 
	//информацией о недавно созданном процессе в его первичном потоке.

	PROCESS_INFORMATION childProcessInfo;
	ZeroMemory(&childProcessInfo, sizeof(childProcessInfo));

	HANDLE hMyPipe;
	HANDLE Semaphores[3];

	char buffer[20];				 // Буфер для передачи
	int bufferSize = sizeof(buffer); // Размер буфера

	string message; //наше сообщение


	Semaphores[0] = CreateSemaphore(NULL, 0, 1, "SEMAPHORE_lab3");      // Семафор уведомяющий о разрешении печати
	Semaphores[1] = CreateSemaphore(NULL, 0, 1, "SEMAPHORE_end_lab3");      // Семафор уведомляющий о окончании печати
	Semaphores[2] = CreateSemaphore(NULL, 0, 1, "SEMAPHORE_EXIT_lab3");	// Семафор уведомляющий о завершении работы			

	cout << "Server process\n\n";

	hMyPipe = CreateNamedPipe("\\\\.\\pipe\\MyPipe",
		/*lpName имя канала. «\\Имя сервера\pipe\Имя канала При использовании данная строка сокращается до	 «\\.\pipe\Имя канала?*/
		PIPE_ACCESS_OUTBOUND, //режим открытия канала;

		PIPE_TYPE_MESSAGE | PIPE_WAIT, //режим работы канала
		PIPE_UNLIMITED_INSTANCES,//максимальное количество реализаций канала;
		0,//размер входного буфера
		0,//размер выходного буфера
		INFINITE,//Время ожидания
		(LPSECURITY_ATTRIBUTES)NULL); //атрибуты защиты

	CreateProcess(path, " 2", NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &childProcessInfo);
	/*
			NULL, // использует? имя моду?
			NULL, // Командная строка
			NULL, // Дескриптор процесса не наследуется.
			NULL, // Дескриптор потока не наследуется.
			FALSE, // Установк?описателей наследован?
			0, // Не?флагов создан? процесса
			NULL, // Блок переменных окружения родительског?процесса
			NULL, // Использовать текущи?катало?родительског?процесса
			&StartupInfo, // Указател?на структур?
			STARTUPINFO. &ProcInfo ) // Указател?на структур?информации ?процессе. )
			*/

	if (!ConnectNamedPipe(hMyPipe, (LPOVERLAPPED)NULL)) //если не удалось подключиться к каналу - ошибка
		cout << "Connection failure\n";

	while (1)
	{
		DWORD NumberOfWrittenBytes; //число записанных байтов

		cout << "\nEnter message:\n";
		cin.clear();
		getline(cin, message);

		if (message == "quit")
		{
			ReleaseSemaphore(Semaphores[2], 1, NULL);  // Сообщаем дочернему процессу о завершении работы
			WaitForSingleObject(childProcessInfo.hProcess, INFINITE);
			break;
		}

		ReleaseSemaphore(Semaphores[0],         // Сообщаем дочернему процессу о готовности начала передачи данных
			1,					//изменение счетчика
			NULL);			// предыдущее значение

		int NumberOfBlocks = message.size() / bufferSize + 1;	// Количество блоков размером с буфер		
		WriteFile(hMyPipe, //дескриптор файла
			&NumberOfBlocks,  //буфер данных
			sizeof(NumberOfBlocks), // Число байтов для записи
			&NumberOfWrittenBytes, //число записанных байтов
			(LPOVERLAPPED)NULL);

		int size = message.size();
		WriteFile(hMyPipe,  //дескриптор файла
			&size,  //буфер данных
			sizeof(size), // Число байтов для записи
			&NumberOfWrittenBytes, //число записанных байтов
			(LPOVERLAPPED)NULL); //асинхронный буфер

		for (int i = 0; i < NumberOfBlocks; i++)
		{
			message.copy(buffer, bufferSize, i*bufferSize);		// Заполняем буфер
			if (!WriteFile(hMyPipe, buffer, bufferSize, &NumberOfWrittenBytes, (LPOVERLAPPED)NULL)) cout << "Write Error\n";
		}

		WaitForSingleObject(Semaphores[1], INFINITE); // Ожидание, пока клиентский процесс не напечатает строку
	}

	CloseHandle(hMyPipe);
	CloseHandle(Semaphores[0]);
	CloseHandle(Semaphores[1]);
	cout << "\n\n";
	system("pause");
	return;
}

void Client()
{
	HANDLE hMyPipe;
	HANDLE Semaphores[3];
	bool successFlag;

	char buffer[20];				 // Буфер для передачи
	int bufferSize = sizeof(buffer); // Размер буфера

	string message; //сообщение

	Semaphores[0] = OpenSemaphore(SEMAPHORE_ALL_ACCESS, TRUE, "SEMAPHORE_lab3");
	Semaphores[1] = OpenSemaphore(SEMAPHORE_ALL_ACCESS, TRUE, "SEMAPHORE_end_lab3");
	Semaphores[2] = OpenSemaphore(SEMAPHORE_ALL_ACCESS, TRUE, "SEMAPHORE_EXIT_lab3");

	cout << "Child process\n\n";

	hMyPipe = CreateFile("\\\\.\\pipe\\MyPipe", GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);


	while (1)
	{
		successFlag = TRUE;
		DWORD NumberOfBytesRead; // число прочитанных байтов
		message.clear();

		int index = WaitForMultipleObjects(3, Semaphores, FALSE, INFINITE) - WAIT_OBJECT_0; // Получиние уведомления о возможности чтения семафора
		if (index == 2) // Если сигнальный семафор выхода
			break;

		int NumberOfBlocks;
		if (!ReadFile(hMyPipe, &NumberOfBlocks, sizeof(NumberOfBlocks), &NumberOfBytesRead, NULL)) break;

		int size;
		if (!ReadFile(
			hMyPipe,			// дескриптор файла
			&size,				 // буфер данных
			sizeof(size),		// число байтов для чтения
			&NumberOfBytesRead, // число прочитанных байтов
			NULL))				 // асинхронный буфер

			break;

		for (int i = 0; i < NumberOfBlocks; i++)
		{
			successFlag = ReadFile(hMyPipe, buffer, bufferSize, &NumberOfBytesRead, NULL);
			if (!successFlag) break;

			message.append(buffer, bufferSize); // Добавляем полученный буфер в строку
		}
		if (!successFlag) break;

		message.resize(size);

		for (int i = 0; i < size; i++) //посимвольный вывод сообщения
		{
			cout << message[i];
			Sleep(100);
		}
		cout << endl;

		ReleaseSemaphore(Semaphores[1], 1, NULL);		// Уведомление родительского процесса об успешном получении строки
	}
	CloseHandle(hMyPipe);
	CloseHandle(Semaphores[0]);
	CloseHandle(Semaphores[1]);
	return;
}

int main(int argc, char* argv[])
{
	switch (argc)
	{
	case 1:
		Server(argv[0]);
		break;

	default:
		Client();
		break;
	}
}
