#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BOARD_DIMENSION 15
#define MAX_PLAYERS 4
#define TOKENS_PER_PLAYER 4
#define HOME_PATH_LENGTH 5
#define BOARD_PATH_LENGTH 52

typedef struct {
    int x;
    int y;
} BoardPosition;

typedef struct {
    int posX;
    int posY;
    int startX;
    int startY;
    bool isInYard;
    bool isInHomePath;
    bool hasReachedHome;
} GameToken;

typedef struct {
    int playerID;
    const char *color;
    char symbol;
    GameToken tokens[TOKENS_PER_PLAYER];
    int killCount;
    int consecutiveSixes;
    bool isActive;
    bool isHuman;
} PlayerInfo;

typedef struct {
    int currentTurn;
    bool isGameOver;
    pthread_mutex_t mutexLock;
} GameStatus;

GameStatus gameStatus;
PlayerInfo playersList[MAX_PLAYERS];
BoardPosition boardPath[BOARD_PATH_LENGTH];
char gameBoard[BOARD_DIMENSION][BOARD_DIMENSION];
int playerRanks[MAX_PLAYERS] = {0};
int rankCounter = 1;
int activePlayerCount = MAX_PLAYERS;
int totalPlayerCount = MAX_PLAYERS;

const BoardPosition initialPositions[MAX_PLAYERS] = {
    {13, 6},
    {6, 1},
    {1, 8},
    {8, 13}
};

const BoardPosition homeEntries[MAX_PLAYERS] = {
    {8, 7},
    {6, 7},
    {7, 6},
    {7, 8}
};

const BoardPosition safeSpots[] = {
    {2, 6}, {1, 8}, {6, 12}, {8, 13},
    {6, 1}, {8, 2}, {13, 6}, {12, 8}
};

BoardPosition blueHomePath[HOME_PATH_LENGTH] = {
    {7, 1}, {7, 2}, {7, 3}, {7, 4}, {7, 5}
};

BoardPosition redHomePath[HOME_PATH_LENGTH] = {
    {13, 7}, {12, 7}, {11, 7}, {10, 7}, {9, 7}
};

BoardPosition greenHomePath[HOME_PATH_LENGTH] = {
    {1, 7}, {2, 7}, {3, 7}, {4, 7}, {5, 7}
};

BoardPosition yellowHomePath[HOME_PATH_LENGTH] = {
    {7, 13}, {7, 12}, {7, 11}, {7, 10}, {7, 9}
};

void initializeBoardPath();
void initializeBoard();
void setupPlayers();
void displayBoard();
void moveToken(PlayerInfo *player, int diceValue);

void clearScreen() {
    printf("\033[2J\033[H");
}

void waitForEnter() {
    int ch;
    printf("\nPress Enter to continue...");
    fflush(stdout);
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
}

int readIntInRange(const char *prompt, int minValue, int maxValue) {
    int value;
    int result;
    int ch;

    while (true) {
        printf("%s", prompt);
        fflush(stdout);
        result = scanf("%d", &value);
        while ((ch = getchar()) != '\n' && ch != EOF) {
        }

        if (result == EOF) {
            printf("\nInput closed. Exiting game setup.\n");
            exit(0);
        }

        if (result == 1 && value >= minValue && value <= maxValue) {
            return value;
        }

        printf("Please enter a number from %d to %d.\n", minValue, maxValue);
    }
}

int readPlayerCount() {
    int players;
    while (true) {
        players = readIntInRange("Select players (2 or 4): ", 2, 4);
        if (players == 2 || players == 4) {
            return players;
        }
        printf("Classic Ludo mode supports 2 or 4 players.\n");
    }
}

void printTitleScreen() {
    clearScreen();
    printf("==============================================================\n");
    printf("                 MULTITHREADED LUDO ENGINE\n");
    printf("==============================================================\n");
    printf(" POSIX threads | Mutex synchronization | Terminal gameplay\n");
    printf("--------------------------------------------------------------\n");
    printf(" Symbols: Blue=@  Red=#  Green=$  Yellow=%%  Safe=S  Home=*\n");
    printf(" Rules: roll six to release, safe cells block captures, three\n");
    printf(" consecutive sixes forfeit the turn, and first home wins rank.\n");
    printf("==============================================================\n\n");
}

int diceRoll() {
    return (rand() % 6) + 1;
}

bool samePosition(BoardPosition position, int x, int y) {
    return position.x == x && position.y == y;
}

