#include <stdio.h>
#include <math.h>
//nap vagy nem; nappal indulunk; 6:30


//int mozgas == 0 ha áll 1 ha mozgott épp; 
//int bányászik
//ha nem mozgott ÉS bányászott, akkor az 2 egység
//akku change funkciót minden fél óránként fel kell hívni az elindulás után; rovar lépett --> változások
//ötlet: lehet először összeadjuk a különbségeket (az energiát) és majd hozzáadjuk a végén a végső energiához


int AkkuChange(int battery, int napszak, int speed, int mozgas, int banyaszott) { //a flagos értékeket megváltozatom majd bináris alakba; főleg a 4 mozgási flagot
    int osszes_valtozas = 0;

    if (napszak) { //1, nap van
        osszes_valtozas += 10; 
    }

    if (mozgas == 0) { //nem volt mozgás
        if (banyaszott) {
            osszes_valtozas -= 2;
            int uj_akku = battery + osszes_valtozas;
            return uj_akku;
        } else {
            osszes_valtozas -= 1;
            int uj_akku = battery + osszes_valtozas;
            return uj_akku;
        }
    }

    //most ezt kell exekutálni, ha van mozgas

    const int k = 2; //2 állandó
    int E_felhasznalt = (k * pow(speed, 2)); //ezen a blokkon a rover ennyi energiát használt el
    osszes_valtozas -= E_felhasznalt; //negatív érték lesz 
    //ha nap van akkor +10 energia egység
    
    printf("Összes változás az első esetben: %d\n", osszes_valtozas); //debug
    int uj_akku = battery + osszes_valtozas; //we add the osst valt, becasue it is either negative or positive
    return uj_akku;

    
    


} 

int main() {

    float startTime = 6.5;
    int battery = 100;

    int interval; //no checking for legit data, just the base

    printf("Adja meg hány óraig mozog rover");

    scanf("%d", &interval);

    printf("%d", interval); //jó

    //Eset 1: nap van, gyorsan mozgott
    int check1 = AkkuChange(battery, 1, 3, 1, 0);
    //Eset 2: északa van, nem mozgott és bányászott
    int check2 = AkkuChange(battery, 0, 0, 0, 1);

    printf("Eset 1 akkuja: %d\n", check1);
    printf("Eset 2 akkuja: %d\n", check2);




    return 0;
}