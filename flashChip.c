/*
*	Secret Robot
*	Reprogram an arduino anywhere!
*	Uses an Electric Imp, some hardware, some code, and some magic.
*	All you need is a WiFi connection at your Arduino deploy location.
*
*
*	This guy uses the AVR syntax directly
*	It might be a little bit of a hack at times
*
*released under CC-A 3.0; no guarantee of any kind is made for this code
*/

const PROG-ENABLE = "\xAC\x53\x00\x00";
const CHIP-ERASE = "\xAC\x80\x00\x00";
const CHIP-ERASE-DELAY = 0.09; //need to wait for a good 90ms before doing after erasing chip
const SIGNATURE-CHECK = "\x30\x00";
const SYNC-CHECK-PROG = 0x53;

class ArduinoProgrammer {
	constructor(hex){
		attempt = 0; //the programming attempt we are currently on
		//configure hardware for our usage
		hardware.pin7.configure(DIGITAL_OUT_OD_PULLUP) //this will be our reset pin
		resetPin = hardware.pin7;
		//it should be pulled up most of the time, so we don't inadverrently pull the avr into a reset
		local spi_speed = hardware.spi.configure(MSB_FIRST|CLOCK_IDLE_LOW,300);
		server.log(format("Configured SPI to run at %s", spi_speed));
		spi = hardware.spi257;
		hex_info = hex;
	}
	function programAVR(){
	server.log("Starting AVR programming process now");
		try{
			resetAVR();
			enable_programming_request();
			verify_signature();
			delete_flash();
			write_flash();
			verify_flash();
			server.log("Successfully programmed AVR with new .hex file");
		}
		catch error{
			server.log("ERROR. Programming failed due to "+error);
			attempt++;
			if (attempt<3){
				programAVR(); //attempt to program again, but only if this is our second or less attempt
			}
		}
	}
	
	function resetAVR(){
		resetPin.write(0); //pull pin7 low to reset the system
		imp.sleep(.25); //and wait .25s to ensure the AVR resets
		resetPin.write(1); //
		imp.sleep(.05); //wait 50ms to allow the AVR to wake back up
	}
	
	function enable_programming_request(){
		response = spi.writeread(PROG-ENABLE);
		if (response[2] != SYNC-CHECK-PROG){
			//we expect the third byte recieved to be the same as the second byte we sent; if not, we are not in sync
			throw "Error in programming enable: not in sync";
		}
	}
	
	function verify_signature(){
		//we need to see if the device signature is correct
		//we DO NOT want to program an ATTiny8 as if it was an ATMega328
		local observed-sig = blob(3);
		for (local i=0; i<3; i++){
			spi.write(SIGNATURE-CHECK + 0);
			observed-sig[i] = spi.readblob(1)//we expect a single blob response
		}
		if (target.SIG != observed-sig){
			throw "Target device signature was incorrect. Expected "+target.SIG+" but recieved "+observed-sig;
		}
		return 1;
		//everything checked out!
	}
	
	function chip_erase(){
		//erase the flash programming
		//*might* also delete the EEPROM programming, but that depends on fuse bits
		//you should make sure before overwritting EEPROM
		//default Arduino fuse doesn't overwrite EEPROM
		spi.write(CHIP-ERASE);
		//send the erase command
		imp.sleep(CHIP-ERASE-DELAY);
		//wait for the chip to erase before doing anything else
	}
	function write_flash(){
		
		//esentially, we move from page address to page address until we run out of .hex to write
		//then we finish up
		//note that due to memory constraints on the Imp, we don't load the entire .hex at the same time
		//so we verify the bit of the .hex we have, then grab some more
		
		for (int i=0; i<=hex_info.size; i+target.PAGE_SIZE){
			//let's write this page to Flash memory!
			//first, we need to get the hex code:
			agent.get("hex-page", i);
			agent.on("recieve-hex-page", function (hex_blob){
				//first send LOAD PROGRAM MEMORY PAGE instruction
				spi.write();
				//then send the data
				spi.write();
				//then issue WRITE PROGRAM MEMORY PAGE instruction
				imp.sleep(0.045); //wait 4.5ms until the flash location has been written to
		}
	}
}

prog = ArduinoProgrammer(Blob(4000));
prog.programAVR();
//tada!

/*
In reality, we typically use this in conjunction with agent code
See the following:
*/
agent.on("arduino firmware change", function (hex){
	prog = ArduinoProgrammer(hex);
	prog.programAVR();
}
//hex, ArduinoProgrammer class are garbage collected after run

//hex can use up signifigant portion of user-memory 
//Max Flash size on an ATMEGA-328P is 34Kb
//There are 40KB of user-code-RAM; 34Kb/40Kb used by Hex File AND more RAM needed for other aspects of the code
//I will look into splitting hex into chunks and downloading chunks as needed later