bool isSafeSpot(int x, int y) {
    int totalSafeSpots = sizeof(safeSpots) / sizeof(safeSpots[0]);
    for (int i = 0; i < totalSafeSpots; i++) {
        if (samePosition(safeSpots[i], x, y)) {
            return true;
        }
    }
    return false;
}

char baseCellAt(int x, int y) {
    if (isSafeSpot(x, y)) {
        return 'S';
    }

    if (x >= 0 && x < 6 && y >= 0 && y < 6) {
        return 'R';
    }
    if (x >= 0 && x < 6 && y >= 9 && y < BOARD_DIMENSION) {
        return 'G';
    }
    if (x >= 9 && x < BOARD_DIMENSION && y >= 9 && y < BOARD_DIMENSION) {
        return 'Y';
    }
    if (x >= 9 && x < BOARD_DIMENSION && y >= 0 && y < 6) {
        return 'B';
    }
    if (x >= 6 && x <= 8 && y >= 6 && y <= 8) {
        return '*';
    }
    if (x == 7 && y > 0 && y < 14) {
        return '-';
    }
    if ((x > 0 && x < 6 && y == 7) || (x > 8 && x < 14 && y == 7)) {
        return '|';
    }

    return ' ';
}

BoardPosition *homePathForPlayer(int playerID) {
    switch (playerID) {
        case 1: return blueHomePath;
        case 2: return redHomePath;
        case 3: return greenHomePath;
        case 4: return yellowHomePath;
        default: return NULL;
    }
}

int pathIndexForPosition(int x, int y) {
    for (int i = 0; i < BOARD_PATH_LENGTH; i++) {
        if (boardPath[i].x == x && boardPath[i].y == y) {
            return i;
        }
    }
    return -1;
}

void placeToken(PlayerInfo *player, int x, int y) {
    gameBoard[x][y] = player->symbol;
}

void clearCell(int x, int y) {
    gameBoard[x][y] = baseCellAt(x, y);
}

void refreshBoard(PlayerInfo *player, int oldX, int oldY, int newX, int newY) {
    clearCell(oldX, oldY);
    placeToken(player, newX, newY);
}

void initializeBoardPath() {
    BoardPosition tempPath[BOARD_PATH_LENGTH] = {
        {0, 6}, {0, 7}, {0, 8}, {1, 8}, {2, 8}, {3, 8}, {4, 8}, {5, 8},
        {6, 9}, {6, 10}, {6, 11}, {6, 12}, {6, 13}, {6, 14}, {7, 14},
        {8, 14}, {8, 13}, {8, 12}, {8, 11}, {8, 10}, {8, 9}, {9, 8},
        {10, 8}, {11, 8}, {12, 8}, {13, 8}, {14, 8}, {14, 7}, {14, 6},
        {13, 6}, {12, 6}, {11, 6}, {10, 6}, {9, 6}, {8, 5}, {8, 4},
        {8, 3}, {8, 2}, {8, 1}, {8, 0}, {7, 0}, {6, 0}, {6, 1},
        {6, 2}, {6, 3}, {6, 4}, {6, 5}, {5, 6}, {4, 6}, {3, 6},
        {2, 6}, {1, 6}
    };
    memcpy(boardPath, tempPath, sizeof(tempPath));
}

void initializeBoard() {
    for (int i = 0; i < BOARD_DIMENSION; i++) {
        for (int j = 0; j < BOARD_DIMENSION; j++) {
            gameBoard[i][j] = baseCellAt(i, j);
        }
    }
}

