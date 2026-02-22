




#include <stdio.h>
#include <unistd.h> //read
#include <stdint.h> //int8_t
#include <fcntl.h> //open

//open file descriptor, fildes; read nbyte bytes 
// ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset);
//open is a system call that is used to open a new file and obtain its file descriptor

//szerintem az érceket el fogom menteni ide valahova, legyen globális és hozzáférhető adatbázisból
//lehet, hogy flagokat érdemes lenne bitesebben eltárolni
#define MAX_X 50
#define MAX_Y 50
#define BUFFER_SIZE 6000
#define RADIUS 8 //elég lesz
#define NEGYZET (RADIUS * 2 + 1)

#define sharpASCII 35
#define B_ASCII 66 //kék ásvány
#define Y_ASCII 89 //sárga ásvány
#define G_ASCII 71 //zöld ásvány
#define dotASCII 46

//broadcast FLAGs
int ercmezo_van; //az előző lépésnél érc mező volt, tehát most érc mezőn áll
int ercDarab = 0; //hány darabot bányászott ki; majd meg is fogjuk különböztetni a típusokat
int akku = 100; 

float nap = 6.5; //napfelkelte; nap 6:30 --> 22:30 
float ejszaka = 0; //éjszaka; 0-ról indulunk; lehet, hogy össze kéne vonni a kettőt binárisan we'll see

struct MezoAdatai {
    int mozgas;
    int ercmezo;
};

char radius[NEGYZET][NEGYZET]; //

void countRadius(char array[MAX_X][MAX_Y], int ROVER_POS[]) { //minden körben változik
    int rovX = ROVER_POS[0];
    int rovY = ROVER_POS[1];

    printf("DEBUG: négyzet közepe %d -- rover mostani poziciója: %d:%d", radius[RADIUS+1][RADIUS+1], ROVER_POS[0], ROVER_POS[1]);
    for (int dx = -RADIUS; dx <= RADIUS; dx++) {
        for (int dy = -RADIUS; dy <= RADIUS; dy++) {
            radius[dx + RADIUS][dy + RADIUS] =
                array[rovX + dx][rovY + dy];
        }
    }

    radius[RADIUS][RADIUS] = 'S';

    // for (int x = 0; x < NEGYZET; x++) {
    //     for (int y = 0; y < NEGYZET; y++) {
    //         printf("%c", radius[x][y]);
    //     }
    //     printf("\n");
    // }
}

void napJaras() {

    if (nap > 0) { //játék van és nap van
        printf("INFO: Nap van! %.1f óra maradt\n", nap);
        nap -= 0.5; //kivonjuk egy félórát

        if (nap == 0) { //ha kivonás után 0 lett, akkor a következő körben éjszaka lesz
            ejszaka = 8;
        }
    } else if (ejszaka > 0) {
        printf("INFO: Éjszaka van! %.1f óra maradt\n", ejszaka);
        ejszaka -= 0.5;
        if (ejszaka == 0) {
            nap = 16;
        }
    }


}

void energiakoltseg(int mozgas, int sebesseg, int banyaszott) { //mozgott-e, ha igen mi a sebessége, ha nem akkor bányászott-e; MAJD: figyelembe vesszük a napot és az éjszakát
    
    if (mozgas) {
        const int k = 2;
        akku -= (k * (sebesseg * sebesseg)); //lehetne math.h headerrel is megoldani
    }

    else if (banyaszott) {
        akku -= 2;
    }

    else {
        akku -= 1;
    }

    if (nap > 0) {
        akku += 10;
    }
}

struct MezoAdatai mezoEllenorzo(char array[MAX_X][MAX_Y], int ROVER_POS[], int lepes, int sebesseg) {

    //0,1,2,3,4,5,6,7,8
    //fel, le, jobbra, balra, felésjobb; felésbal; leésjobb; leésbal (most max 7 indexű)
    struct MezoAdatai mezo;

    int8_t mozgasXtengely[] = {-1, 1, 0, 0, -1, -1, 1, 1};
    int8_t mozgasYtengely[] = {0, 0, 1, -1, 1, -1, 1, -1};

    


    for (int i = 0; i < sebesseg; i++) { 

        int x = ROVER_POS[0] + (mozgasXtengely[lepes] * i);
        int y = ROVER_POS[1] + (mozgasYtengely[lepes] * i);

        if (x >= 0 && x < MAX_X && y >= 0 && y < MAX_Y) {

        switch (array[x][y]) {
            case sharpASCII:
                printf("Blokk van! Nem lépünk!\n");
                mezo.mozgas = 0;
                mezo.ercmezo = 0;
                break;

            case B_ASCII:
            case Y_ASCII:
            case G_ASCII:
                printf("Érc mező! Lépjünk!\n");
                mezo.mozgas = 1;
                mezo.ercmezo = 1;
                ROVER_POS[0] = x;
                ROVER_POS[1] = y;
                break;

            case dotASCII:
                printf("Üres mező! Lépjünk!\n");
                mezo.mozgas = 1;
                mezo.ercmezo = 0;
                ROVER_POS[0] = x;
                ROVER_POS[1] = y;
                break; 


        } 
        } else {
            printf("Out of bounds move!\n");
            mezo.mozgas = 0;
            mezo.ercmezo = 0;
    }
    }
    
     
        printf("DEBUG:mező MOZGAS: %d\n", mezo.mozgas);
        printf("DEBUG:mező ERCMEZO: %d\n", mezo.ercmezo);
        return mezo;
    }


