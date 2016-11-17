#include "RewindPlugin.h"

BAKKESMOD_PLUGIN(RewindPlugin, "Rewind Plugin", "0.2", 0)

GameWrapper* gw;
ConsoleWrapper* cons;

bool rewinderEnabled = true;
bool rewinding = false;
bool overwriting = false;
bool slowDown = false; //adjust rewind speed
bool speedUp = false;
bool roll = true; //slowly "roll" game back to full speed so player has time to adjust
bool rolling = false;
float gs = 1.0f; //gamespeed
float gs_prv = 1.0f;
float rs = 0.0f; //rolling speed
long long t = 75; //time (ms) between updates
double T = 0; //measures current time
double speed = 1;
int H = 60000/t; //1 minute history
double mdf;

class instant {
public:	
	Vector bps; //ball position
	Vector cps;
	Vector bvl;
	Vector cvl; //car velocity
	Rotator brt;
	Rotator crt;
	float bst; //boost

	instant(TutorialWrapper tw) {
		BallWrapper b = tw.GetBall();
		CarWrapper c = tw.GetGameCar();

		bps = b.GetLocation();
		cps = c.GetLocation();
		bvl = b.GetVelocity();
		cvl = c.GetVelocity();
		brt = b.GetRotation();
		crt = c.GetRotation();
		bst = c.GetBoost().GetCurrentBoostAmount();
	}

	instant() {
		bps = Vector(0, 0, 0);
		cps = Vector(0, 0, 0);
		bvl = Vector(0, 0, 0);
		cvl = Vector(0, 0, 0);
		brt = Rotator(0, 0, 0);
		crt = Rotator(0, 0, 0);
		bst = 0;
	}

	//for rewinding, interpolate between two instants
	void interpolate(instant n, instant m, double d) {
		bps = n.bps.operator+((m.bps.operator-(n.bps)).operator*(d));
		cps = n.cps.operator+((m.cps.operator-(n.cps)).operator*(d));
		bvl = n.bvl.operator+((m.bvl.operator-(n.bvl)).operator*(d));
		cvl = n.cvl.operator+((m.cvl.operator-(n.cvl)).operator*(d));
		brt = n.brt.operator+((m.brt.operator-(n.brt)).operator*(d));
		crt = n.crt.operator+((m.crt.operator-(n.crt)).operator*(d));
		bst = n.bst + ((m.bst - n.bst)*d);
	}
};

std::vector<instant> history;

instant favorite = instant();
instant overwrite = instant();
int current = 0;

long long record() {
	TutorialWrapper tw = gw->GetGameEventAsTutorial();

	if (overwriting || rewinding) {
		BallWrapper b = tw.GetBall();
		CarWrapper c = tw.GetGameCar();

		if (rewinding) {
			if (current == 0) { //if no more history
				T = 0;
				cons->executeCommand("rwp_ply");
			}
			else {
				overwrite.interpolate(history.at(current - 1), history.at(current), modf(T, &mdf));

				if (slowDown) {
					if (speed == 10) speed = 6;
					else if (speed == 6) speed = 3;
					else if (speed == 3) speed = 2;
					else if (speed == 2) speed = 1;
					else if (speed == 1) speed = 0.5;
					else if (speed == 0.5) speed = 0.25;
					else if (speed == 0.25) speed = 0.2;
					else if (speed == 0.2) speed = 0.1;
					else if (speed == 0.1) speed = 0;
					else if (speed == 0);
					else speed = 1;
				}
				if (speedUp) {
					if (speed == 10);
					else if (speed == 6) speed = 10;
					else if (speed == 3) speed = 6;
					else if (speed == 2) speed = 3;
					else if (speed == 1) speed = 2;
					else if (speed == 0.5) speed = 1;
					else if (speed == 0.25) speed = 0.5;
					else if (speed == 0.2) speed = 0.25;
					else if (speed == 0.1) speed = 0.2;
					else if (speed == 0) speed = 0.1;
					else speed = 1;
				}

				T -= speed;
				if (T <= 0) {
					T = 0;
				}

				while (current > ceil(T)) {
					current--;
					history.pop_back();
				}
			}
		}

		b.SetLocation(overwrite.bps);
		c.SetLocation(overwrite.cps);
		b.SetVelocity(overwrite.bvl);
		c.SetVelocity(overwrite.cvl);
		b.SetRotation(overwrite.brt);
		c.SetRotation(overwrite.crt);
		c.GetBoost().SetBoostAmount(overwrite.bst);
		overwriting = false;
	}
	else {
		T = ceil(T);
		if (current == H) {
			history.erase(history.begin());
		}
		else {
			current++;
			T++;
		}
		history.push_back(instant(tw));

		if (roll && rolling) { //gradually accelerate game speed back to what it was before rewinding/overwriting
			float rt = cons->getCvarFloat("rwp_roll", 1000.0f);

			gs += t*(rs + (t*(gs_prv - 0.01) / pow(rt, 2)));
			rs += t*(gs_prv - 0.01) / pow(rt, 2);
			if (gs >= gs_prv) {
				gs = gs_prv;
				rs = 0;
				rolling = false;
			}
			cons->executeCommand("gamespeed " + std::to_string(gs));
		}
	}

	slowDown = false;
	speedUp = false;
	return t;
}