void setupPlayers() {
    const char *colors[MAX_PLAYERS] = {"Blue", "Red", "Green", "Yellow"};
    const char symbols[MAX_PLAYERS] = {'@', '#', '$', '%'};
    const int yardPositions[MAX_PLAYERS][TOKENS_PER_PLAYER][2] = {
        {{11, 2}, {11, 3}, {12, 2}, {12, 3}},
        {{2, 2}, {2, 3}, {3, 2}, {3, 3}},
        {{2, 11}, {2, 12}, {3, 11}, {3, 12}},
        {{11, 11}, {11, 12}, {12, 11}, {12, 12}}
    };

    for (int i = 0; i < MAX_PLAYERS; i++) {
        playersList[i].playerID = i + 1;
        playersList[i].color = colors[i];
        playersList[i].symbol = symbols[i];
        playersList[i].killCount = 0;
        playersList[i].consecutiveSixes = 0;
        playersList[i].isActive = i < totalPlayerCount;
        playersList[i].isHuman = false;

        for (int j = 0; j < TOKENS_PER_PLAYER; j++) {
            playersList[i].tokens[j].posX = yardPositions[i][j][0];
            playersList[i].tokens[j].posY = yardPositions[i][j][1];
            playersList[i].tokens[j].startX = yardPositions[i][j][0];
            playersList[i].tokens[j].startY = yardPositions[i][j][1];
            playersList[i].tokens[j].isInYard = true;
            playersList[i].tokens[j].isInHomePath = false;
            playersList[i].tokens[j].hasReachedHome = false;
            if (playersList[i].isActive) {
                placeToken(&playersList[i], yardPositions[i][j][0], yardPositions[i][j][1]);
            }
        }
    }
}

void printColoredCell(char cell) {
    switch (cell) {
        case 'R': printf("\033[1;31m%2c \033[0m| ", cell); break;
        case 'G': printf("\033[1;32m%2c \033[0m| ", cell); break;
        case 'Y': printf("\033[1;33m%2c \033[0m| ", cell); break;
        case 'B': printf("\033[1;34m%2c \033[0m| ", cell); break;
        case 'S': printf("\033[1;37m%2c \033[0m| ", cell); break;
        case '*': printf("\033[1;35m%2c \033[0m| ", cell); break;
        case '-':
        case '|': printf("\033[1;36m%2c \033[0m| ", cell); break;
        case '@': printf("\033[1;34m%2c \033[0m| ", cell); break;
        case '#': printf("\033[1;31m%2c \033[0m| ", cell); break;
        case '$': printf("\033[1;32m%2c \033[0m| ", cell); break;
        case '%': printf("\033[1;33m%2c \033[0m| ", cell); break;
        default: printf("%2c | ", cell);
    }
}

void displayBoard() {
    printf("\n=== MULTITHREADED LUDO BOARD ===\n");
    printf("--------------------------------------------------------------\n");
    for (int i = 0; i < BOARD_DIMENSION; i++) {
        for (int j = 0; j < BOARD_DIMENSION; j++) {
            if (j == 0) {
                printf(" | ");
            }
            printColoredCell(gameBoard[i][j]);
        }
        printf("\n--------------------------------------------------------------\n");
    }
}

bool releaseToken(PlayerInfo *player) {
    for (int i = 0; i < TOKENS_PER_PLAYER; i++) {
        if (player->tokens[i].isInYard) {
            int oldX = player->tokens[i].posX;
            int oldY = player->tokens[i].posY;
            int startX = initialPositions[player->playerID - 1].x;
            int startY = initialPositions[player->playerID - 1].y;

            player->tokens[i].posX = startX;
            player->tokens[i].posY = startY;
            player->tokens[i].isInYard = false;

            refreshBoard(player, oldX, oldY, startX, startY);
            printf("Player %d released token %d to (%d, %d).\n", player->playerID, i + 1, startX, startY);
            return true;
        }
    }
    return false;
}

bool releaseSpecificToken(PlayerInfo *player, int tokenIdx) {
    if (tokenIdx < 0 || tokenIdx >= TOKENS_PER_PLAYER || !player->tokens[tokenIdx].isInYard) {
        return false;
    }

    int oldX = player->tokens[tokenIdx].posX;
    int oldY = player->tokens[tokenIdx].posY;
    int startX = initialPositions[player->playerID - 1].x;
    int startY = initialPositions[player->playerID - 1].y;

    player->tokens[tokenIdx].posX = startX;
    player->tokens[tokenIdx].posY = startY;
    player->tokens[tokenIdx].isInYard = false;

    refreshBoard(player, oldX, oldY, startX, startY);
    printf("Player %d released token %d to (%d, %d).\n", player->playerID, tokenIdx + 1, startX, startY);
    return true;
}

int homePathIndex(PlayerInfo *player, int tokenIdx) {
    BoardPosition *homePath = homePathForPlayer(player->playerID);
    if (homePath == NULL) {
        return -1;
    }

    for (int i = 0; i < HOME_PATH_LENGTH; i++) {
        if (player->tokens[tokenIdx].posX == homePath[i].x &&
            player->tokens[tokenIdx].posY == homePath[i].y) {
            return i;
        }
    }
    return -1;
}

