#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <math.h>

///Animal and related classes

class Animal {
protected:
    double weight;
    double age;
public:
    Animal(double Weight, double Age) : weight(Weight), age(Age) {}
    virtual ~Animal() {}
    
    virtual unsigned long calculate_meal_time() = 0;
    virtual unsigned long calculate_water_time() = 0;
    
    void set_weight(double Weight) { weight = Weight; }
    void set_age(double Age) { age = Age; }
};

class Dog : public Animal {
public:
    Dog(double Weight, double Age) : Animal(Weight, Age) {}
    
    unsigned long calculate_meal_time() override {
        double rer = 70.0 * pow(weight, 0.75); 
        double factor = (age < 1.0) ? 2.5 : 1.6; 
        double kcal_per_day = rer * factor;
        double grams_per_day = kcal_per_day / 3.5;
        double grams_per_meal = grams_per_day / 3.0; 
        unsigned long servo_time_ms = (grams_per_meal / 4.0) * 1000; 
        return servo_time_ms;
    }
    
    unsigned long calculate_water_time() override {
        double ml_per_day = weight * 55.0; 
        double ml_per_session = ml_per_day / 3.0; 
        unsigned long servo_water_ms = (ml_per_session / 10.0) * 1000; 
        return servo_water_ms;
    }
};

class Cat : public Animal {
public:
    Cat(double Weight, double Age) : Animal(Weight, Age) {}
    
    unsigned long calculate_meal_time() override {
        double rer = 70.0 * pow(weight, 0.75); 
        double factor = (age < 1.0) ? 2.5 : 1.2; 
        double kcal_per_day = rer * factor;
        double grams_per_day = kcal_per_day / 4.0;
        double grams_per_meal = grams_per_day / 3.0; 
        unsigned long servo_time_ms = (grams_per_meal / 4.0) * 1000; 
        return servo_time_ms;
    }
    
    unsigned long calculate_water_time() override {
        double ml_per_day = weight * 45.0; 
        double ml_per_session = ml_per_day / 3.0; 
        unsigned long servo_water_ms = (ml_per_session / 10.0) * 1000; 
        return servo_water_ms;
    }
};

///menu

class Menu {
public:
    virtual ~Menu() {}
    virtual void draw() = 0;
    virtual void update_value(int pot_value) = 0;
    virtual void save() = 0;
};

///System implementation

// System variables
struct SystemConfig {
    int animalType = 0;     // 0 = cat, 1 = dog
    int petWeight = 5;      
    int petAge = 2;        
    int hoursInterval = 8;   
} config;

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servoH; // food pin 9
Servo servoA; // water pin 10

class HardwareManager {
public:
    static void init() {
        DDRD = 0b00110000;  // setting the red and green leds as output
        PORTD = 0b00001100; // menu and ok buton setted as input pull-up
        greenLed(true);
        redLed(false);
    }
    static bool isMenuPressed() { return !(PIND & 0b00000100); }
    static bool isOkPressed()   { return !(PIND & 0b00001000); }
    static void greenLed(bool state) { 
        if(state) PORTD |= 0b00100000; 
        else PORTD &= 0b11011111; 
    }
    static void redLed(bool state) {
        if(state) PORTD |= 0b00010000; 
        else PORTD &= 0b11101111;
    }
    static void blinkConfirm() {
        for(int i = 0; i < 3; i++) {
            greenLed(false); delay(150);
            greenLed(true);  delay(150);
        }
    }
    static void blinkRed() {
        redLed(true);  delay(150);
        redLed(false); delay(150);
    }
};

class PetDispenser {
private:
    Animal* currentPet;

    void updatePetLogic() {
        if (currentPet != nullptr) {
            delete currentPet;
        }
        if (config.animalType == 0) {
            currentPet = new Cat((double)config.petWeight, (double)config.petAge);
        } else {
            currentPet = new Dog((double)config.petWeight, (double)config.petAge);
        }
    }

public:
    PetDispenser() : currentPet(nullptr) {
        updatePetLogic();
    }

    ~PetDispenser() {
        if (currentPet != nullptr) delete currentPet;
    }

    void refreshSettings() {
        updatePetLogic();
    }

    void feed() {
        lcd.backlight();
        lcd.clear();
        lcd.print("ALIMENTARE...");
        HardwareManager::greenLed(false);

        unsigned long foodTime = currentPet->calculate_meal_time();
        unsigned long waterTime = currentPet->calculate_water_time();

        // Dispense Food
        servoH.write(90);
        unsigned long start = millis();
        while (millis() - start < foodTime) HardwareManager::blinkRed();
        servoH.write(0);

        // Transition
        HardwareManager::blinkRed(); HardwareManager::blinkRed();

        // Dispense Water
        servoA.write(90);
        start = millis();
        while (millis() - start < waterTime) HardwareManager::blinkRed();
        servoA.write(0);

        HardwareManager::greenLed(true);
        lcd.noBacklight();
        lcd.clear();
    }
};

