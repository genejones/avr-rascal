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

/*
*
* On the server (agent) the following code is used:
*
*/

//define per-user constants
const owner-name = 'generjones'; //your Github user-name
const repo-name = 'button-masher'; //your Github repo-name
const file-path = 'flash.hex'; //the path to the .hex file within the repo
const device = "ATMEGA328P"; //the device type of your AVR, choose among the devices table below. ATmega328P is typical for Arduino Uno and Diecemillia.
//can also be within a folder, e.g. 'hardware/avr/flash.hex'

//general constants. Don't modify unless you know what you are doing.
const github-URL-base = format('https://api.github.com/repos/%s/%s/', owner-name, repo-name);


//define device constants

//taken from 28.3 and 28.5 in the Atmel AVR doc
//signature is what we expect the value to be
//flash size is how many words are present
//page_zie defines how many words are in each page
//each word is 2 bytes long, so a Flash Size of 1600 is 32 Kb
devices = {
"ATMEGA328P" : {"SIG":"\x1e\x95\x0f", "FLASH_SIZE":16384, "PAGE_SIZE":64},
"ATMEGA328" : {"SIG":"\x1e\x95\x14", "FLASH_SIZE":16384, "PAGE_SIZE":64},
"ATMEGA168A" : {"SIG":"\x1e\x94\x06", "FLASH_SIZE":8192, "PAGE_SIZE":64},
}

function router(request, res){
	try{
		switch(request.path){
			case "updateAVR":
				update_avr(request, res);
			default:
				if (request.method == "OPTIONS" && request.path == ""){
					res.header("ALLOW", "GET,PUT,DELETE,OPTIONS");
					res.send(200, "OK");
				}
				else if (request.method == "GET" && request.path == ""{
					res.send(200, "OK", "This is the endpoint for an Electric Imp running the Secret Robot AVR reflash system. Only for private usage");
				}
				res.send(400, "Bad Request");
		}
	}
	catch(e){
		res.send(500, "Server Error" + e);
	}
}

http.onrequest(router);

function github_request(request, res){
	switch (request.method){
		case "POST":
			try{
				local github = http.jsondecode(request.body);
				//Someone made a request asking me to flash some code to an AVR!
				//I'll get right to that...wait a second...
				//what if someone kept spamming me with re-write requests
				//The AVR would never get around to doing anyting useful!
				//or they could be trying to have us revert to an older commit!
				//or some other nefarious thing!
				//We will be diligent, wary, and awesome.
				//Let no unhallowed code pass through.
				
				if (github.repository.owner.name == owner-name){
					//Alright Mr. Request...who sent you?
					//Oh, I see from this note you carry that you were sent on my behalf by Github.
					//Hmm. Well, that's not enough proof. Let's see if Github backs up your story.
					local canidateCommit = github.commits[0].id;
					//Github, was the .hex file updated  on my repository recently?
					hexFileInfoResponse <- http.get(github-URL-base + 'contents/' + file-path, {'Accept':"application/vnd.github.raw"}).sendsync();
					if (hexFileInfoResponse.statuscode == 200){
						//I was able to reach out to my friend at Github. What did he say?
						hexInfo <- http.jsondecode(hexFileInfoResponse.body);
						if (hexInfo.sha == canidateCommit){
							//Good news, Mr. Request, Github says that my .hex file *was* updated and it matches your claimed commit ID
							//I guess everything checks out.
							//Holdup! How do I know that you aren't just a repeated message?
							//I know! I'll check to see if I already wrote this commit ID.
							//If I haven't, then you are totally legit, and I'll go about your business.
							if ("lastCommit" in server.permanent){
								if (server.permanent.lastCommit != canidateCommit){
									//OK, I haven't written this commit yet.
									//Sorry for the bother, Mr. Request.
									//I'm going to jot down your commit ID so I don't forget I already let you in...
									updatePermanent("lastCommit", canidateCommit);
									//I'll get right on that update for you:
									update_avr(hexInfo.content);
								}
								else{
									raise "You imposter! I already wrote that commit! Go away!";
								}
							}
							else{
								//Huh. That's embarrassing...
								//I haven't actually written any .hex files yet...
								//I guess I'll trust you, just this once.
								//But I'm going to write down your commit ID, and it's never going to happen again, OK?
								updatePermanent("lastCommit", canidateCommit);
								//All right, let's get to the .hex writing!
								update_avr(hexInfo.html_url);
							}
						}
					}
				}
				res.send(200, "Hex Updated");
			}
			catch (e){
				res.send(200, "Are you a hacker? Your JSON is all wrong: "+e);
			}
		case "OPTIONS":
			res.header("ALLOW", "POST,OPTIONS");
			res.send(200, "Allowed methods are POST. POST should be JSON formated according to <a href='https://help.github.com/articles/post-receive-hooks'>Github</a>. We check with Github to ensure a valid commit, so don't try anything funny.");
			break;
		default:
			res.send(405, "Method not implemented. See HTTP OPTIONS for valid options. Are you lost?");
			break;
	}
}

function update_avr(content){
	
	
}

function updatePermanent(name, value){
	//used so server.permanent isn't overridden with an empty table
	//server.log("adding/changing " +name + " to " + value);
	cachedPerm = server.permanent;
	cachedPerm <- {name = value};
	server.setpermanentvalues(cachedPerm);
}