bool advanceInHomePath(PlayerInfo *player, int tokenIdx, int diceValue) {
    int currentIndex = homePathIndex(player, tokenIdx);
    if (currentIndex == -1) {
        return false;
    }

    int newIndex = currentIndex + diceValue;
    if (newIndex >= HOME_PATH_LENGTH) {
        return false;
    }

    BoardPosition *homePath = homePathForPlayer(player->playerID);
    int oldX = player->tokens[tokenIdx].posX;
    int oldY = player->tokens[tokenIdx].posY;
    int newX = homePath[newIndex].x;
    int newY = homePath[newIndex].y;

    player->tokens[tokenIdx].posX = newX;
    player->tokens[tokenIdx].posY = newY;
    refreshBoard(player, oldX, oldY, newX, newY);

    printf("Player %d advanced token %d inside the home path.\n", player->playerID, tokenIdx + 1);

    if (newIndex == HOME_PATH_LENGTH - 1) {
        player->tokens[tokenIdx].hasReachedHome = true;
        clearCell(newX, newY);
        printf("Player %d token %d reached home.\n", player->playerID, tokenIdx + 1);
    }

    return true;
}

bool canEnterHomePath(PlayerInfo *player, int tokenIdx) {
    BoardPosition entry = homeEntries[player->playerID - 1];
    return player->tokens[tokenIdx].posX == entry.x && player->tokens[tokenIdx].posY == entry.y;
}

bool enterHomePath(PlayerInfo *player, int tokenIdx) {
    BoardPosition *homePath = homePathForPlayer(player->playerID);
    if (homePath == NULL) {
        return false;
    }

    int oldX = player->tokens[tokenIdx].posX;
    int oldY = player->tokens[tokenIdx].posY;

    player->tokens[tokenIdx].posX = homePath[0].x;
    player->tokens[tokenIdx].posY = homePath[0].y;
    player->tokens[tokenIdx].isInHomePath = true;

    refreshBoard(player, oldX, oldY, homePath[0].x, homePath[0].y);
    printf("Player %d token %d entered the home path.\n", player->playerID, tokenIdx + 1);
    return true;
}

bool eliminateOpponent(PlayerInfo *currentPlayer, int newX, int newY) {
    if (isSafeSpot(newX, newY)) {
        return false;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i == currentPlayer->playerID - 1) {
            continue;
        }

        for (int j = 0; j < TOKENS_PER_PLAYER; j++) {
            GameToken *token = &playersList[i].tokens[j];
            if (!token->isInYard && !token->hasReachedHome &&
                token->posX == newX && token->posY == newY) {
                clearCell(newX, newY);
                token->posX = token->startX;
                token->posY = token->startY;
                token->isInYard = true;
                token->isInHomePath = false;
                placeToken(&playersList[i], token->startX, token->startY);

                currentPlayer->killCount++;
                printf("Player %d eliminated Player %d token %d.\n",
                       currentPlayer->playerID, playersList[i].playerID, j + 1);
                return true;
            }
        }
    }
    return false;
}

int collectMovableTokens(PlayerInfo *player, int diceValue, int tokens[]) {
    int count = 0;

    for (int i = 0; i < TOKENS_PER_PLAYER; i++) {
        if (player->tokens[i].isInHomePath && !player->tokens[i].hasReachedHome) {
            int currentIndex = homePathIndex(player, i);
            if (currentIndex != -1 && currentIndex + diceValue < HOME_PATH_LENGTH) {
                tokens[count++] = i;
            }
        }
    }

    for (int i = 0; i < TOKENS_PER_PLAYER; i++) {
        if (!player->tokens[i].isInYard && !player->tokens[i].isInHomePath &&
            !player->tokens[i].hasReachedHome) {
            tokens[count++] = i;
        }
    }

    return count;
}

void printTokenChoices(PlayerInfo *player, int tokens[], int count) {
    printf("\nAvailable tokens:\n");
    for (int i = 0; i < count; i++) {
        GameToken token = player->tokens[tokens[i]];
        printf("  %d. Token %d at (%d, %d)%s\n",
               i + 1,
               tokens[i] + 1,
               token.posX,
               token.posY,
               token.isInHomePath ? " - home path" : "");
    }
}

