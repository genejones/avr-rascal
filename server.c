/*
*	Secret Robot
*	Reprogram an arduino anywhere!
*	Uses an Electric Imp, some hardware, some code, and some magic.
*	All you need is a WiFi connection at your Arduino deploy location.
*
*
*	Warning: semi-literate programming used. I ain't Knux w/ Tex, that's for sure...
*
*	released under CC-A 3.0; no guarantee of any kind is made for this code
*/

/*
*
* This code is used server (agent) side.
* I lknow there's...a lot here
* It's all important though. Most of it only deals with the initial setup (getting the oAuth toekn from Github)
*/

//define per-user constants
const OWNER_NAME = "generjones"; //your Github user-name
const REPO_NAME = "blink"; //your Github REPO_NAME
const FILE_PATH = "./build/uno/firmware.hex"; //the path to the .hex file within the repo. By default, using the ino build-system, it should be './build/uno/firmware.hex'
const TARGET_NAME = "ATMEGA328P"; //the type of your AVR, choose among the targets table below. ATmega328P is typical for Arduino Uno and Diecemillia.
//can also be within a folder, e.g. 'hardware/avr/flash.hex'

//heres some oAuth stuff
//taken liberally from http://devwiki.electricimp.com/doku.php?id=example:howsthedaylooking
//we use oAuth with Github so we can update statuses, and also not worry about quotas and the like
server.log("agent started on "+http.agenturl());

const GITHUB_TOKEN = ""
//see https://github.com/blog/1509-personal-api-tokens on how to generate the above


const HOOK_SECRET = "You can't hack me! I'm invincible!"; //some random secret phrase for added protection from fake requests
//don't share this with anyone!


//general constants. Don't modify unless you know what you are doing.
GITHUB_URL_BASE <- "https://api.github.com/repos/" + OWNER_NAME + "/" + REPO_NAME;


//define device constants

//taken from 28.3 and 28.5 in the Atmel AVR doc
//signature is what we expect the value to be
//flash size is how many words are present
//page_zie defines how many words are in each page
//each word is 2 bytes long, so a Flash Size of 1600 is 32 Kb
targets <- {
"ATMEGA328P" : {"SIG":"\x1e\x95\x0f", "FLASH_SIZE":16384, "PAGE_SIZE":64},
"ATMEGA328" : {"SIG":"\x1e\x95\x14", "FLASH_SIZE":16384, "PAGE_SIZE":64},
"ATMEGA168A" : {"SIG":"\x1e\x94\x06", "FLASH_SIZE":8192, "PAGE_SIZE":64},
}

function router(request, res){
	try{
			switch(request.path){
				case "/updateAVR":
					github_request(request, res)
				default:
					if (request.method == "OPTIONS" && request.path == ""){
						res.header("ALLOW", "GET,PUT,DELETE,OPTIONS");
						res.send(200, "OK");
					}
					else if (request.method == "GET" && request.path == ""){
						res.send(200, "This Rascal needs setup, captain!");
					}
					res.send(404, "Resource Does Not Exist");
			}
	}
	catch(e){
		res.send(500, "Server Error: " + e);
		raise(e);
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
				
				if (github.repository.owner.name == OWNER_NAME && github.secret == HOOK_SECRET){
					//Alright Mr. Request...who sent you?
					//Oh, I see from this note you carry that you were sent on my behalf by Github.
					//And, you also carry the secret pass phrase me and Github setup a while ago...
					//Hmm. Well, that's not enough proof. Let's see if Github backs up your story.
					local canidateCommit = github.commits[0].id;
					//Github, was the .hex file updated  on my repository recently?
					hexFileInfoResponse <- http.get(GITHUB_URL_BASE + "contents/" + FILE_PATH, {"Accept":"application/vnd.github.raw", "User-Agent" : "Secret Robot/Imp Agent", "Authorization":"token "+GITHUB_TOKEN} ).sendsync();
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
									update_avr(hexInfo.bindenv(this));
								}
								else{
									raise ("You imposter! I already wrote that commit! Go away!");
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

function update_avr(info){
	//expects a table, with "content" containing the actual, raw, .hex file...
	//and "length" as well, which we use to determine the length, funnily enough
	try{
		//alright, let's get started by telling Github that we are starting the deploy process
		updateGithubStatus(info.sha, "pending", "Rascal is on the job!");
		//this provides a nice, easy wasy to see the status of our deploy
		
		local target_properties = targets[TARGET_NAME];
		//find out what our device is, and it's capabilities
		if (info.size > target_properties.size){
			//our target has too small of a flash memory
			//this should have gotten caught by the GCC process
			raise ("Flash memory smaller than flash program");
		}
		
		device.send("avr firmware change", target_properties, info); //just send the code right over to the Imp
		//it's in his hands now.
		
		device.on("avr reflash success", function (){
			//we are all done now!
			updateGithubStatus(info.sha, "success");
			//alter the github status to reflect our awesomeness
		});
	}
	catch (e){
		//something went horribly wrong during the deploy process
		//sigh
		//log it
		server.log("Error during update process: "+e);
		//and tell the whole world about our mistake on Github
		updateGithubStatus(info.sha, "failure");
		//wait, what? Too late now. Nobody goes to our Github page anyway, I'm sure it's fine...
	}
}

function updateGithubStatus(commitID, status, description){
	local headers = {"User-Agent" : "Secret Robot/Imp Agent", "Authorization":"token "+GITHUB_TOKEN};
	local payload = http.jsonencode({"state":status, "description":description});
	http.post(GITHUB_URL_BASE + "statuses/" + commitID, headers, payload).sendsync();
}

function updatePermanent(name, value){
	//used so server.permanent isn't overridden with an empty table
	//server.log("adding/changing " +name + " to " + value);
	cachedPerm = server.permanent;
	cachedPerm <- {name = value};
	server.setpermanentvalues(cachedPerm);
}

function setUpHook(){
	//sets up a post-commit hook on this agent url,
	//so that we get notified when hooks occur
	//and can update teh AVR accordingly
	local body = http.jsonencode({
		"name":"web",
		"active": true,
		"events": [
			"push"
		],
		"config": {
			"url" : http.agenturl()+"/updateAVR",
			"content_type": "json",
			"secret": HOOK_SECRET
		}
	});
	response <- http.post(GITHUB_URL_BASE + "hooks/" {"User-Agent" : "Secret Robot/Imp Agent", "Authorization":"token "+GITHUB_TOKEN}, body).sendsync();
	if (! response.statuscode == 201){
		server.log("Hook was not setup correctly");
	}
	updatePermanent("hookSetup", true);
}