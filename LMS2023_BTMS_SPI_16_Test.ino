/*
 LMS 2023 BTMS test software for the SPI interface.
 This sketch is designed for use with the LMS2023 BTMS circuit board, but can be run on a standalone Teensy 4.1 id desired.
 In normal operation, the SPI is connected to the ADC on the HVI board. For this sketch, the HVI board is not present.
 The ADC is a Linear Technology Corporation LTC2315-12fa. It has 12-bit resolution and a 5MHz maximum smaple speed.
 The Teensy 4.1 SPI0 port is used and is connected as follows:
  Chip Select (or Slave Select), CS0*: pin 10
  SPI Clock, SCLK0: pin 13
  Micro Out-Serial In, MOSI0: pin 11
  Micro In-Serial Out, MISO0: pin 12
 SPI Clock Speed Determination
  The LTC2315-12fa has a maximum clock speed of 20MHz.
  There are 5 HVI boards and each HVI board has 2 ADC chips.
  Each ADC is connected to a 16 channel multiplexer, of which only 9 channels are used.
  Each multiplexer channel is connected to a battery temperature sensor.
  This results in a total of 90 ADC channels (temperatures) to be read. The desired smaple rate is once per second.
  Although it only requires 14 clock cycles plus CS Select and Deselect time to read the ADC, 
  the Teensy is set up for 8 bit or 16 bit reads so a 16 bit read will be used. This results in a minimum of 18 clock cycles per read.
  (Reads beyond 12 bits of the ADC result in trailing zeros which the software has to accomodate.)
  At 18 clock cycles per read, 90 ADC reads, and a 1MHz SPI clock speed, the resulting time to read all the channels is a minimum of 1.62mSec.
  Estimating CS select and deselect time, plus multiplexer address set time, plus a 100uSec delay to accomodate multiplexer settling time,
  yields approximately 12mSec to complete all the ADC reads.  
  This means a 1MHz SPI clock is sufficient and could be reduced if clock speed issues arise due to wire length.
 SPI Mode
  Both the Teensy 4.1 and the LTC2315-12fa support SPI Mode 0 operation so that has been choosen.
 
 */

/* Library: The ADC communicates using SPI, so include the SPI.h library:
 Note that the way the SPI is implemented on the Teensy, both a read and a write are performed simultaneously.
 In other words, the SPI.transfer(Byte_to_send) instruction is used to both "read" or "write" a byte to the SPI.
 The return value  "Byte_Received = SPI.Transfer();" is the data received.
 To perform a two byte read or write, use the SPI.transfer16(Two_buytes_to_send) instruction.
 There are additional SPI.transfer options, but we don't need them for this ADC.
 
 This sketch uses the default SPI pins: MOSI = pin 11, MISO = pin 12, SCK = 13.  To use the alternate pins, use the SPIsetMOSI(pin), SPIsetMISO(pin) and SPIsetSCK(pin) instructions
 */

// !!!!!!!!!!!!!!!!! -----------  Jumper pin 11 to pin 12 on Teensy 4.1  -----------!!!!!!!!!!!!!!!!

#include <SPI.h>  // include the SPI library

// ------ Name output pins --------
int HVI_Pwr = 20;
int ADDR0 = 37;
int ADDR1 = 36;
int ADDR2 = 35;
int ADDR3 = 34;
int HVIx_EN[10] = {19, 14, 40, 17, 22, 18, 15, 41, 16, 23};
int HVI1_LOWER_EN = 19;
int HVI1_UPPER_EN = 18;
int HVI2_LOWER_EN = 14;
int HVI2_UPPER_EN = 15;
int HVI3_LOWER_EN = 40;
int HVI3_UPPER_EN = 41;
int HVI4_LOWER_EN = 17;
int HVI4_UPPER_EN = 16;
int HVI5_LOWER_EN = 22;
int HVI5_UPPER_EN = 23;
int BMS_RESET = 39;
int IMD_RESET = 38;
const int CS0 = 10;

// The SPI.begin instruction takes care of assigning SCK to pin 13 as an output, 
// MOSI to pin 11 as an output, and MISO to pin 12 as an input. 
// The SPI bus CS pin is not assigned by SPI.begin so it must be done explictly

// -------------------------------------------- declare functions ------------------
void setHVI (String );  // declared for future use
void setADDR (String);  // declared for future use

//  Declare a function to do an SPI byte read or write:
unsigned int read_writeSPI(byte);

//  Declare a function to do an SPI word (16 bit) read or write:
unsigned int read_writeSPI16(word);

/* Note: there is some confusion regarding which SPI port usage. In one reference for the Arduino Due, the command SPI.transfer(10, 0x0A) would send 
   0x0Ah to the SPI port that uses pin 10 as the CS pin.  It also states you would not have to explictly command the CS pin low and then high to perform a transfer.
   Examples of Teensy 4.1 code indictaes it does not have that functionality so control of the CS pin must be done with individual instructions.     
*/
// -------------------------------------------- end declare functions ------------------