void moveSelectedToken(PlayerInfo *player, int diceValue, int selectedToken) {
    if (player->tokens[selectedToken].isInHomePath) {
        advanceInHomePath(player, selectedToken, diceValue);
        return;
    }

    if (canEnterHomePath(player, selectedToken)) {
        enterHomePath(player, selectedToken);
        return;
    }

    int currentX = player->tokens[selectedToken].posX;
    int currentY = player->tokens[selectedToken].posY;
    int currentPathIndex = pathIndexForPosition(currentX, currentY);

    if (currentPathIndex == -1) {
        printf("Player %d token %d is not on the board path.\n", player->playerID, selectedToken + 1);
        return;
    }

    int newPathIndex = (currentPathIndex + diceValue) % BOARD_PATH_LENGTH;
    int newX = boardPath[newPathIndex].x;
    int newY = boardPath[newPathIndex].y;

    eliminateOpponent(player, newX, newY);

    player->tokens[selectedToken].posX = newX;
    player->tokens[selectedToken].posY = newY;
    refreshBoard(player, currentX, currentY, newX, newY);

    printf("Player %d moved token %d to (%d, %d). Kills: %d\n",
           player->playerID, selectedToken + 1, newX, newY, player->killCount);
}

void moveToken(PlayerInfo *player, int diceValue) {
    int movableTokens[TOKENS_PER_PLAYER];
    int movableCount = collectMovableTokens(player, diceValue, movableTokens);

    if (movableCount == 0) {
        printf("Player %d has no token available to move.\n", player->playerID);
        return;
    }

    int selectedToken = movableTokens[rand() % movableCount];
    moveSelectedToken(player, diceValue, selectedToken);
}

bool hasYardToken(PlayerInfo *player) {
    for (int i = 0; i < TOKENS_PER_PLAYER; i++) {
        if (player->tokens[i].isInYard) {
            return true;
        }
    }
    return false;
}

void handleHumanMove(PlayerInfo *player, int roll) {
    int movableTokens[TOKENS_PER_PLAYER];
    int movableCount = collectMovableTokens(player, roll, movableTokens);
    bool canRelease = roll == 6 && hasYardToken(player);

    if (!canRelease && movableCount == 0) {
        printf("No available move this turn.\n");
        return;
    }

    if (canRelease) {
        printf("\nYou rolled a six. Choose what to do:\n");
        printf("  1. Release a token from the yard\n");
        if (movableCount > 0) {
            printf("  2. Move a token already on the board\n");
        }

        int action = readIntInRange("Choice: ", 1, movableCount > 0 ? 2 : 1);
        if (action == 1) {
            int yardTokens[TOKENS_PER_PLAYER];
            int yardCount = 0;
            printf("\nTokens in yard:\n");
            for (int i = 0; i < TOKENS_PER_PLAYER; i++) {
                if (player->tokens[i].isInYard) {
                    yardTokens[yardCount++] = i;
                    printf("  %d. Token %d\n", yardCount, i + 1);
                }
            }

            int choice = readIntInRange("Release token: ", 1, yardCount);
            releaseSpecificToken(player, yardTokens[choice - 1]);
            return;
        }
    }

    printTokenChoices(player, movableTokens, movableCount);
    int choice = readIntInRange("Move token: ", 1, movableCount);
    moveSelectedToken(player, roll, movableTokens[choice - 1]);
}

bool processDiceRoll(PlayerInfo *player, int roll) {
    printf("\n--------------------------------------------------------------\n");
    printf("Player %d (%s, %s) turn\n",
           player->playerID,
           player->color,
           player->isHuman ? "Human" : "Computer");

    if (player->isHuman) {
        printf("Press Enter to roll the dice...");
        fflush(stdout);
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
        }
    } else {
        printf("Computer is rolling");
        fflush(stdout);
        for (int i = 0; i < 3; i++) {
            printf(".");
            fflush(stdout);
            usleep(250000);
        }
        printf("\n");
    }

    printf("Player %d rolled %d.\n", player->playerID, roll);

    if (roll == 6) {
        player->consecutiveSixes++;
        if (player->consecutiveSixes == 3) {
            printf("Player %d rolled three consecutive sixes and forfeits the turn.\n", player->playerID);
            player->consecutiveSixes = 0;
            return false;
        }

        if (player->isHuman) {
            handleHumanMove(player, roll);
        } else if (!releaseToken(player)) {
            moveToken(player, roll);
        }
        return true;
    }

    player->consecutiveSixes = 0;
    if (player->isHuman) {
        handleHumanMove(player, roll);
    } else {
        moveToken(player, roll);
    }
    return false;
}