class SystemMenu : public Menu {
private:
    bool menuActive = false;
    bool editMode = false;
    int currentCategory = 0;
    int tempValue = 0;
    PetDispenser* dispenser;
    bool forceRedraw = true;

    void syncTemporary() {
        if(currentCategory == 0) tempValue = config.animalType;
        else if(currentCategory == 1) tempValue = config.petWeight;
        else if(currentCategory == 2) tempValue = config.petAge;
        else tempValue = config.hoursInterval;
    }

public:
    SystemMenu(PetDispenser* disp) : dispenser(disp) {}

    bool isActive() const { return menuActive; }
    
    void toggle() {
        menuActive = !menuActive;
        if (menuActive) {
            lcd.backlight();
            lcd.display();
            forceRedraw = true; 
        } else {
            editMode = false;
            lcd.noBacklight();
            lcd.clear();
        }
    }

    void update_value(int pot_value) override {
        if (!editMode) {
            int newCategory = pot_value / 256; 
            if (newCategory > 3) newCategory = 3; 
            
            if (newCategory != currentCategory) {
                currentCategory = newCategory;
                forceRedraw = true;
            }
            
            if (HardwareManager::isOkPressed()) {
                syncTemporary();
                editMode = true;
                forceRedraw = true; 
                delay(300);
            }
        } else {
            int newValue = tempValue;
            if (currentCategory == 0) newValue = pot_value / 512;
            else if (currentCategory == 1) newValue = pot_value / 20; 
            else if (currentCategory == 2) newValue = pot_value / 50; 
            else if (currentCategory == 3) newValue = (pot_value / 42) + 1;

            if (newValue != tempValue) {
                tempValue = newValue;
                forceRedraw = true;
            }

            if (HardwareManager::isOkPressed()) {
                save();
                editMode = false;
                forceRedraw = true; 
                HardwareManager::blinkConfirm();
                delay(300);
            }
        }
    }

    void save() override {
        if (currentCategory == 0) config.animalType = tempValue;
        else if (currentCategory == 1) config.petWeight = tempValue;
        else if (currentCategory == 2) config.petAge = tempValue;
        else config.hoursInterval = tempValue;

        dispenser->refreshSettings(); 
    }

    void draw() override {
        if (!forceRedraw) return; 
        forceRedraw = false;

        if (!editMode) {
            lcd.setCursor(0, 0);
            lcd.print("    SELECTIE    ");
            lcd.setCursor(0, 1);
            lcd.print("                "); 
            lcd.setCursor(0, 1);
            
            switch(currentCategory) {
                case 0: lcd.print("- TIP: "); lcd.print(config.animalType == 0 ? "Pisica" : "Caine"); break;
                case 1: lcd.print("- GREUTATE: "); lcd.print(config.petWeight); lcd.print("kg"); break;
                case 2: lcd.print("- VARSTA: "); lcd.print(config.petAge); lcd.print(" ani"); break;
                case 3: lcd.print("- REPETARE: "); lcd.print(config.hoursInterval); lcd.print("h"); break;
            }
        } else {
            lcd.setCursor(0, 0);
            lcd.print("    MODIFICA    ");
            lcd.setCursor(0, 1);
            lcd.print("                "); 
            lcd.setCursor(0, 1);
            
            lcd.print("SET: ");
            switch(currentCategory) {
                case 0: lcd.print(tempValue == 0 ? "Pisica" : "Caine"); break;
                case 1: lcd.print(tempValue); lcd.print(" kg"); break;
                case 2: lcd.print(tempValue); lcd.print(" ani"); break;
                case 3: lcd.print(tempValue); lcd.print(" ore"); break;
            }
        }
    }
};

///Main
PetDispenser* mainDispenser;
SystemMenu* mainMenu;
unsigned long lastMealTime = 0; 

void setup() {
    HardwareManager::init();

    lcd.init();
    lcd.backlight();
    lcd.noBacklight();

    servoH.attach(9);
    servoA.attach(10);
    servoH.write(0);
    servoA.write(0);

    mainDispenser = new PetDispenser();
    mainMenu = new SystemMenu(mainDispenser);
}

void loop() {
    // Checking menu button
    if (HardwareManager::isMenuPressed()) {
        mainMenu->toggle();
        delay(400); // Debounce
    }

    // state based behavioral
    if (mainMenu->isActive()) {
        int potRead = analogRead(A0);
        mainMenu->update_value(potRead);
        mainMenu->draw();
    } else {
        // automatic start
        unsigned long intervalMs = (unsigned long)config.hoursInterval * 3600000;
        if (millis() - lastMealTime >= intervalMs) {
            mainDispenser->feed();
            lastMealTime = millis();
        }

        // Manual starting
        if (HardwareManager::isOkPressed()) {
            mainDispenser->feed();
            lastMealTime = millis();
            delay(400);
        }
    }
}