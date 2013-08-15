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

PROGRAM_SERIAL_SPEED <- 115200; //the baud for UNO programming
//Duemilanove 328P requires 57600, 168P based programm at 19200 baud

STK_ENTER_PROGMODE <- 0x50;
STK_CRC_EOP <- 0x20;
STK_OK <- 0x10;
CHIP_ERASE <- "\xAC\x80\x00\x00";
CHIP_ERASE_DELAY <- 0.09; //need to wait for a good 90ms before doing after erasing chip
SIGNATURE_CHECK <- "\x30\x00";
SYNC_CHECK_PROG <- 0x53;
STK_LOAD_ADDRESS <- 0x55;
STK_LEAVE_PROGMODE <- 0x51;

class ArduinoProgrammer {
	constructor(target, hex_info){
		attempt = 0; //the programming attempt we are currently on
		//configure hardware for our usage
		hardware.pin7.configure(DIGITAL_OUT_OD_PULLUP) //this will be our reset pin
		resetPin = hardware.pin7;
		serial = hardware.uart12.configure(PROGRAM_SERIAL_SPEED, 8, PARITY_NONE, 1, NO_CTSRTS);
		hex_info = hex_info;
		target = target;
	}
	function programAVR(){
	server.log("Starting AVR programming process now");
		try{
			resetAVR();
			enable_programming();
			verify_signature();
			program();
			server.log("Successfully programmed AVR with new .hex file");
		}
		catch (error) {
			server.log("ERROR. Programming failed due to "+error);
			attempt++;
			if (attempt<3){
				programAVR(); //attempt to program again, but only if this is our second or less attempt
			}
		}
	}
	
	function resetAVR(){
		resetPin.write(0); //pull pin7 low to reset the system
		imp.sleep(0.25); //and wait .25s to ensure the AVR resets
		resetPin.write(1); //
		imp.sleep(0.05); //wait 50ms to allow the AVR to wake back up
	}
	
	function send_command(){
		serial.write(command + STK_CRC_EOP);
		serial.flush();
		local b = serial.read();
		response = blob(1);
		while( b!= STK_OK){
			if(b >= 0){
				response.writen(b);
			}
			local b = serial.read();
		}
		return response;
	}
	
	function enable_programming(){
		response = send_command(PROG-ENABLE);
		if (response[2] != STK_OK){
			//we expect the third byte recieved to be the same as the second byte we sent; if not, we are not in sync
			throw "Error in programming enable: not in sync";
		}
	}
	
	function exit_programming(){
		response = send_command(STK_LEAVE_PROGMODE);
		if (response[2] != STK_OK){
			//we expect the third byte recieved to be the same as the second byte we sent; if not, we are not in sync
			throw "Error in programming enable: not in sync";
		}
	}
	
	function program(){
		extractAddressAndData(hex_info);
	}
	
	function verify_signature(){
		//we need to see if the device signature is correct
		//we DO NOT want to program an ATTiny8 as if it was an ATMega328
		observed_sig = send_command(SIGNATURE_CHECK);
		observed_sig = observed_sig.slice(0, 3);
		if (target.SIG != observed_sig){
			throw "Target device signature was incorrect. Expected "+target.SIG+" but recieved "+observed_sig;
		}
		return 1;
		//everything checked out!
	}
	
}

//hex, ArduinoProgrammer class are garbage collected after run

//hex can use up signifigant portion of user-memory
//Max Flash size on an ATMEGA-328P is 34Kb
//There are 40KB of user-code-RAM; 34Kb/40Kb used by Hex File AND more RAM needed for other aspects of the code
//I will look into splitting hex into chunks and downloading chunks as needed later
agent.on("avr firmware change", function (target, hex_info){
	local prog = ArduinoProgrammer(target, hex_info);
	prog.programAVR();
	agent.send("avr reflash success");
	//because this is local, the prog constructor should be garbage collected after this function is ran...
});

//start hex stuff:
//This file is for understanding .hex files
//It has two main functions
//The first function breaks up the .hex by line breaks
//The second analyzes each line, and turns the .hex into a binary blob

//http://www.sbprojects.com/knowledge/fileformats/intelhex.php

