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
	for (line in lines){
		local result = understandHex(line);
	}
}

function convertHexToInteger(hex){
	//expects a string representing the hexidecimal of a single byte
	if (! len(hex) = 2){
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
	local byteCount = convertHexToInteger(hexLine[1:2]);
	local address = blob(2);
	address.writen(convertHexToInteger(hexLine[3:4]), 'b');
	address.writen(convertHexToInteger(hexLine[5:6]), 'b');
	local recordType = hexLine[8];
	local dataPayload = blob(byteCount);
	for (local i =0; i<byteCount; i++){
		local datum = convertHexToInteger(hexLine[8 + i: 9 + i]);
		dataPayload.writen(datum, 'b');
	}
	//local checksum = convertHexToInteger(hexLine[byteCount + 10: byteCount + 11]);
	//I am not checking the checksum, HTTPS checksum already ensures integrity
	//if the checksum fails, we have bigger problems, so let's just assume it would have never failed
	if (recordType = '0'){
		//record is data, program chip...
		spi.write(STK_LOAD_ADDRESS + address + SYNC_CRC_EOP);
		//#55#00#00#20 STK_LOAD_ADDRESS, 0×0000(address location), SYNC_CRC_EOP
		//now write the data to the address...
		spi.write(STK_PROGRAM_PAGE + byteCount + 'F' +  dataPayload + SYNC_CRC_EOP);
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