/*
*    Secret Robot
*    Reprogram an arduino anywhere!
*    Uses an Electric Imp, some hardware, some code, and some magic.
*    All you need is a WiFi connection at your Arduino deploy location.
*
*
*	released under CC-A 3.0; no guarantee of any kind is made for this code
*/

/*
*
* This code is used server (agent) side.
*/

const TARGET_NAME = "ATMEGA328P"; //the type of your AVR, choose among the targets table below. ATmega328P is typical for Arduino Uno and Diecemillia.

server.log("agent started on "+http.agenturl());

function update_avr(fileURL){
	//expects a table, with "content" containing the actual, raw, .hex file...
	//and "length" as well, which we use to determine the length, funnily enough
	try{
			server.log("grabbing file..");
			file <- http.get(fileURL).sendsync();
			server.log("status was " + file.statuscode);
			if (file.statuscode == 200){
				hexItUp(file.body);
			}
			device.on("avr reflash success", function (t){
				//we are all done now!
				server.log("flash success!");
				//maybe tell Twitter about our success?
			});
			device.on("next_chunk", chunkLines);
	}
	catch (e){
		//something went horribly wrong during the deploy process
		//sigh
		//log it
		server.log("Error during update process: "+e);
	}
}

//start hex stuff:
//This file is for understanding .hex files
//It has two main functions
//The first function breaks up the .hex by line breaks
//The second analyzes each line, and turns the .hex into a binary blob

//http://www.sbprojects.com/knowledge/fileformats/intelhex.php

function splitHex(hexString){
	//split each instruction in the hex File by newline instructions (0x0D0A)
	local lines = split(hexString, "\r\n");
	return lines;
}

//then split lines into chunks of about 64 lines at a time (which is 64*16 bytes, or 1024)

programPosition <- 0;
function chunkLines(t){
	server.log("chunk requested by device");
	local chunk_size = 64; //64 lines at a time works out to be 1024 data bytes at a time, plus addresses...
	if (len(programData) > programPosition + chunk_size){
		local chunkTable = [];
		for (local i=0; i < programPosition + chunk_size; i++){
			chunkTable[i] = programData[i + programPosition]
		}
		device.send("next_chunk", chunkTable);
		programPosition = programPosition + chunk_size;
		server.log(format("sent chunk, at position %s of %s" %programPosition, len(programData)));
	}
	else {
		//we don't have a full 64 lines, so give the remaining lines as our chunk
		local chunkTable = [];
		for (local i=0; i < programPosition + chunk_size; i++){
			chunkTable[i] = programData[i + programPosition]
		}
		device.send("next_chunk", chunkTable);
	}
}

function extractAddressAndData(hexString){
	local lines = splitHex(hexString);
	for (local i=0; i<lines.len(); i++){
		local result = understandHex(lines[i]);
	}
}

function convertHexToInteger(hex){
	//expects a string representing the hexidecimal of a single byte
	
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
		else if(hex[d] >= 0x41 && hex[d] <= 0x46){//if the hex digit is less than or equal to 'A', but less than or greater than 'F'
			 digit = hex[d] - 0x37; //shift value downwards by 55 (decimal), such that 'A'  = 10 (decimal), and 'F' = 16
		}
		else if (hex[d] >= 0x30 && hex[d] <= 0x39){ //if the digit is greater or equal to 0, but less than or equal to 9
			 digit = hex[d] - 0x30; //move the value downwards by 48 (decimal), such that 0 (ascii) represents 0 (integer), and 9 (ascii) represents 9 (integer)
		}
		else{
			throw (format("Error in hexadecimal conversion: value outside of expected ranges (0-9, A-F, a-f). Value was %x", hex[d]));
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

programData <- [];

function hexItUp(raw_hex_file){
	local raw_lines = splitHex(raw_hex_file);
	server.log("hexItUp");
	foreach (i,raw_line in raw_lines){
		local result = understandHex(raw_line);
		server.log(format("parsed line %04d", i));
		programData.append(result);
	}
	//instruct the device to begin downloading our data...
	device.send("programModeBegin", 1);
}

//needs hex to be split by line...
function understandHex(hexLine){
	if (!hexLine[0] == ':'){
		throw ("Error: hex format not correct, must contain : on start of each line");
	}
	local byteCount = convertHexToInteger(hexLine.slice(1,2) );
	local address = blob(2);
	address.writen(convertHexToInteger(hexLine.slice(3,4) ), 'b');
	address.writen(convertHexToInteger(hexLine.slice(5,6) ), 'b');
	local recordType = hexLine[8];
	local dataPayload = blob(byteCount);
	for (local i =0; i<byteCount; i++){
		local datum = convertHexToInteger(hexLine.slice(8 + i, 9 + i) );
		dataPayload.writen(datum, 'b');
	}
	//I am not checking the checksum, HTTPS checksum already ensures integrity
	//if the checksum fails, we have bigger problems, so let's just assume
	results <- {"recordType":recordType, "address":address, "dataPayload":dataPayload, "byteCount":byteCount};
	return results;
}

update_avr("https://raw.github.com/genejones/blink/master/.build/uno/firmware.hex");
//testing...