void setup() {   // ------------------------ start setup -------------------------------

  Serial.begin(38400); //Setup Serial (USB) port so we can use the Serial Monitor to send commands and view results
  Serial.println("LMS 2023 BTMS Board SPI Test");

  // Configure output pins
  pinMode(ADDR0, OUTPUT);
  pinMode(ADDR1, OUTPUT);
  pinMode(ADDR2, OUTPUT);
  pinMode(ADDR3, OUTPUT);
  pinMode(HVI_Pwr, OUTPUT);
  pinMode(HVI1_LOWER_EN, OUTPUT);
  pinMode(HVI1_UPPER_EN, OUTPUT);
  pinMode(HVI2_LOWER_EN, OUTPUT);
  pinMode(HVI2_UPPER_EN, OUTPUT);
  pinMode(HVI3_LOWER_EN, OUTPUT);
  pinMode(HVI3_UPPER_EN, OUTPUT);
  pinMode(HVI4_LOWER_EN, OUTPUT);
  pinMode(HVI4_UPPER_EN, OUTPUT);
  pinMode(HVI5_LOWER_EN, OUTPUT);
  pinMode(HVI5_UPPER_EN, OUTPUT);
  for (int j = 0; j < 10; j++) {  // To prevent output contention on the BTMS hardware, set all HVIx Enables HIGH => disabled
    digitalWrite(HVIx_EN[j], HIGH);
  }
  pinMode(BMS_RESET, OUTPUT);
  digitalWrite(BMS_RESET, LOW);  // ensure +12V is not present on BMS_RESET pin
  pinMode(IMD_RESET, OUTPUT);
  digitalWrite(IMD_RESET, LOW);  // ensure +12V is not present on IMD_RESET pin
  pinMode(CS0, OUTPUT);  // Teensy 4.1: The SPI.begin command does not configuer the CS pin so this must be done explictly

  //  Configure all unused pins as inputs with pullups to prevent floating inputs
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);
  pinMode(21, INPUT_PULLUP);
  pinMode(24, INPUT_PULLUP);
  pinMode(25, INPUT_PULLUP);
  pinMode(26, INPUT_PULLUP);
  pinMode(27, INPUT_PULLUP);
  pinMode(28, INPUT_PULLUP);
  pinMode(29, INPUT_PULLUP);
  pinMode(30, INPUT_PULLUP);
  pinMode(31, INPUT_PULLUP);
  pinMode(32, INPUT_PULLUP);
  pinMode(33, INPUT_PULLUP);

// start the SPI library function.  This configures SCK, MOSI and MISO pins:
SPI.begin();
  
}  //  -------------------------------- end setup ----------------------

//  ------------------------------------ start loop ----------------------
void loop() {  
int menuChoice1 = 1;
int menuChoice2;
int ADC_Read_Result;  // variable that is read by SPI.transfer
word TXdata = 0x1000;   // variable to write to SPI.transfer 
    Serial.println("Enter the number of SPI read/writes to perform");
    
    while (Serial.available() == 0) {} // wait for keyboard entry. Serial.available returns the number of bytes available
    menuChoice1 = Serial.parseInt(); // Read keyboard entry 1st byte
    menuChoice2 = Serial.read(); // Read keyboard entry 2nd Byte to clear the Line Feed character in the buffer
    Serial.print("Number of times to read-write: ");
    Serial.print(menuChoice1);
    Serial.print("  Buffer: ");
    Serial.println(menuChoice2);
    for (int i = 1; i <= menuChoice1; i++) {
    ADC_Read_Result = read_writeSPI16(TXdata);  // read SPI and send out TXdata on MOSI
    TXdata++;                                 // increment TXdata
    Serial.print(" Result (hex): ");
    Serial.print(ADC_Read_Result, HEX);
    Serial.print(" Count ");
    Serial.println(i);
    delay(100);   //  delay 100msec.
    }
  
  Serial.println();  // insert blank line
}         // ------------------------------- end loop ---------------------

//  ---------------------------------------- start read_writeSPI16 function ----------------
unsigned int read_writeSPI16(word dataToSend) {
word dataToReturn = 0x0000;
  // Start and Configure the SPI port (Speed (Hz), bit order, SPI Mode)
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));  // 1Mhz
  // take the chip select low to select the device:
  digitalWrite(CS0, LOW);
  delayMicroseconds(10); //  Allow time for the ADC to complete a conversion

  dataToReturn = SPI.transfer16(dataToSend);   // perform an SPI read-write
  
  delayMicroseconds(100);  // Allow time for SPI transfer to complete
  digitalWrite(CS0, HIGH);   // take the chip select high to de-select:
  // release control of the SPI port
  SPI.endTransaction();
  // return the result:
  return(dataToReturn);
}
