#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servoH; // Left Motor pin 9
Servo servoA; // Right Motor pin 10

// Initialize data with default values
int tipAnimal = 0;    // 0 cat, 1 dog
int greutate = 5;     
int varsta = 2;       
int oreInterval = 8;  // Hourly interval


bool meniuActiv = false;
bool editareMod = false;
int categorieCurenta = 0; // 0 type, 1 weight, 2 age, 3 hours
int valoareTemporara = 0;

// For time management, calculations are based on the last meal
unsigned long ultimaMasa = 0; 

void setup() {
  
  DDRD = 0b00110000;  // Red and green pins set as output
  PORTD = 0b00001100; // Menu and OK pins set as input pull-up

  lcd.init();
  lcd.backlight();
  lcd.noBacklight(); // LCD initially turned off
  
  servoH.attach(9);
  servoA.attach(10);
  servoH.write(0);
  servoA.write(0);

  
  PORTD |= 0b00100000; // Green LED continuously ON
}

void loop() {
  gestionareButonMeniu();
  
  if (meniuActiv) {
    ruleazaInterfataUtilizator();
  } else {
    verificareAutomataHranire();
    gestionareButonManual();
  }
}



void gestionareButonMeniu() {
  // Check if left button is active
  if (!(PIND & 0b00000100)) { 
    meniuActiv = !meniuActiv;
    if (meniuActiv) {
      lcd.backlight();
      lcd.display();
    } else {
      editareMod = false;
      lcd.noBacklight();
      lcd.clear();
    }
    delay(400);
  }
}

void ruleazaInterfataUtilizator() {
  int citirePot = analogRead(A0);

  if (!editareMod) {
    // Navigate between the 4 categories
    categorieCurenta = citirePot / 256; 
    afiseazaMeniuPrincipal();
    
    if (!(PIND & 0b00001000)) { // OK button 
      sincronizeazaTemporar();
      editareMod = true;
      delay(300);
    }
  } else {
    // Modify value before saving
    calculeazaModificare(citirePot);
    afiseazaEditare();
    
    if (!(PIND & 0b00001000)) { // Save current settings
      salveazaSetarile();
      editareMod = false;
      confirmareSetareVizuala();
      delay(300);
    }
  }
}

void verificareAutomataHranire() {  
  unsigned long intervalMs = (unsigned long)oreInterval * 3600000;

  if (millis() - ultimaMasa >= intervalMs) {
    executaProcesAlimentare();
    ultimaMasa = millis();
  }
}

void gestionareButonManual() {
  // Pin 3 triggers feeding if the menu is closed
  if (!(PIND & 0b00001000)) { 
    executaProcesAlimentare();
    ultimaMasa = millis();
    delay(400);
  }
}



void afiseazaMeniuPrincipal() {
  lcd.setCursor(0, 0);
  lcd.print("    SELECTIE    ");
  lcd.setCursor(0, 1);
  switch(categorieCurenta) {
    case 0: lcd.print("- TIP: "); lcd.print(tipAnimal == 0 ? "Pisica " : "Caine  "); break;
    case 1: lcd.print("- GREUTATE: "); lcd.print(greutate); lcd.print("kg "); break;
    case 2: lcd.print("- VARSTA: "); lcd.print(varsta); lcd.print(" ani "); break;
    case 3: lcd.print("- REPETARE: "); lcd.print(oreInterval); lcd.print("h "); break;
  }
  lcd.print("    "); // Clear residual characters
}

void afiseazaEditare() {
  lcd.setCursor(0, 0);
  lcd.print("    MODIFICA    ");
  lcd.setCursor(0, 1);
  lcd.print("SET: ");
  if (categorieCurenta == 0) {
    lcd.print(valoareTemporara == 0 ? "Pisica     " : "Caine       ");
  } else {
    lcd.print(valoareTemporara);
    if(categorieCurenta == 3) lcd.print(" ore    ");
    else lcd.print("        "); 
  }
}

void sincronizeazaTemporar() {
  if(categorieCurenta == 0) valoareTemporara = tipAnimal;
  else if(categorieCurenta == 1) valoareTemporara = greutate;
  else if(categorieCurenta == 2) valoareTemporara = varsta;
  else valoareTemporara = oreInterval;
}

void calculeazaModificare(int pot) {
  if (categorieCurenta == 0) valoareTemporara = pot / 512;
  else if (categorieCurenta == 1) valoareTemporara = pot / 20; 
  else if (categorieCurenta == 2) valoareTemporara = pot / 50; 
  else if (categorieCurenta == 3) valoareTemporara = (pot / 42) + 1; // 1-24 hours
}

void salveazaSetarile() {
  if (categorieCurenta == 0) tipAnimal = valoareTemporara;
  else if (categorieCurenta == 1) greutate = valoareTemporara;
  else if (categorieCurenta == 2) varsta = valoareTemporara;
  else oreInterval = valoareTemporara;
}

void confirmareSetareVizuala() {
  for(int i = 0; i < 3; i++) {
    PORTD &= 0b11011111; delay(150);
    PORTD |= 0b00100000; delay(150);
  }
}


void executaProcesAlimentare() {
  lcd.backlight();
  lcd.clear();
  lcd.print("ALIMENTARE...");
  
  // Calculate dispensing time based on pet profile
  long timpMancare = (long)greutate * 200 + (tipAnimal * 1000);
  if (varsta < 1) timpMancare += 500;
  long timpApa = 2000;
  
  PORTD &= 0b11011111; // Green LED turned OFF

  servoH.write(90);  // Food dispensing + Red LED turned ON
  unsigned long startH = millis();
  while (millis() - startH < timpMancare) {
    PORTD |= 0b00010000;  delay(150);
    PORTD &= 0b11101111;  delay(150);
  }
  servoH.write(0);
  
  // Red LED transition blinking
  for(int i=0; i<2; i++) {
     PORTD |= 0b00010000;
     delay(150);
     PORTD &= 0b11101111;
     delay(150);
  }

  
  servoA.write(90);// Water dispensing + Red LED blinking
  unsigned long startA = millis();
  while (millis() - startA < timpApa) {
    PORTD |= 0b00010000;  delay(150);
    PORTD &= 0b11101111;  delay(150);
  }
  servoA.write(0);

  
  PORTD |= 0b00100000; // Back to solid Green LED
  lcd.noBacklight();
  lcd.clear();
}
