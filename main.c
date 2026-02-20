#include <stdio.h>
#include <unistd.h> //read

#include <fcntl.h> //open

//open file descriptor, fildes; read nbyte bytes 
// ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset);
//open is a system call that is used to open a new file and obtain its file descriptor

#define MAX_X 50
#define MAX_Y 50

int mezoEllenorzo(char array[MAX_X][MAX_Y], int ROVER_POS[], int lepes) {
    int sharpASCII = 35;

    //0,1,2,3,4,5,6,7,8
    //fel, le, jobbra, balra, felésjobb; felésbal; leésjobb; leésbal (valamilyért most max 7)

    int mozgasXtengely[] = {0, 0, 1, -1, -1, -1, 1, 1};
    int mozgasYtengely[] = {1, -1, 0, 0, -1, 1, -1, 1};
    
    int x = ROVER_POS[0] + mozgasXtengely[lepes];
    int y = ROVER_POS[1] + mozgasYtengely[lepes];


    if (x >= 0 && x < MAX_X && y >= 0 && y < MAX_Y) {
        printf("Létező mező.\n");
        if (array[x][y] != sharpASCII) {
            printf("Nincs blokk. Tudunk lépni");
                ROVER_POS[0] = x;
                ROVER_POS[1] = y;
                return 0;
        } else {
            printf("Blokk van! Nem lépünk!");
            return 1;
        }

    } else {
        printf("Hiba! Nincs ilyen mező\n");
        return 1;
    }

}

void Iranyitas(char array[MAX_X][MAX_Y], int ROVER_POS[]) {


    int lepes;
    printf("Rover poziciója mozgás előtt: %d:%d\n", ROVER_POS[0], ROVER_POS[1]);
    printf("Hova mozogjon a rover? (lásd a számokat)\n"); 
    scanf("%d", &lepes);

    // if (!mezoEllenorzo) {
    //     return 1;
        
    // }

}


#define BUFFER_SIZE 6000

int main() {


    char *FILENAME = "mars_map_50x50.csv";
    size_t filedesc = open(FILENAME, O_RDONLY);
    if (filedesc < 0) {
        printf("Failed reading in");
        return 1;
    } else {
        printf("Success read\n");

    }
    printf("%lu\n", filedesc);


    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while((bytes_read = read(filedesc, buffer, BUFFER_SIZE)) > 0) {
        printf("%i\n", bytes_read);

        
    }


    char array[50][50] = {0}; //tömb, a mezővel


    //Rover_POS; akku; nap, éj, áll, mozog, bányász, lassú, gyors, normál, elindulási idő 
    int ROVER_POS[2]; 
    int akku = 100;

    int adottIdoTartam;

    printf("Adja meg egy időtartamot (óra): \n");
    scanf("%d", &adottIdoTartam);

    int newLineCount = 0; //we count newlines and separate by them
    int row = 0;
    int column = 0;

    for (int i = 0; i < strlen(buffer); i++) {
        int commanHex = 44;
        int newlineHex = 10; //debug --> find if debugs are comprehended, if yes then use it to separate rows
        int S_Hex = 83;

        if (commanHex == buffer[i]) { //all commas deleted
            continue;
        }

        column += 1; //shift out array by 1 to the right

        if (newlineHex == buffer[i]) {
            newLineCount += 1;
            row += 1; //shift our row down
            column = 0; //set column again to zeroooooooo ;) 
            
        }

        if (S_Hex == buffer[i]) {

            ROVER_POS[0] = row;
            ROVER_POS[1] = column; 

        }
        char convertedBuff = buffer[i];
        array[row][column] = convertedBuff; //converts hex to the char
        
        //printf("%c\n", buffer[i]); 
       
       
    }

    printf("Rover Pozíciója: %d:%d\n", ROVER_POS[0], ROVER_POS[1]);
    printf("Aksi játek elején: %d\n", akku);
    printf("Adott idő: %d\n", adottIdoTartam);
    printf("Elindulási idő: 6:30\n");

    Iranyitas(array, ROVER_POS);
    printf("New position of the rover: %d:%d\n", ROVER_POS[0], ROVER_POS[1]);
    //Elindulási adatok:
    //printf("Newline count %d\n", newLineCount);

    // for (int i = 0; i < 50; i++) {
    //     for (int j = 0; j < 50; j++) {
    //         printf("%c", array[i][j]);
    //     }
    // }

    //User fogja íranyítani a robotot először:


    if (close(filedesc) < 0) {
        printf("Failed closing");
        return 1;
    } else {
        printf("Successful close\n");
        return 0;
    }

    return 0;
}