function splitHex(hexString){
	//split each instruction in the hex File by newline instructions (0x0D0A)
	local lines = String.split(hexString, "\r\n");
	return lines;
}

function extractAddressAndData(hexString){
	local lines = splitHex(hexString);
	for (local i=0; i<lines.len(); i++){
		local result = understandHex(lines[i]);
	}
}

function convertHexToInteger(hex){
	//expects a string representing the hexidecimal of a single byte
	if (! len(hex) == 2){
		raise ("Error converting hexadecimal: length incorrect. Expected 2 chars to represent a single byte");
	}
	
	// see http://devwiki.electricimp.com/doku.php?id=webcolor&s[]=hex
	//I basically took the above, and add extra checks
	
	local result = 0;
	local shift = hex.len() * 4;
	for (local d=0; d<hex.len(); d++){
		local digit;
	 
		// convert ascii such that it is case insensitive
		if(hex[d] >= 0x61 && hex[d] <= 0x66){ //if the hex digit is greater than or equal to 'a', but less than or greater than 'f'
			digit = hex[d] - 0x57;
		}
		else if(hex[d] >= 0x41 && hex[d] <= 0x41){//if the hex digit is less than or equal to 'A', but less than or greater than 'F'
			 digit = hex[d] - 0x37; //shift value downwards by 55 (decimal), such that 'A'  = 10 (decimal), and 'F' = 16
		}
		else if (hex[d] >= 0x30 && hex[d] <= 0x39){ //if the digit is greater or equal to 0, but less than or equal to 9
			 digit = hex[d] - 0x30; //move the value downwards by 48 (decimal), such that 0 (ascii) represents 0 (integer), and 9 (ascii) represents 9 (integer)
		}
		else{
			raise (format("Error in hexadecimal conversion: value outside of expected ranges (0-9, A-F, a-f). Value was %x", hex[d]));
		}

		// Accumulate digit
		shift -= 4;
		result += digit << shift;
	}
	return result;
}

function integerToBlob(integer){
	local blob_result = blob(1);
	blob_result.writen(integer, 'b'); //write the integer result as an unsigned 8-bit integer (basically, a single byte)
	return blob_result;
}

//needs hex to be split by line...
function understandHex(hexLine){
	//Recall that we come close to the Max RAM on the Imp when programming, so saving the table space is worth it.
	if (!hexLine[0] == ':'){
		raise ("Error: hex format not correct, must contain : on start of each line");
	}
	local byteCount = convertHexToInteger(hexLine.splice(1,2) );
	local address = blob(2);
	address.writen(convertHexToInteger(hexLine.splice(3,4) ), 'b');
	address.writen(convertHexToInteger(hexLine.splice(5,6) ), 'b');
	local recordType = hexLine[8];
	local dataPayload = blob(byteCount);
	for (local i =0; i<byteCount; i++){
		local datum = convertHexToInteger(hexLine.splice(8 + i, 9 + i) );
		dataPayload.writen(datum, 'b');
	}
	//local checksum = convertHexToInteger(hexLine[byteCount + 10: byteCount + 11]);
	//I am not checking the checksum, HTTPS checksum already ensures integrity
	//if the checksum fails, we have bigger problems, so let's just assume it would have never failed
	if (recordType = '0'){
		//record is data, program chip...
		send_command(STK_LOAD_ADDRESS + address);
		//#55#00#00#20 STK_LOAD_ADDRESS, 0×0000(address location)
		//now write the data to the address...
		send_command(STK_PROGRAM_PAGE + byteCount + 'F' +  dataPayload);
		//STK_PROGRAM_PAGE, 0×0080 (page size), ‘F’(flash memory), data bytes…,SYNC_CRC_EOP
		imp.sleep(0.045); //wait 4.5ms until the flash location has been written to
	}
	else if (recordType = '1'){
		//this is the last record. It's empty, should have no data, checksum should be 'FF'
		//assert byteCount is 00
		if (byteCount == 00){
			exitProgramMode();
		}
		else{
			raise ("Error: final record contains more than 0 bytes. Abort.");
		}
	}
	else{
		raise ("Error: in hex format, Extended Address modes NOT accepted. Record type must be 0x00 or 0x01");
	}
	return results;
}