int banyaszFunc(char array[MAX_X][MAX_Y], int ROVER_POS[]) { //és egy globális ercmezo_van = 1
    if (ercmezo_van) {
        printf("Ércmezőn áll; Tud bányászni; Bányásszunk!\n");
        ercDarab += 1;
        array[ROVER_POS[0]][ROVER_POS[1]] = '.'; //üres lesz a mező

        printf("%c", array[ROVER_POS[0]][ROVER_POS[1]]);

        return 1; //siker

    } else {
        printf("ERROR: Nem érc mezőn áll; nem bányászunk!\n");
        return 0;
    }

}

int Iranyitas(char array[MAX_X][MAX_Y], int ROVER_POS[]) {

    int banyasz = -1;
    int standyMode = -1; //tudjon állni ás skippelni a kört
    int banyaszatEredmeny = banyaszFunc(array, ROVER_POS);
    int lepes = -1;
    int sebesseg = 0; // 1 -> lassú; 2 --> normál; 3--> gyors

    napJaras();

    while (standyMode != 0 && standyMode != 1) {
        printf("Marad a helyen ebben a körben? (0 = nem, csinálok valamit, 1 = igen, standbyMode)\n");
        scanf("%d", &standyMode);
    }

    if (standyMode) {
        energiakoltseg(0, 0, 0);
        return 1;
    }

    while (banyasz != 0 && banyasz != 1) {
        printf("Bányászik vagy lép? (0 = lép, 1 = bányászik)\n");
        scanf("%d", &banyasz);
    }

    if (banyasz) {
        if (banyaszatEredmeny) {
        printf("INFO: Sikeres bánya\n");
        energiakoltseg(0, 0, 1);
        return 1;
        } 
        else {
            printf("INFO: Sikertelen bánya\n");
            return 0;
        }


    } 

    printf("Rover poziciója mozgás előtt: %d:%d\n", ROVER_POS[0], ROVER_POS[1]);


    printf("Válasszon sebességet: (1,2,3) \n");
    scanf("%d", &sebesseg);

    printf("INFO: Választott sebesség: %d\n", sebesseg);


    while(lepes < 0 || lepes > 7) {
        printf("Hova mozogjon a rover? (lásd a számokat)\n"); 
        scanf("%d", &lepes);
    }


    struct MezoAdatai mezoEllenorzoEredmenyek;
    mezoEllenorzoEredmenyek = mezoEllenorzo(array, ROVER_POS, lepes, sebesseg);

    energiakoltseg(mezoEllenorzoEredmenyek.mozgas, sebesseg, banyaszatEredmeny);

    printf("DEBUG; return mezo eredm MOZGAS: %d\n", mezoEllenorzoEredmenyek.mozgas);
    printf("DEBUG; return mezo eredm ERCMEZO: %d\n", mezoEllenorzoEredmenyek.ercmezo);

    if (!(mezoEllenorzoEredmenyek.mozgas)) {
        printf("Most nem tudott mozogni blokk miatt. Rover poziciója: %d:%d\n", ROVER_POS[0], ROVER_POS[1]);
        return 0;
    }
    if (mezoEllenorzoEredmenyek.ercmezo) {
        ercmezo_van = 1;
        printf("Követkető lépésben tud bányászni!\n");

    } else {
        ercmezo_van = 0;
    }



    return 1;





}

void JatekKezdete(char array[MAX_X][MAX_Y], int ROVER_POS[]) { //játék logika


    int adottIdoTartam = 0;

    const char *padding = 
    "################################\n"
    "################################\n"
    "################################\n";

    while (adottIdoTartam < 24) {
        printf("Adja meg egy időtartamot (óra): \n");
        scanf("%d", &adottIdoTartam);
    }

    int lepesekSzama = adottIdoTartam * 2; //egy óra = 2 félóra
    printf("Sikeresen töltöttünk be az időt!\n");

    //radius betöltése

    countRadius(array, ROVER_POS);



////////////////////////////////
////////////////////////////////
////////////////////////////////


    

    printf("Rover Pozíciója: %d:%d\n", ROVER_POS[0], ROVER_POS[1]);
    printf("Aksi játek elején: %d\n", akku);
    printf("Adott idő: %d\n", adottIdoTartam);
    printf("Elindulási idő: 6:30\n");
    printf("Fordulatok/Lépések Száma: %d\n", lepesekSzama);
    
    for (int i = 0; i < lepesekSzama; i++) {
        printf("%s", padding);
        printf("INFO: Lépés száma: %d a %d-ból/ből\n", (i+1), lepesekSzama);
        while (!(Iranyitas(array, ROVER_POS))) {};
        printf("INFO: New position of the rover: %d:%d\n", ROVER_POS[0], ROVER_POS[1]);
        printf("INFO: Kibányászott érmek (db): %d\n", ercDarab);
        printf ("A rover töltöttség szintje: %d\n", akku);
    }


}


int main() { //fájl beolvasás, fájl műveletek és funkció hívások


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
    int ROVER_POS[2]; 

    //Rover_POS; akku; nap, éj, áll, mozog, bányász, lassú, gyors, normál, elindulási idő 


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
        

       
        column += 1; //shift out array by 1 to the right
       
    }

    JatekKezdete(array, ROVER_POS);


    if (close(filedesc) < 0) {
        printf("Failed closing");
        return 1;
    } else {
        printf("Successful close\n");
        return 0;
    }

    return 0;
}