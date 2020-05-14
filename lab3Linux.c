#include <stdlib.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <wait.h>
#include <semaphore.h>
#include <string.h>
#include <math.h>

using namespace std;

union semun
{
    int              val;    /* Value for SETVAL */ 
    
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short  *array;  /* Array for GETALL, SETALL */
    struct seminfo  *__buf;  /* Buffer for IPC_INFO
(Linux-specific) */
} semunion;

void WaitSemaphore(int sem_id, int num);
void ReleaseSemaphore(int sem_id, int num);


int main(int argc, char *argv[])
{
    int bufferSize = 20; // Размер буфера
    int shm_id = shmget                         // Создание сегмента разделяемой памяти
                        (IPC_PRIVATE,  //уникальный ключ
                        bufferSize,    //размер выделяемой области в байтах
                        IPC_CREAT|0666); //флаг доступа

    if (shm_id < 0)
    {
        printf("shmget error\n"); //если случилась ошибка
        return(0);
    }

    int NumberOfBlocks; // Количество блоков размером с буфер
    int length;	      // Длина передаваемой строки

    system("clear");
    string message; //наше сообщение

    // Создание набора семафоров
    int sem_id = semget(IPC_PRIVATE, 4, IPC_CREAT|0666);
    /*Для расширенных операций над сегментом разделяемой памяти используется функция shmctl(). 
    В спектре ее возможностей получение и установка параметров сегмента, его удаление, а также блокирование/разблокирование*/
    semctl(sem_id, 
           0, 
           SETALL, 
           0);
    if(sem_id < 0)
    {
        printf("Semaphores is not created.");
        return 0;
    }

    int pid = fork(); //создание процесса
    switch(pid)
    {
    case -1: //если произошла ошибка
        printf("New process is not created\n");
        return 0;

    case 0: // Дочерний процесс
        {
            /*Функция shmat подстыковывает сегмент разделяемой памяти shmid к адресному пространству вызывающего процесса.*/
            void *buffer = shmat(shm_id, NULL, 0); //shmaddr равен NULL - значит система выбирает
                                                    //для подстыкованного сегмента подходящий (неиспользованный) адрес.

            while(1)
            {
                message.clear();
                WaitSemaphore(sem_id, 2); // Ожидание, пока счётчик семафора не станет равным 1
                WaitSemaphore(sem_id, 0);// и сброс в 0 по окончании ожидания
                length = *(int*)buffer;
                ReleaseSemaphore(sem_id, 1);

                if(length == -1) break;  // Выход из программы
//Функция ceil() возвращает наименьшее целое (представленное как double), которое не меньше чем num
                NumberOfBlocks = ceil((double)length / (double)bufferSize); // Количество блоков размером с буфер
                for( int i=0; i < NumberOfBlocks; i++)
                {
                    WaitSemaphore(sem_id, 0);
                    message.append((char*)buffer, bufferSize); // Добавляем полученный буфер в строку
                    ReleaseSemaphore(sem_id, 1);
                }

                message.resize(length);
                cout << "\nChild process:\n";

                for(int i = 0; i < length; i++) //посимвольный вывод
                {
                    putchar(message[i]);
                    fflush(stdout);
                    usleep(100000); //в мк секундах
                }
                cout<<endl<<endl;

                ReleaseSemaphore(sem_id, 3); //окончание работы
            }


        }
        break;

    default: // Родительский процесс
        {
            void *buffer = shmat(shm_id, NULL, 0);
            while(1)
            {
                message.clear();
                cout<<"Parent process:\n";
                system("stty echo");//отображение вводимых символов
                tcflush(STDIN_FILENO, TCIFLUSH);//очистка буфера
                getline(cin, message);
                system("stty -echo");

                ReleaseSemaphore(sem_id, 2);	// уведомление о готовности

                if(message == "quit")
                {
                    length = -1; //уведомляем о завершении работы
                    *(int*)buffer = length;
                    ReleaseSemaphore(sem_id, 0);
                    WaitSemaphore(sem_id, 1);    // Ожидаем получения сообщения
                    waitpid(pid, NULL, 0);
                    break;
                }

                length = message.size();
                *(int*)buffer = length;

                ReleaseSemaphore(sem_id, 0);	// сообщаем о передаче сообщения
                WaitSemaphore(sem_id, 1);    // Ожидаем получения сообщения

                NumberOfBlocks = ceil((double)length / (double)bufferSize); // Количество блоков размером с буфер
                for(int i = 0; i < NumberOfBlocks; i++)
                {
                    message.copy((char*)buffer, bufferSize, i*bufferSize);	// Заполняем буфер

                    ReleaseSemaphore(sem_id, 0);	// сообщаем о передаче сообщения
                    WaitSemaphore(sem_id, 1);    // Ожидаем получения сообщения
                }

                WaitSemaphore(sem_id, 3);  // Ожидание, пока счётчик семафора не станет равным 1
                // и сброс в 0 по окончании ожидания
            }

            semctl(sem_id, 0, IPC_RMID, semunion); // Удаляем набор семафоров
        }
        break;
    }

    system("clear");
    system("stty echo");
    return 0;
}


// Ожидание, пока счётчик семафора не станет равным 1
// и сброс в 0 по окончании ожидания
void WaitSemaphore(int sem_id, int num)
{
    struct sembuf buf;
    buf.sem_op = -1;
    buf.sem_flg = SEM_UNDO;
    buf.sem_num = num;
    semop(sem_id, &buf, 1);
}

// Установить счётчик семафора в 1
void ReleaseSemaphore(int sem_id, int num)
{
    struct sembuf buf;
    buf.sem_op = 1;
    buf.sem_flg = SEM_UNDO;
    buf.sem_num = num;
    semop(sem_id, &buf, 1);
}