void go() {
	if (!gw->IsInTutorial() || !rewinderEnabled)
		return;
	gw->SetTimeout([](GameWrapper* gameWrapper) {
		go();
	}, record());
}

void rewindPlugin_onCommand(std::vector<std::string> params) {
	string command = params.at(0);
	if (!gw->IsInTutorial()) {
		return;
	}

	if (command.compare("rwp_on") == 0) { //this could be improved
		cons->executeCommand("bind XboxTypeS_DPad_Left \"rwp_rwd\"");
		cons->executeCommand("bind XboxTypeS_DPad_Right \"rwp_ply\"");
		cons->executeCommand("bind XboxTypeS_DPad_Down \"rwp_add\"");
		cons->executeCommand("bind XboxTypeS_DPad_Up \"rwp_fav\"");
		cons->executeCommand("bind XboxTypeS_LeftTrigger \"rwp_slw\"");
		cons->executeCommand("bind XboxTypeS_RightTrigger \"rwp_fst\"");
		rewinderEnabled = true;
		go();
	}
	else if (command.compare("rwp_off") == 0) {
		rewinderEnabled = false;
	}
	else if (command.compare("rwp_rwd") == 0) {
		if (roll) {
			//if (!rolling) gs_prv = cons->getCvarFloat("gamespeed", 1.0f); //BUG: doesn't retrieve gamespeed
			//if (!rolling) gs_prv = gw->GetGameEventAsTutorial().getGameSpeed();
			gs_prv = 1.0f; //temporary bandaid
			cons->executeCommand("gamespeed 0.01");
			gs = 0.01f;
		}
		rewinding = true;
	}
	else if (command.compare("rwp_ply") == 0) {
		if(roll) rolling = true;
		rewinding = false;
	}
	else if (command.compare("rwp_add") == 0) {
		if (current > 0) {
			if (rewinding) {
				favorite.interpolate(history.at(current - 1), history.at(current), modf(T, &mdf));
			}
			else favorite = history.at(current);
		}
	}
	else if (command.compare("rwp_fav") == 0) {
		overwriting = true;
		overwrite = favorite;
		if (roll) {
			//if (!rolling) gs_prv = cons->getCvarFloat("gamespeed", 1.0f); //BUG: doesn't retrieve gamespeed
			//if (!rolling) gs_prv = gw->GetGameEventAsTutorial().getGameSpeed();
			gs_prv = 1.0f; //temporary bandaid
			cons->executeCommand("gamespeed 0.01");
			gs = 0.01f;
			rolling = true;
		}
	}
	else if (command.compare("rwp_slw") == 0) {
		if (rewinding) slowDown = true;
	}
	else if (command.compare("rwp_fst") == 0) {
		if (rewinding) speedUp = true;
	}
	else if (command.compare("rwp_rollon") == 0) {
		roll = true;
	}
	else if (command.compare("rwp_rolloff") == 0) {
		roll = false;
	}
}

void RewindPlugin::onLoad()
{
	gw = gameWrapper;
	cons = console;
	
	history.push_back(instant(gw->GetGameEventAsTutorial())); 

	cons->registerNotifier("rwp_on", rewindPlugin_onCommand);
	cons->registerNotifier("rwp_off", rewindPlugin_onCommand);
	cons->registerNotifier("rwp_rwd", rewindPlugin_onCommand);
	cons->registerNotifier("rwp_ply", rewindPlugin_onCommand);
	cons->registerNotifier("rwp_add", rewindPlugin_onCommand);
	cons->registerNotifier("rwp_fav", rewindPlugin_onCommand);
	cons->registerNotifier("rwp_slw", rewindPlugin_onCommand);
	cons->registerNotifier("rwp_fst", rewindPlugin_onCommand);
	cons->registerNotifier("rwp_rollon", rewindPlugin_onCommand); //bad at naming things
	cons->registerNotifier("rwp_rolloff", rewindPlugin_onCommand);
	console->registerCvar("rwp_roll", "1000.0f");
}

void RewindPlugin::onUnload()
{
}
