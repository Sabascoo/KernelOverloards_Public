platform = []



with open("tartomany.txt", "r") as f:

    text = f.readlines()
    

    for i in text:
        toAdd = i.strip().split(",")
        platform.append(toAdd)


row = 0
column = 0
for i in platform:
    for j in i:
        #print(f"{j} ",end="")
        if j == "S":
            S_POS = (row, column)
        column += 1
    #print("\n")
    row += 1
    column = 0

#(3,3)

class Erem:
    def __init__(self, ermekMellettem, szabadMezok):
        self.ermekMellettem = ermekMellettem
        self.szabadMezok = szabadMezok

time = 5 #hours
rounds = 5*2

def C_Circle(S_POS):
    x = S_POS[0]
    y = S_POS[1]

    known_array = {}
    toBeDiscovered = [] #here we sat the equivalent values?


    rangeNumbersSet = {x, y, x+1, y+1, x-1, y-1}
    known_array = set()  

    for i in rangeNumbersSet:
        for j in rangeNumbersSet:
            toAdd = (i, j)
            known_array.add(toAdd) 

    #brainfry

    print(toBeDiscovered)



while rounds != 0:
    
    rounds -= 0.5

C_Circle(S_POS=S_POS)