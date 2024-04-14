#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <csignal>
#include <ctime>
#include <csetjmp>

jmp_buf env;

// Функція f(x), яка може зациклитися
int f(int x) {
    sleep(1);
    return x * 2; // Приклад обчислення
}

// Функція g(x), що може зациклитися
int g(int x) {
    sleep(1);
    return x * 3; // Приклад обчислення
}

// Функція, яка буде викликана при отриманні сигналу SIGALRM
void handle_alarm(int sig) {
    char answer[3];
    do {
        printf("Function is taking too long. Do you want to continue? (yes/no)\n");
        scanf("%s", answer);
    } while (strcmp(answer, "yes") != 0 && strcmp(answer, "no") != 0);

    if (strcmp(answer, "yes") == 0) {
        alarm(1); // Reset the timer
        longjmp(env, 1); // Jump back to the setjmp point
    } else {
        printf("Exiting.\n");
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <integer value>\n", argv[0]);
        return -1;
    }

    int x = atoi(argv[1]);

    // Створення сокета
    int sockfd[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockfd) < 0) {
        perror("socketpair");
        exit(EXIT_FAILURE);
    }

    // Встановлення обробника сигналу
    struct sigaction sa{};
    sa.sa_handler = handle_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, nullptr);

    if (setjmp(env) == 0) {
        if (fork() == 0) {
            // Дочірній процес для f(x)
            close(sockfd[0]);
            alarm(2); // Встановлення таймера
            int result = f(x);
            write(sockfd[1], &result, sizeof(result));
            exit(0);
        }
    }

    if (setjmp(env) == 0) {
        if (fork() == 0) {
            // Дочірній процес для g(x)
            close(sockfd[0]);
            alarm(2); // Встановлення таймера
            int result = g(x);
            write(sockfd[1], &result, sizeof(result));
            exit(0);
        }
    }

    // Очікування результатів від обох процесів
    int results[2] = {0}, count = 0;
    while (count < 2) {
        ssize_t bytesRead = read(sockfd[0], &results[count], sizeof(int));
        if (bytesRead > 0) {
            count++;
        }
    }
    close(sockfd[1]);

    // Обчислення та виведення результату
    int result = results[0] && results[1];
    printf("Result: %d\n", result);

    return 0;
}