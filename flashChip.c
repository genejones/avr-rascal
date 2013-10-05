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

//define device constants

//taken from 28.3 and 28.5 in the Atmel AVR doc
//signature is what we expect the value to be
//flash size is how many words are present
//each word is 2 bytes long, so a Flash Size of 1600 is 32 Kb
targets <- {
"ATMEGA328P" : {"SIG":"\x1e\x95\x0f", "FLASH_SIZE":16384},
"ATMEGA328" : {"SIG":"\x1e\x95\x14", "FLASH_SIZE":16384},
"ATMEGA168A" : {"SIG":"\x1e\x94\x06", "FLASH_SIZE":8192},
}

imp.configure("8-bit-internet", [], []);
server.show(format("Signal: %d dBm", imp.rssi()));

PROGRAM_SERIAL_SPEED <- 115200; //the baud for UNO programming
//Duemilanove 328P requires 57600, 168P based programm at 19200 baud

STK_ENTER_PROGMODE <- 0x50;
STK_GET_SYNCH <- 0x30;
STK_CRC_EOP <- 0x20;
STK_OK <- 0x10;
CHIP_ERASE <- "\xAC\x80\x00\x00";
CHIP_ERASE_DELAY <- 0.09; //need to wait for a good 90ms before doing after erasing chip
SIGNATURE_CHECK <- "\x30\x00";
SYNC_CHECK_PROG <- 0x53;
STK_LOAD_ADDRESS <- 0x55;
STK_LEAVE_PROGMODE <- 0x51;

MAX_CHIP_RESPONSE_DELAY <- 0.1; //if the chip doesn't repsond within 100ms, assume the process failed.

agent.on("programModeBegin", function(t){
	server.log("got told to do a program by the agent");
	prog <- ArduinoProgrammer();
	//make sure we make it a permanent class to avoid garbage collection until everything is better...
	prog.programAVR();
	//deltete prog here, such that it is garage collected...
});

class ArduinoProgrammer {
	
		resetPin = hardware.pin2;
		statusLEDPin = hardware.pin8;
		progLEDPin = hardware.pin9;
		
		attempt = 0; //the programming attempt we are currently on

	constructor(){
		//configure hardware for our usage
		hardware.pin2.configure(DIGITAL_OUT_OD_PULLUP) //this will be our reset pin
		hardware.pin8.configure(DIGITAL_OUT) //this will be our LED pin
		hardware.pin9.configure(DIGITAL_OUT) //this will be our LED pin
		
		hardware.uart57.configure(PROGRAM_SERIAL_SPEED, 8, PARITY_NONE, 1, NO_CTSRTS); //setup the serial lines...
		
		//blank out the LEDs to start with...
		progLEDPin.write(0);
		statusLEDPin.write(0);
		
		agent.on("next_chunk", program.bindenv(this));
	}
	function programAVR(){
		server.log("Starting AVR programming process now");
		getSynch();
		enable_programming();
		//verify_signature();
		requestFirstChunk();
		server.log("Successfully programmed AVR with new .hex file");
	}
	
	function requestFirstChunk(){
		agent.send("next_chunk", "please");
	}
	
	function resetAVR(){
		server.log("ordering a reset!");
		progLEDPin.write(1); //let the user know a program is in progress
		resetPin.write(0); //pull pin7 low to reset the system
		imp.sleep(0.25); //and wait .25s to ensure the AVR resets
		resetPin.write(1); //and pull the AVR back up...
		imp.sleep(0.05); //wait 50ms to allow the AVR to wake back up
	}
	
	function getSynch(){
		resetAVR();
		server.log("set sync...");
		local getSync = [STK_GET_SYNCH];
		send_command_ignore_response(getSync);
		send_command(getSync);
		server.log("reset completed successfully");
	}
	
	
	function enable_programming(){
		response = send_command([STK_ENTER_PROGMODE]);
		if (response[2] != STK_OK){
			//we expect the third byte recieved to be the same as the second byte we sent; if not, we are not in sync
			throw "Error in programming enable: not in sync";
		}
		server.log("arduino ready for programming!");
	}
	
	function exit_programming(){
		response = send_command([STK_LEAVE_PROGMODE]);
		if (response[2] != STK_OK){
			//we expect the third byte recieved to be the same as the second byte we sent; if not, we are not in sync
			throw "Error in programming enable: not in sync";
		}
		progLEDPin.write(0); //let the user know a program is finished
		agent.send("avr reflash success", "great success!");
	}
	
	function program(chunk){
		statusLEDPin.write(1);
		local finished = 0;
		for (local i=0; i<chunk.len(); i++){
			local progResult = progLine(chunk[i]);
			if(progResult == "exitProgMode"){
				finished = 1;
				break;
			}
		}
		statusLEDPin.write(0);
		if (!finished){
			agent.send("next_chunk", "please");
		}
		else{
			exit_programming();
		}
	}
	
	function send_command_ignore_response(command_array){
		foreach (command in command_array){
			hardware.uart57.write(command);
		}
		hardware.uart57.write(STK_CRC_EOP);
		hardware.uart57.flush();
	}
	
	function send_command(command_array){
		//expects an array
		foreach (command in command_array){
			hardware.uart57.write(command);
		}
		hardware.uart57.write(STK_CRC_EOP);
		hardware.uart57.flush();
		local b = hardware.uart57.read();
		local response = [];
		//needs some kind of timeout function here...
		while( b!= STK_OK){
			if(b >= 0){
				response.append(b);
			}
			local b = hardware.uart57.read();
		}
		return response;
	}
	
	
	function progLine(hexLine){
		//program each individual line of the code with this!
		if (hexLine.recordType = '0'){
			//record is data, program chip...
			send_command([STK_LOAD_ADDRESS, hexLine.address]);
			//#55#00#00#20 STK_LOAD_ADDRESS, 0װ000(address location)
			//now write the data to the address...
			send_command([STK_PROGRAM_PAGE, byteCount,  'F',  hexLine.dataPayload]);
			//STK_PROGRAM_PAGE, 0װ080 (page size), ҆Ҩflash memory), data bytes SYNC_CRC_EOP
			imp.sleep(0.045); //wait 4.5ms until the flash location has been written to
			return 1;
		}
		else if (hexLine.recordType = '1'){
			//finalize programming...
			return "exitProgMode";
		}
	}
}