int nextActivePlayer(int currentPlayerID) {
    for (int offset = 1; offset <= totalPlayerCount; offset++) {
        int nextID = ((currentPlayerID - 1 + offset) % totalPlayerCount) + 1;
        if (playersList[nextID - 1].isActive) {
            return nextID;
        }
    }
    return currentPlayerID;
}

void *playerRoutine(void *arg) {
    PlayerInfo *player = (PlayerInfo *)arg;

    while (!gameStatus.isGameOver) {
        pthread_mutex_lock(&gameStatus.mutexLock);

        if (gameStatus.currentTurn == player->playerID && player->isActive) {
            clearScreen();
            displayBoard();
            bool extraTurn = processDiceRoll(player, diceRoll());
            displayBoard();
            if (player->isHuman) {
                waitForEnter();
            }
            if (!extraTurn) {
                gameStatus.currentTurn = nextActivePlayer(player->playerID);
            }
        }

        pthread_mutex_unlock(&gameStatus.mutexLock);
        usleep(30000);
    }

    return NULL;
}

void *gameMonitor(void *arg) {
    (void)arg;

    while (!gameStatus.isGameOver) {
        pthread_mutex_lock(&gameStatus.mutexLock);

        for (int i = 0; i < totalPlayerCount; i++) {
            if (!playersList[i].isActive || playerRanks[i] != 0) {
                continue;
            }

            bool allTokensHome = true;
            for (int j = 0; j < TOKENS_PER_PLAYER; j++) {
                if (!playersList[i].tokens[j].hasReachedHome) {
                    allTokensHome = false;
                    break;
                }
            }

            if (allTokensHome) {
                playerRanks[i] = rankCounter++;
                playersList[i].isActive = false;
                activePlayerCount--;
                printf("Player %d finished with rank %d.\n", playersList[i].playerID, playerRanks[i]);
            }
        }

        if (activePlayerCount <= 1) {
            for (int i = 0; i < totalPlayerCount; i++) {
                if (playersList[i].isActive && playerRanks[i] == 0) {
                    playerRanks[i] = rankCounter++;
                    playersList[i].isActive = false;
                }
            }

            printf("\n=== GAME OVER ===\n");
            printf("Final rankings:\n");
            for (int rank = 1; rank <= totalPlayerCount; rank++) {
                for (int player = 0; player < totalPlayerCount; player++) {
                    if (playerRanks[player] == rank) {
                        printf("%d. Player %d (%s)\n", rank, playersList[player].playerID, playersList[player].color);
                    }
                }
            }

            printf("\nElimination counts:\n");
            for (int i = 0; i < totalPlayerCount; i++) {
                printf("Player %d: %d\n", playersList[i].playerID, playersList[i].killCount);
            }

            gameStatus.isGameOver = true;
        }

        pthread_mutex_unlock(&gameStatus.mutexLock);
        sleep(1);
    }

    return NULL;
}

int main() {
    srand((unsigned int)time(NULL));

    printTitleScreen();
    totalPlayerCount = readPlayerCount();
    int humanPlayers = readIntInRange("How many human players? ", 1, totalPlayerCount);
    activePlayerCount = totalPlayerCount;

    initializeBoardPath();
    initializeBoard();
    setupPlayers();

    for (int i = 0; i < humanPlayers; i++) {
        playersList[i].isHuman = true;
    }

    printf("\nGame setup complete:\n");
    for (int i = 0; i < totalPlayerCount; i++) {
        printf("  Player %d - %s - %s\n",
               playersList[i].playerID,
               playersList[i].color,
               playersList[i].isHuman ? "Human" : "Computer");
    }
    waitForEnter();

    pthread_t playerThreads[MAX_PLAYERS];
    pthread_t monitorThread;

    gameStatus.currentTurn = 1;
    gameStatus.isGameOver = false;
    pthread_mutex_init(&gameStatus.mutexLock, NULL);

    pthread_create(&monitorThread, NULL, gameMonitor, NULL);

    for (int i = 0; i < totalPlayerCount; i++) {
        pthread_create(&playerThreads[i], NULL, playerRoutine, &playersList[i]);
    }

    for (int i = 0; i < totalPlayerCount; i++) {
        pthread_join(playerThreads[i], NULL);
    }

    pthread_join(monitorThread, NULL);
    pthread_mutex_destroy(&gameStatus.mutexLock);

    return 0;
}
